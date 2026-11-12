//
// SPDX-License-Identifier: Apache-2.0
//
// AVX2 closest-point queries for mesh voxelization. Each lane repeats the
// scalar OpenVDB closestPointOnTriangleToPoint arithmetic (mul then add, no
// FMA) so a non-degenerate triangle returns the same point bits. Degenerate
// triangles stay on the scalar path. The switch defaults off.

#ifndef PICOGK_MESH_VOXEL_AVX_H_
#define PICOGK_MESH_VOXEL_AVX_H_

#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

// Intrinsics stay inside the x86-64 branch. arm64 and every other target
// compile only the scalar stub, even if a build system defines the hook.
#if defined(__x86_64__) || defined(_M_X64)
// cl.exe only. clang-cl defines _MSC_VER too, and it keeps the Clang path.
#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#endif
#include <immintrin.h>
#endif

namespace PicoGK
{

inline bool bMeshVoxelToken(const char* pszBegin, const char* pszEnd, const char* pszLit)
{
    const char* psz = pszBegin;
    while (psz != pszEnd && *pszLit != '\0')
    {
        const unsigned char uc = static_cast<unsigned char>(*psz);
        const char cFolded = (uc >= 'A' && uc <= 'Z')
            ? static_cast<char>(uc - 'A' + 'a')
            : static_cast<char>(uc);
        if (cFolded != *pszLit)
            return false;
        ++psz;
        ++pszLit;
    }
    return psz == pszEnd && *pszLit == '\0';
}

inline bool bMeshVoxelIsSpace(unsigned char uc)
{
    return uc == ' ' || uc == '\t' || uc == '\n' || uc == '\r' || uc == '\v' || uc == '\f';
}

// PICOGK_MESH_AVX2 selects the AVX2 neighbor distance path in meshToVolume.
// On, trimmed, case-insensitive: 1, true, yes, on.
// Scalar, no warning: unset, empty, 0, false, no, off.
// Any other value stays on the scalar path and prints one warning to stderr.
// Read once. Only compiled into the x86-64 hook.
#if defined(__x86_64__) || defined(_M_X64)

#if defined(_MSC_VER) && !defined(__clang__)
inline bool bMeshVoxelCpuAvx2()
{
    int aInfo[4] = {0, 0, 0, 0};
    __cpuid(aInfo, 0);
    if (aInfo[0] < 7)
        return false;
    __cpuid(aInfo, 1);
    const bool bOsxsave = (aInfo[2] & (1 << 27)) != 0;
    const bool bAvx = (aInfo[2] & (1 << 28)) != 0;
    if (!bOsxsave || !bAvx)
        return false;
    // OSXSAVE is set. YMM state is usable only when XCR0 bits 1 and 2 are set.
    const unsigned long long nXcr0 = static_cast<unsigned long long>(_xgetbv(0));
    if ((nXcr0 & 0x6ull) != 0x6ull)
        return false;
    __cpuidex(aInfo, 7, 0);
    return (aInfo[1] & (1 << 5)) != 0;
}
#define PICOGK_MESH_AVX_FN
#else
inline bool bMeshVoxelCpuAvx2()
{
    return __builtin_cpu_supports("avx2");
}
#define PICOGK_MESH_AVX_FN __attribute__((target("avx2")))
#endif

inline bool bMeshVoxelAvx2()
{
    static const bool bEnabled = []()
    {
        const char* pszRaw = std::getenv("PICOGK_MESH_AVX2");
        const char* psz = pszRaw;
        if (psz == nullptr)
            return false;
        while (*psz != '\0' && bMeshVoxelIsSpace(static_cast<unsigned char>(*psz)))
            ++psz;
        const char* pszEnd = psz;
        while (*pszEnd != '\0')
            ++pszEnd;
        while (pszEnd > psz && bMeshVoxelIsSpace(static_cast<unsigned char>(pszEnd[-1])))
            --pszEnd;
        if (psz == pszEnd)
            return false;
        if (bMeshVoxelToken(psz, pszEnd, "1")
            || bMeshVoxelToken(psz, pszEnd, "true")
            || bMeshVoxelToken(psz, pszEnd, "yes")
            || bMeshVoxelToken(psz, pszEnd, "on"))
            return bMeshVoxelCpuAvx2();
        if (bMeshVoxelToken(psz, pszEnd, "0")
            || bMeshVoxelToken(psz, pszEnd, "false")
            || bMeshVoxelToken(psz, pszEnd, "no")
            || bMeshVoxelToken(psz, pszEnd, "off"))
            return false;
        static std::atomic<bool> bWarned{false};
        if (!bWarned.exchange(true))
        {
            std::fprintf(stderr,
                "PICOGK_MESH_AVX2=\"%s\" is not recognized; mesh voxelization is using the scalar path. "
                "AVX2 values: 1, true, yes, on. Scalar values: unset, empty, 0, false, no, off.\n",
                pszRaw);
        }
        return false;
    }();
    return bEnabled;
}

// Per-triangle visit set for the AVX flood. The scratch primId tree stays the
// scalar path's record, and it is also the visit set for a triangle whose box
// does not fit in the dense table. This table answers the same "already
// visited" question for one triangle, including the seed, so the distance and
// index writes do not move.
class FloodVisit
{
public:
    // True when this triangle fits in the dense table. A false return leaves
    // the visit set to the primId tree; the hash is not grown for that triangle.
    bool begin(double ax, double ay, double az,
               double bx, double by, double bz,
               double cx, double cy, double cz,
               int sx, int sy, int sz)
    {
        const double xLo = ax < bx ? (ax < cx ? ax : cx) : (bx < cx ? bx : cx);
        const double yLo = ay < by ? (ay < cy ? ay : cy) : (by < cy ? by : cy);
        const double zLo = az < bz ? (az < cz ? az : cz) : (bz < cz ? bz : cz);
        const double xHi = ax > bx ? (ax > cx ? ax : cx) : (bx > cx ? bx : cx);
        const double yHi = ay > by ? (ay > cy ? ay : cy) : (by > cy ? by : cy);
        const double zHi = az > bz ? (az > cz ? az : cz) : (bz > cz ? bz : cz);
        const int xmin = static_cast<int>(std::floor(xLo)) - 2;
        const int ymin = static_cast<int>(std::floor(yLo)) - 2;
        const int zmin = static_cast<int>(std::floor(zLo)) - 2;
        const int xmax = static_cast<int>(std::floor(xHi)) + 2;
        const int ymax = static_cast<int>(std::floor(yHi)) + 2;
        const int zmax = static_cast<int>(std::floor(zHi)) + 2;
        mX0 = xmin;
        mY0 = ymin;
        mZ0 = zmin;
        const long long nx = static_cast<long long>(xmax) - xmin + 1;
        const long long ny = static_cast<long long>(ymax) - ymin + 1;
        const long long nz = static_cast<long long>(zmax) - zmin + 1;
        const long long nCells = (nx > 0 && ny > 0 && nz > 0) ? nx * ny * nz : 0;
        mDense = nCells > 0 && nCells <= (1 << 20);
        if (!mDense)
            return false;
        hashNewTriangle();
        mNx = static_cast<int>(nx);
        mNy = static_cast<int>(ny);
        mNz = static_cast<int>(nz);
        if (mStamp.size() < static_cast<size_t>(nCells))
            mStamp.assign(static_cast<size_t>(nCells), 0u);
        if (mDenseGen == 0xffffffffu)
        {
            // The retained buffer can be larger than this triangle. A partial
            // clear would leave stale generation numbers in the tail.
            std::fill(mStamp.begin(), mStamp.end(), 0u);
            mDenseGen = 1;
        }
        else
            ++mDenseGen;
        mark(sx, sy, sz);
        return true;
    }

    // Drop a hash that grew past the retain cap. The next triangle allocates
    // again from the small initial size. Called after that triangle's flood.
    void end()
    {
        if (mHashMask + 1 > kHashRetain)
        {
            std::vector<int>().swap(mHashX);
            std::vector<int>().swap(mHashY);
            std::vector<int>().swap(mHashZ);
            std::vector<uint32_t>().swap(mHashStamp);
            mHashMask = 0;
            mHashCount = 0;
        }
    }

    // Test hook. Sets the dense generation so the next begin() takes the wrap path.
    void forceDenseGenForTest(uint32_t nGen)
    {
        mDenseGen = nGen;
    }

    // True when this voxel was not already marked for the current triangle.
    bool mark(int x, int y, int z)
    {
        if (mDense)
        {
            const int ix = x - mX0;
            const int iy = y - mY0;
            const int iz = z - mZ0;
            if (static_cast<unsigned>(ix) < static_cast<unsigned>(mNx)
                && static_cast<unsigned>(iy) < static_cast<unsigned>(mNy)
                && static_cast<unsigned>(iz) < static_cast<unsigned>(mNz))
            {
                const size_t i = static_cast<size_t>(ix)
                    + static_cast<size_t>(mNx) * (static_cast<size_t>(iy)
                    + static_cast<size_t>(mNy) * static_cast<size_t>(iz));
                if (mStamp[i] == mDenseGen)
                    return false;
                mStamp[i] = mDenseGen;
                return true;
            }
        }
        return hashMark(x, y, z);
    }

private:
    void hashNewTriangle()
    {
        if (mHashGen == 0xffffffffu)
        {
            std::fill(mHashStamp.begin(), mHashStamp.end(), 0u);
            mHashGen = 1;
        }
        else
            ++mHashGen;
        mHashCount = 0;
    }

    static uint32_t mix(int x, int y, int z)
    {
        uint32_t h = static_cast<uint32_t>(x) * 0x9E3779B1u;
        h ^= static_cast<uint32_t>(y) * 0x85EBCA77u;
        h ^= static_cast<uint32_t>(z) * 0xC2B2AE3Du;
        h ^= h >> 16;
        return h;
    }

    bool hashMark(int x, int y, int z)
    {
        if (mHashMask == 0)
            hashRehash(1024);
        else if (mHashCount * 2 > mHashMask)
            hashRehash((mHashMask + 1) * 2);
        size_t i = static_cast<size_t>(mix(x, y, z)) & mHashMask;
        for (;;)
        {
            if (mHashStamp[i] != mHashGen)
            {
                mHashStamp[i] = mHashGen;
                mHashX[i] = x;
                mHashY[i] = y;
                mHashZ[i] = z;
                ++mHashCount;
                return true;
            }
            if (mHashX[i] == x && mHashY[i] == y && mHashZ[i] == z)
                return false;
            i = (i + 1) & mHashMask;
        }
    }

    void hashRehash(size_t cap)
    {
        std::vector<int> oldX, oldY, oldZ;
        std::vector<uint32_t> oldStamp;
        oldX.swap(mHashX);
        oldY.swap(mHashY);
        oldZ.swap(mHashZ);
        oldStamp.swap(mHashStamp);
        const uint32_t gen = mHashGen;
        mHashX.assign(cap, 0);
        mHashY.assign(cap, 0);
        mHashZ.assign(cap, 0);
        mHashStamp.assign(cap, 0u);
        mHashMask = cap - 1;
        mHashCount = 0;
        for (size_t i = 0; i < oldStamp.size(); ++i)
        {
            if (oldStamp[i] == gen)
                hashMark(oldX[i], oldY[i], oldZ[i]);
        }
    }

    // Slots kept on the thread after a triangle. A flood that needs more
    // grows the hash, and end() frees it back to this size.
    static constexpr size_t kHashRetain = 4096;

    bool mDense = false;
    int mX0 = 0, mY0 = 0, mZ0 = 0, mNx = 0, mNy = 0, mNz = 0;
    uint32_t mDenseGen = 0;
    std::vector<uint32_t> mStamp;
    uint32_t mHashGen = 0;
    size_t mHashMask = 0;
    size_t mHashCount = 0;
    std::vector<uint32_t> mHashStamp;
    std::vector<int> mHashX, mHashY, mHashZ;
};

inline FloodVisit& floodVisit()
{
    thread_local FloodVisit oVisit;
    return oVisit;
}

// Four closest points on a non-degenerate triangle. p and cp are xyz xyz xyz xyz.
// cl.exe accepts the AVX2 intrinsics without a target attribute.
// clang-cl is Clang, so it keeps the target attribute below.
PICOGK_MESH_AVX_FN
inline void closestPointsOnTriangle4(
    const double a[3], const double b[3], const double c[3],
    const double p[12], double cp[12])
{
    const __m256d a0 = _mm256_set1_pd(a[0]);
    const __m256d a1 = _mm256_set1_pd(a[1]);
    const __m256d a2 = _mm256_set1_pd(a[2]);
    const __m256d b0 = _mm256_set1_pd(b[0]);
    const __m256d b1 = _mm256_set1_pd(b[1]);
    const __m256d b2 = _mm256_set1_pd(b[2]);
    const __m256d c0 = _mm256_set1_pd(c[0]);
    const __m256d c1 = _mm256_set1_pd(c[1]);
    const __m256d c2 = _mm256_set1_pd(c[2]);

    const __m256d p0 = _mm256_setr_pd(p[0], p[3], p[6], p[9]);
    const __m256d p1 = _mm256_setr_pd(p[1], p[4], p[7], p[10]);
    const __m256d p2 = _mm256_setr_pd(p[2], p[5], p[8], p[11]);

    const __m256d ab0 = _mm256_sub_pd(b0, a0);
    const __m256d ab1 = _mm256_sub_pd(b1, a1);
    const __m256d ab2 = _mm256_sub_pd(b2, a2);
    const __m256d ac0 = _mm256_sub_pd(c0, a0);
    const __m256d ac1 = _mm256_sub_pd(c1, a1);
    const __m256d ac2 = _mm256_sub_pd(c2, a2);
    const __m256d ap0 = _mm256_sub_pd(p0, a0);
    const __m256d ap1 = _mm256_sub_pd(p1, a1);
    const __m256d ap2 = _mm256_sub_pd(p2, a2);

    // A lambda here is a separate function and does not inherit target("avx2").
#define PICOGK_DOT3(x0, y0, z0, x1, y1, z1) \
    _mm256_add_pd(_mm256_add_pd(_mm256_mul_pd((x0), (x1)), _mm256_mul_pd((y0), (y1))), _mm256_mul_pd((z0), (z1)))

    const __m256d zero = _mm256_setzero_pd();
    const __m256d one = _mm256_set1_pd(1.0);

    const __m256d d1 = PICOGK_DOT3(ab0, ab1, ab2, ap0, ap1, ap2);
    const __m256d d2 = PICOGK_DOT3(ac0, ac1, ac2, ap0, ap1, ap2);

    const __m256d bp0 = _mm256_sub_pd(p0, b0);
    const __m256d bp1 = _mm256_sub_pd(p1, b1);
    const __m256d bp2 = _mm256_sub_pd(p2, b2);
    const __m256d d3 = PICOGK_DOT3(ab0, ab1, ab2, bp0, bp1, bp2);
    const __m256d d4 = PICOGK_DOT3(ac0, ac1, ac2, bp0, bp1, bp2);

    const __m256d cpv0 = _mm256_sub_pd(p0, c0);
    const __m256d cpv1 = _mm256_sub_pd(p1, c1);
    const __m256d cpv2 = _mm256_sub_pd(p2, c2);
    const __m256d d5 = PICOGK_DOT3(ab0, ab1, ab2, cpv0, cpv1, cpv2);
    const __m256d d6 = PICOGK_DOT3(ac0, ac1, ac2, cpv0, cpv1, cpv2);

    const __m256d vc = _mm256_sub_pd(_mm256_mul_pd(d1, d4), _mm256_mul_pd(d3, d2));
    const __m256d vb = _mm256_sub_pd(_mm256_mul_pd(d5, d2), _mm256_mul_pd(d1, d6));
    const __m256d va = _mm256_sub_pd(_mm256_mul_pd(d3, d6), _mm256_mul_pd(d5, d4));

    const __m256d maskA = _mm256_and_pd(
        _mm256_cmp_pd(d1, zero, _CMP_LE_OQ),
        _mm256_cmp_pd(d2, zero, _CMP_LE_OQ));
    const __m256d maskB = _mm256_and_pd(
        _mm256_cmp_pd(d3, zero, _CMP_GE_OQ),
        _mm256_cmp_pd(d4, d3, _CMP_LE_OQ));
    const __m256d maskAB = _mm256_and_pd(
        _mm256_cmp_pd(vc, zero, _CMP_LE_OQ),
        _mm256_and_pd(
            _mm256_cmp_pd(d1, zero, _CMP_GE_OQ),
            _mm256_cmp_pd(d3, zero, _CMP_LE_OQ)));
    const __m256d maskC = _mm256_and_pd(
        _mm256_cmp_pd(d6, zero, _CMP_GE_OQ),
        _mm256_cmp_pd(d5, d6, _CMP_LE_OQ));
    const __m256d maskAC = _mm256_and_pd(
        _mm256_cmp_pd(vb, zero, _CMP_LE_OQ),
        _mm256_and_pd(
            _mm256_cmp_pd(d2, zero, _CMP_GE_OQ),
            _mm256_cmp_pd(d6, zero, _CMP_LE_OQ)));
    const __m256d d4d3 = _mm256_sub_pd(d4, d3);
    const __m256d d5d6 = _mm256_sub_pd(d5, d6);
    const __m256d maskBC = _mm256_and_pd(
        _mm256_cmp_pd(va, zero, _CMP_LE_OQ),
        _mm256_and_pd(
            _mm256_cmp_pd(d4d3, zero, _CMP_GE_OQ),
            _mm256_cmp_pd(d5d6, zero, _CMP_GE_OQ)));

    const __m256d tAB = _mm256_div_pd(d1, _mm256_sub_pd(d1, d3));
    const __m256d abx0 = _mm256_add_pd(a0, _mm256_mul_pd(tAB, ab0));
    const __m256d abx1 = _mm256_add_pd(a1, _mm256_mul_pd(tAB, ab1));
    const __m256d abx2 = _mm256_add_pd(a2, _mm256_mul_pd(tAB, ab2));

    const __m256d tAC = _mm256_div_pd(d2, _mm256_sub_pd(d2, d6));
    const __m256d acx0 = _mm256_add_pd(a0, _mm256_mul_pd(tAC, ac0));
    const __m256d acx1 = _mm256_add_pd(a1, _mm256_mul_pd(tAC, ac1));
    const __m256d acx2 = _mm256_add_pd(a2, _mm256_mul_pd(tAC, ac2));

    const __m256d cb0 = _mm256_sub_pd(c0, b0);
    const __m256d cb1 = _mm256_sub_pd(c1, b1);
    const __m256d cb2 = _mm256_sub_pd(c2, b2);
    const __m256d tBC = _mm256_div_pd(d4d3, _mm256_add_pd(d4d3, d5d6));
    const __m256d bcx0 = _mm256_add_pd(b0, _mm256_mul_pd(tBC, cb0));
    const __m256d bcx1 = _mm256_add_pd(b1, _mm256_mul_pd(tBC, cb1));
    const __m256d bcx2 = _mm256_add_pd(b2, _mm256_mul_pd(tBC, cb2));

    const __m256d denom = _mm256_div_pd(one, _mm256_add_pd(_mm256_add_pd(va, vb), vc));
    const __m256d vw = _mm256_mul_pd(vc, denom);
    const __m256d vv = _mm256_mul_pd(vb, denom);
    const __m256d face0 = _mm256_add_pd(_mm256_add_pd(a0, _mm256_mul_pd(ab0, vv)), _mm256_mul_pd(ac0, vw));
    const __m256d face1 = _mm256_add_pd(_mm256_add_pd(a1, _mm256_mul_pd(ab1, vv)), _mm256_mul_pd(ac1, vw));
    const __m256d face2 = _mm256_add_pd(_mm256_add_pd(a2, _mm256_mul_pd(ab2, vv)), _mm256_mul_pd(ac2, vw));

    __m256d r0 = face0;
    __m256d r1 = face1;
    __m256d r2 = face2;
    r0 = _mm256_blendv_pd(r0, bcx0, maskBC);
    r1 = _mm256_blendv_pd(r1, bcx1, maskBC);
    r2 = _mm256_blendv_pd(r2, bcx2, maskBC);
    r0 = _mm256_blendv_pd(r0, acx0, maskAC);
    r1 = _mm256_blendv_pd(r1, acx1, maskAC);
    r2 = _mm256_blendv_pd(r2, acx2, maskAC);
    r0 = _mm256_blendv_pd(r0, c0, maskC);
    r1 = _mm256_blendv_pd(r1, c1, maskC);
    r2 = _mm256_blendv_pd(r2, c2, maskC);
    r0 = _mm256_blendv_pd(r0, abx0, maskAB);
    r1 = _mm256_blendv_pd(r1, abx1, maskAB);
    r2 = _mm256_blendv_pd(r2, abx2, maskAB);
    r0 = _mm256_blendv_pd(r0, b0, maskB);
    r1 = _mm256_blendv_pd(r1, b1, maskB);
    r2 = _mm256_blendv_pd(r2, b2, maskB);
    r0 = _mm256_blendv_pd(r0, a0, maskA);
    r1 = _mm256_blendv_pd(r1, a1, maskA);
    r2 = _mm256_blendv_pd(r2, a2, maskA);

    alignas(32) double x[4], y[4], z[4];
    _mm256_store_pd(x, r0);
    _mm256_store_pd(y, r1);
    _mm256_store_pd(z, r2);
    for (int i = 0; i < 4; ++i)
    {
        cp[i * 3 + 0] = x[i];
        cp[i * 3 + 1] = y[i];
        cp[i * 3 + 2] = z[i];
    }
#undef PICOGK_DOT3
}

#else

inline bool bMeshVoxelAvx2()
{
    return false;
}

#endif // x86-64

} // namespace PicoGK

#endif
