# Copies MeshToVolume.h and splices in the optional AVX2 neighbor path.
# The OpenVDB submodule stays untouched. Fails if the anchor text moves.
if(NOT DEFINED SRC OR NOT DEFINED DST)
    message(FATAL_ERROR "SRC and DST are required")
endif()
file(READ "${SRC}" text)
set(old_inc [=[
#include <tbb/task_arena.h>

]=])
set(new_inc [=[
#include <tbb/task_arena.h>

#ifdef PICOGK_MESH_AVX2_HOOK
#include "PicoGKMeshVoxelAvx.h"
#if defined(_MSC_VER)
#define PICOGK_MESH_NOINLINE __declspec(noinline)
#else
#define PICOGK_MESH_NOINLINE __attribute__((noinline))
#endif
#endif

]=])
set(old_body [=[
    static void voxelizeTriangle(const Triangle& prim, VoxelizationDataType& data, Interrupter* const interrupter)
    {
        std::deque<Coord> coordList;
        Coord ijk, nijk;

        ijk = Coord::floor(prim.a);
        coordList.push_back(ijk);

        // The first point may not be quite in bounds, and rely
        // on one of the neighbours to have the first valid seed,
        // so we cannot early-exit here.
        updateDistance(ijk, prim, data);

        unsigned char primId = data.getNewPrimId();
        data.primIdAcc.setValueOnly(ijk, primId);

        while (!coordList.empty()) {
            if (interrupter && interrupter->wasInterrupted()) {
                thread::cancelGroupExecution();
                break;
            }
            for (Int32 pass = 0; pass < 1048576 && !coordList.empty(); ++pass) {
                ijk = coordList.back();
                coordList.pop_back();

                for (Int32 i = 0; i < 26; ++i) {
                    nijk = ijk + util::COORD_OFFSETS[i];
                    if (primId != data.primIdAcc.getValue(nijk)) {
                        data.primIdAcc.setValueOnly(nijk, primId);
                        if(updateDistance(nijk, prim, data)) coordList.push_back(nijk);
                    }
                }
            }
        }
    }

]=])
set(new_body [=[
    // Separate from voxelizeTriangle so the scalar flood keeps its original body.
#ifdef PICOGK_MESH_AVX2_HOOK
    PICOGK_MESH_NOINLINE static void voxelizeTriangleAvx(
        const Triangle& prim,
        VoxelizationDataType& data,
        Interrupter* const interrupter,
        std::deque<Coord>& coordList,
        Coord ijk,
        unsigned char primId)
    {
        Coord nijk;
        while (!coordList.empty()) {
            if (interrupter && interrupter->wasInterrupted()) {
                thread::cancelGroupExecution();
                break;
            }
            for (Int32 pass = 0; pass < 1048576 && !coordList.empty(); ++pass) {
                ijk = coordList.back();
                coordList.pop_back();

                Coord aPending[4];
                int nPending = 0;
                auto flushPending = [&]()
                {
                    if (nPending == 0)
                        return;
                    double pxyz[12];
                    double cxyz[12];
                    const int n = nPending;
                    for (int k = 0; k < 4; ++k) {
                        const int src = k < n ? k : 0;
                        pxyz[k * 3 + 0] = double(aPending[src][0]);
                        pxyz[k * 3 + 1] = double(aPending[src][1]);
                        pxyz[k * 3 + 2] = double(aPending[src][2]);
                    }
                    // Match closestPointOnTriangleToPoint(prim.a, prim.c, prim.b, ...).
                    const double triA[3] = {prim.a[0], prim.a[1], prim.a[2]};
                    const double triB[3] = {prim.c[0], prim.c[1], prim.c[2]};
                    const double triC[3] = {prim.b[0], prim.b[1], prim.b[2]};
                    PicoGK::closestPointsOnTriangle4(triA, triB, triC, pxyz, cxyz);
                    for (int k = 0; k < n; ++k) {
                        const Coord& ncoord = aPending[k];
                        const Vec3d voxelCenter(ncoord[0], ncoord[1], ncoord[2]);
                        const Vec3d closest(cxyz[k * 3 + 0], cxyz[k * 3 + 1], cxyz[k * 3 + 2]);
                        using ValueType = typename TreeType::ValueType;
                        const ValueType dist = ValueType((voxelCenter - closest).lengthSqr());
                        if (std::isnan(dist))
                            continue;
                        const ValueType oldDist = data.distAcc.getValue(ncoord);
                        if (dist < oldDist) {
                            data.distAcc.setValue(ncoord, dist);
                            data.indexAcc.setValue(ncoord, prim.index);
                        } else if (math::isExactlyEqual(dist, oldDist)) {
                            data.indexAcc.setValueOnly(ncoord, std::min(prim.index, data.indexAcc.getValue(ncoord)));
                        }
                        if (!(dist > 0.75))
                            coordList.push_back(ncoord);
                    }
                    nPending = 0;
                };
                for (Int32 i = 0; i < 26; ++i) {
                    nijk = ijk + util::COORD_OFFSETS[i];
                    if (primId != data.primIdAcc.getValue(nijk)) {
                        data.primIdAcc.setValueOnly(nijk, primId);
                        aPending[nPending] = nijk;
                        ++nPending;
                        if (nPending == 4)
                            flushPending();
                    }
                }
                flushPending();
            }
        }
    }
#endif

    static void voxelizeTriangle(const Triangle& prim, VoxelizationDataType& data, Interrupter* const interrupter)
    {
        std::deque<Coord> coordList;
        Coord ijk, nijk;

        ijk = Coord::floor(prim.a);
        coordList.push_back(ijk);

        // The first point may not be quite in bounds, and rely
        // on one of the neighbours to have the first valid seed,
        // so we cannot early-exit here.
        updateDistance(ijk, prim, data);

        unsigned char primId = data.getNewPrimId();
        data.primIdAcc.setValueOnly(ijk, primId);

#ifdef PICOGK_MESH_AVX2_HOOK
        if (PicoGK::bMeshVoxelAvx2()) {
            const bool bDegenerateTriangle =
                (math::isApproxEqual(prim.a, prim.b) && math::isApproxEqual(prim.a, prim.c))
                || math::isApproxEqual(prim.a, prim.b)
                || math::isApproxEqual(prim.a, prim.c)
                || math::isApproxEqual(prim.b, prim.c);
            if (!bDegenerateTriangle) {
                voxelizeTriangleAvx(prim, data, interrupter, coordList, ijk, primId);
                return;
            }
        }
#endif

        while (!coordList.empty()) {
            if (interrupter && interrupter->wasInterrupted()) {
                thread::cancelGroupExecution();
                break;
            }
            for (Int32 pass = 0; pass < 1048576 && !coordList.empty(); ++pass) {
                ijk = coordList.back();
                coordList.pop_back();

                for (Int32 i = 0; i < 26; ++i) {
                    nijk = ijk + util::COORD_OFFSETS[i];
                    if (primId != data.primIdAcc.getValue(nijk)) {
                        data.primIdAcc.setValueOnly(nijk, primId);
                        if(updateDistance(nijk, prim, data)) coordList.push_back(nijk);
                    }
                }
            }
        }
    }

]=])
string(FIND "${text}" "${old_inc}" inc_at)
string(FIND "${text}" "${old_body}" body_at)
if(inc_at EQUAL -1 OR body_at EQUAL -1)
    message(FATAL_ERROR "MeshToVolume.h anchors for the AVX2 splice were not found")
endif()
string(REPLACE "${old_inc}" "${new_inc}" text "${text}")
string(REPLACE "${old_body}" "${new_body}" text "${text}")
string(FIND "${text}" "voxelizeTriangleAvx" ok)
if(ok EQUAL -1)
    message(FATAL_ERROR "MeshToVolume AVX2 splice did not apply")
endif()
get_filename_component(dst_dir "${DST}" DIRECTORY)
file(MAKE_DIRECTORY "${dst_dir}")
file(WRITE "${DST}" "${text}")
