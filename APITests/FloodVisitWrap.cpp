//
// SPDX-License-Identifier: Apache-2.0
//
// Forces the dense visit-set generation to wrap and checks the marks against
// the primId rule: a voxel is new for this triangle until this triangle marks
// it. A stale stamp left past the cleared prefix would drop that voxel.
//
// Built into APITests, and also runnable on its own:
//   c++ -std=c++20 -DPICOGK_FLOOD_VISIT_TEST_MAIN APITests/FloodVisitWrap.cpp
//

#include "../Source/PicoGKMeshVoxelAvx.h"

#include <iostream>
#include <set>
#include <tuple>

#if defined(__x86_64__) || defined(_M_X64)
namespace
{

struct Voxel
{
    int x, y, z;
    bool operator<(const Voxel& o) const
    {
        return std::tie(x, y, z) < std::tie(o.x, o.y, o.z);
    }
};

// primId visit for one triangle: the first mark of a voxel is new.
bool bPrimIdMark(std::set<Voxel>& oSeen, int x, int y, int z)
{
    return oSeen.insert(Voxel{x, y, z}).second;
}

bool bSameMark(PicoGK::FloodVisit& oVisit, std::set<Voxel>& oSeen, int x, int y, int z)
{
    const bool bGot = oVisit.mark(x, y, z);
    const bool bExpected = bPrimIdMark(oSeen, x, y, z);
    if (bGot == bExpected)
        return true;
    std::cerr << "flood visit wrap: voxel " << x << " " << y << " " << z
              << " side-table " << bGot << " primId " << bExpected << "\n";
    return false;
}

bool bBeginLarge(PicoGK::FloodVisit& oVisit)
{
    // Box is 85 x 85 x 5. The far corner (82, 82, 2) sits in the last stamp.
    return oVisit.begin(0, 0, 0, 80, 0, 0, 0, 80, 0, 0, 0, 0);
}

bool bBeginSmall(PicoGK::FloodVisit& oVisit)
{
    // Box is 6 x 6 x 5, so a wrap clears a prefix and leaves the far stamp.
    return oVisit.begin(0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0);
}

} // namespace
#endif

bool bRunFloodVisitWrapTest()
{
#if !defined(__x86_64__) && !defined(_M_X64)
    std::cout << "flood visit wrap test skipped\n";
    return true;
#else
    PicoGK::FloodVisit oVisit;

    // A triangle past the dense cap must stay on the primId tree.
    if (oVisit.begin(0, 0, 0, 2000, 0, 0, 0, 2000, 0, 0, 0, 0))
    {
        std::cerr << "flood visit wrap: oversized triangle used the side table\n";
        return false;
    }

    if (!bBeginLarge(oVisit) || !oVisit.mark(82, 82, 2))
    {
        std::cerr << "flood visit wrap: first large triangle did not fit\n";
        return false;
    }
    // Second large triangle writes generation 2 into the far stamp.
    if (!bBeginLarge(oVisit) || !oVisit.mark(82, 82, 2))
    {
        std::cerr << "flood visit wrap: second large triangle did not fit\n";
        return false;
    }

    oVisit.forceDenseGenForTest(0xffffffffu);
    if (!bBeginSmall(oVisit))
    {
        std::cerr << "flood visit wrap: small triangle did not fit\n";
        return false;
    }
    std::set<Voxel> oSmall;
    oSmall.insert(Voxel{0, 0, 0});
    const int aSmall[][3] = {{1, 0, 0}, {0, 1, 0}, {1, 1, 0}, {-1, 0, 0}, {0, 0, 0}};
    for (const auto& p : aSmall)
    {
        if (!bSameMark(oVisit, oSmall, p[0], p[1], p[2]))
            return false;
    }

    // Generation is 2 again, and the far stamp still held 2 before the fix.
    if (!bBeginLarge(oVisit))
    {
        std::cerr << "flood visit wrap: triangle after the wrap did not fit\n";
        return false;
    }
    std::set<Voxel> oLarge;
    oLarge.insert(Voxel{0, 0, 0});
    if (!bSameMark(oVisit, oLarge, 82, 82, 2))
        return false;
    if (oVisit.mark(82, 82, 2))
    {
        std::cerr << "flood visit wrap: far voxel was marked twice\n";
        return false;
    }
    oLarge.insert(Voxel{82, 82, 2});
    for (int x = 0; x <= 82; x += 7)
    {
        for (int y = 0; y <= 82; y += 7)
        {
            for (int z = 0; z <= 2; ++z)
            {
                if (!bSameMark(oVisit, oLarge, x, y, z))
                    return false;
            }
        }
    }

    std::cout << "flood visit wrap test passed\n";
    return true;
#endif
}

#ifdef PICOGK_FLOOD_VISIT_TEST_MAIN
int main()
{
    return bRunFloodVisitWrapTest() ? 0 : 1;
}
#endif
