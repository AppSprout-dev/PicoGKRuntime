//
// SPDX-License-Identifier: Apache-2.0
//
// AVX2 closest-point queries for mesh voxelization. Each lane repeats the
// scalar OpenVDB closestPointOnTriangleToPoint arithmetic (mul then add, no
// FMA) so a non-degenerate triangle returns the same point bits. Degenerate
// triangles stay on the scalar path. The switch defaults off.

#ifndef PICOGK_MESH_VOXEL_AVX_H_
#define PICOGK_MESH_VOXEL_AVX_H_

#include <cstdlib>
#include <immintrin.h>

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
// Truthy, trimmed, case-insensitive: 1, true, yes, on.
// Unset, empty, and every other value keep the scalar path. Read once.
inline bool bMeshVoxelAvx2()
{
    static const bool bEnabled = []()
    {
        if (!__builtin_cpu_supports("avx2"))
            return false;
        const char* psz = std::getenv("PICOGK_MESH_AVX2");
        if (psz == nullptr)
            return false;
        while (*psz != '\0' && bMeshVoxelIsSpace(static_cast<unsigned char>(*psz)))
            ++psz;
        const char* pszEnd = psz;
        while (*pszEnd != '\0')
            ++pszEnd;
        while (pszEnd > psz && bMeshVoxelIsSpace(static_cast<unsigned char>(pszEnd[-1])))
            --pszEnd;
        return bMeshVoxelToken(psz, pszEnd, "1")
            || bMeshVoxelToken(psz, pszEnd, "true")
            || bMeshVoxelToken(psz, pszEnd, "yes")
            || bMeshVoxelToken(psz, pszEnd, "on");
    }();
    return bEnabled;
}

// Four closest points on a non-degenerate triangle. p and cp are xyz xyz xyz xyz.
__attribute__((target("avx2")))
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

} // namespace PicoGK

#endif
