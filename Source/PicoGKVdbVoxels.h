//
// SPDX-License-Identifier: Apache-2.0
//
// PicoGK ("peacock") is a compact software kernel for computational geometry,
// specifically for use in Computational Engineering Models (CEM).
//
// For more information, please visit https://picogk.org
//
// PicoGK is developed and maintained by LEAP 71 - © 2023-2026 by LEAP 71
// https://leap71.com
//
// Computational Engineering will profoundly change our physical world in the
// years ahead. Thank you for being part of the journey.
//
// We have developed this library to be used widely, for both commercial and
// non-commercial projects alike. Therefore, have released it under a permissive
// open-source license.
//
// The foundation of PicoGK is a thin layer on top of the powerful open-source
// OpenVDB project, which in turn uses many other Free and Open Source Software
// libraries. We are grateful to be able to stand on the shoulders of giants.
//
// LEAP 71 licenses this file to you under the Apache License, Version 2.0
// (the "License"); you may not use this file except in compliance with the
// License. You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, THE SOFTWARE IS
// PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED.
//
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef PICOGKVDBVOXELS_H_
#define PICOGKVDBVOXELS_H_

#include <openvdb/openvdb.h>
#include <openvdb/tools/Composite.h>
#include <openvdb/tools/MeshToVolume.h>
#include <openvdb/tools/VolumeToMesh.h>
#include <openvdb/tools/LevelSetRebuild.h>
#include <openvdb/tools/LevelSetFilter.h>
#include <openvdb/tools/Diagnostics.h>
#include <openvdb/tools/LevelSetMeasure.h>
#include <openvdb/tools/RayIntersector.h>
#include <openvdb/tools/LevelSetSphere.h>
#include <openvdb/tools/LevelSetTubes.h>
#include <openvdb/tools/LevelSetDilatedMesh.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>
#include <vector>

#include "PicoGKMesh.h"
#include "PicoGKTrace.h"

using namespace openvdb;

#define PICOGK_VOXEL_DEFAULTNARROWBAND 3

namespace PicoGK
{

class Voxels
{

public:
    typedef std::shared_ptr<Voxels> Ptr;
    
    Voxels( VoxelSize oVoxelSize,
            int nNarrowBand)
    {
        m_nSdfNarrowBand    = nNarrowBand;
        m_roGrid            = FloatGrid::create(oVoxelSize.fToMM(nNarrowBand));
        m_roGrid->setGridClass(GRID_LEVEL_SET);
        m_roGrid->setTransform(openvdb::math::Transform::createLinearTransform(oVoxelSize));
    };
    
    Voxels( FloatGrid::Ptr roGrid,
            int nNarrowBand)
    {
        PKTRACE(Voxels_FromGridConstructor);
        
        if (!bHasValidPicoGKTransform(roGrid))
            throw std::invalid_argument("Source grid sent to Voxels constructor has complex transformation");
        
        m_nSdfNarrowBand    = nNarrowBand;
        m_roGrid            = roGrid;
        m_roGrid->setGridClass(GRID_LEVEL_SET);
    };
    
    Voxels(const Voxels& oSource)
    {
        PKTRACE(Voxels_CopyConstructor);
        
        m_roGrid = deepCopyTypedGrid<FloatGrid>(oSource.m_roGrid);
        m_nSdfNarrowBand = oSource.m_nSdfNarrowBand;
        
        assert(m_roGrid->getGridClass() == GRID_LEVEL_SET);
        assert(bHasValidPicoGKTransform(m_roGrid));
    };
    
    Voxels( VoxelSize oVoxSize,
            int32_t nNarrowBand,
            const Vector3& vecCenter,
            float fRadius)
    {
        PKTRACE(Voxels_SphereConstructor);
        
        m_roGrid = openvdb::tools::createLevelSetSphere<FloatGrid>( fRadius,
                                                                    openvdb::Vec3f( vecCenter.X,
                                                                                    vecCenter.Y,
                                                                                    vecCenter.Z),
                                                                    oVoxSize,
                                                                    nNarrowBand);
        
        m_nSdfNarrowBand = nNarrowBand;
        
        assert(m_roGrid->getGridClass() == GRID_LEVEL_SET);
        assert(bHasValidPicoGKTransform(m_roGrid));
    }
    
    Voxels( VoxelSize oVoxSize,
            int32_t nNarrowBand,
            const Vector3& vecStart,
            const Vector3& vecEnd,
            float fRadiusStart,
            float fRadiusEnd)
    {
        PKTRACE(Voxels_CapsuleConstructor);
        
        m_roGrid = openvdb::tools::createLevelSetTaperedCapsule<FloatGrid>( openvdb::Vec3f( vecStart.X,
                                                                                            vecStart.Y,
                                                                                            vecStart.Z),
                                                                            openvdb::Vec3f( vecEnd.X,
                                                                                            vecEnd.Y,
                                                                                            vecEnd.Z),
                                                                            fRadiusStart,
                                                                            fRadiusEnd,
                                                                            oVoxSize,
                                                                            nNarrowBand);
        
        m_nSdfNarrowBand = nNarrowBand;
        
        assert(m_roGrid->getGridClass() == GRID_LEVEL_SET);
        assert(bHasValidPicoGKTransform(m_roGrid));
    }
    
    Voxels( VoxelSize oVoxSize,
            int32_t nNarrowBand,
            const Mesh& oMesh,
            float fRadius)
    {
        m_roGrid = openvdb::tools::createLevelSetDilatedMesh<FloatGrid>(    (const std::vector<math::Vec3<float>>&) oMesh.vVertices(),
                                                                            (const std::vector<Vec3I>&)             oMesh.vTriangles(),
                                                                            fRadius,
                                                                            oVoxSize,
                                                                            nNarrowBand);
        
        m_nSdfNarrowBand = nNarrowBand;
        
        assert(m_roGrid->getGridClass() == GRID_LEVEL_SET);
        assert(bHasValidPicoGKTransform(m_roGrid));
    }

    ~Voxels()
    {
    }
    
    std::string strDiagnose() const
    {
        return tools::checkLevelSet<FloatGrid>(*m_roGrid);
    }
    
    int64_t nMemUsage() const
    {
        return sizeof(Voxels) + m_roGrid->memUsage();
    }
    
    bool bIsEmpty() const
    {
        PKTRACE(Voxels_bIsEmpty);
        
        if (m_roGrid->tree().empty())
            return true;
        
        // We check for values below 0, which are inside the object.
        // Technically 0 would be at the surface, but 0 values are often
        // left over after boolean operations, and an object that has
        // 0 values, but no value below 0 is fundamentally empty
        for (auto iter = m_roGrid->cbeginValueOn(); iter.test(); ++iter)
        {
            if (*iter < 0.0f)
            {
                return false;
            }
        }
        
        return true;
    }
    
    bool bIsEqual(const Voxels& oCompare) const
    {
        PKTRACE(Voxels_bIsEqual);
        
        if (m_roGrid->transform() != oCompare.m_roGrid->transform())
            return false;
        
        CoordBBox oBBoxThis = m_roGrid->evalActiveVoxelBoundingBox();
        CoordBBox oBBoxComp = oCompare.m_roGrid->evalActiveVoxelBoundingBox();
        
        int32_t iMinX = std::min(   oBBoxThis.min().x(),
                                    oBBoxComp.min().x());
        
        int32_t iMinY = std::min(   oBBoxThis.min().y(),
                                    oBBoxComp.min().y());
        
        int32_t iMinZ = std::min(   oBBoxThis.min().z(),
                                    oBBoxComp.min().z());
        
        int32_t iMaxX = std::max(   oBBoxThis.max().x(),
                                    oBBoxComp.max().x());
        
        int32_t iMaxY = std::max(   oBBoxThis.max().y(),
                                    oBBoxComp.max().y());
        
        int32_t iMaxZ = std::max(   oBBoxThis.max().z(),
                                    oBBoxComp.max().z());
        
        auto oThis = m_roGrid->getConstAccessor();
        auto oComp = oCompare.m_roGrid->getConstAccessor();
        
        for(int32_t x = iMinX; x <= iMaxX; x++)
        for(int32_t y = iMinY; y <= iMaxY; y++)
        for(int32_t z = iMinZ; z <= iMaxZ; z++)
        {
            openvdb::Coord xyz(x,y,z);
            
            bool bThisInside = (oThis.getValue(xyz) <= 0.0f);
            bool bCompInside = (oComp.getValue(xyz) <= 0.0f);
            
            if (bThisInside != bCompInside)
                return false;
        }
        
        return true;
     }

    void BoolAdd(const Voxels& oOther)
    {
        PKTRACE(Voxels_BoolAdd);
        
        FloatGrid::Ptr roOperand = deepCopyTypedGrid<FloatGrid>(oOther.m_roGrid);
        openvdb::tools::csgUnion(*m_roGrid, *roOperand);
        
        RebuildGrid();
    }

    void BoolSubtract(const Voxels& oOther)
    {
        PKTRACE(Voxels_BoolSubtract);
        
        FloatGrid::Ptr roOperand = deepCopyTypedGrid<FloatGrid>(oOther.m_roGrid);
        openvdb::tools::csgDifference(*m_roGrid, *roOperand);
        
        RebuildGrid();
    }

    void BoolIntersect(const Voxels& oOther)
    {
        PKTRACE(Voxels_BoolIntersect);
        
        FloatGrid::Ptr roOperand = deepCopyTypedGrid<FloatGrid>(oOther.m_roGrid);
        openvdb::tools::csgIntersection(*m_roGrid, *roOperand);
        
        RebuildGrid();
    }
    
    void Offset(float fSize)
    {
        PKTRACE(Voxels_Offset);
        
        // Create a clean level set with the extended size narrow band
        RebuildGrid(std::abs(fSize));
        
        openvdb::tools::LevelSetFilter<openvdb::FloatGrid> oFilter(*m_roGrid);
        
        // OpenVDB treats offsets as inwards
        oFilter.offset(-fSize);
        
        RebuildGrid();
    }
    
    void DoubleOffset(  float fSize1,
                        float fSize2)
    {
        PKTRACE(Voxels_DoubleOffset);
        
        // Create a clean level set with the extended size narrow band
        float fNarrowBandMM = std::max(std::abs(fSize1), std::abs(fSize2));
        RebuildGrid(fNarrowBandMM);
        
        openvdb::tools::LevelSetFilter<openvdb::FloatGrid> oFilter(*m_roGrid);
        
        // OpenVDB treats offsets as inwards
        oFilter.offset(-fSize1);
        oFilter.offset(-fSize2);
        
        RebuildGrid();
    }
    
    void TripleOffset(float fSize)
    {
        PKTRACE(Voxels_TripleOffset);
        
        // Create a clean level set with the extended size narrow band
        RebuildGrid(std::abs(fSize));
        
        openvdb::tools::LevelSetFilter<openvdb::FloatGrid> oFilter(*m_roGrid);
        
        // offset inwards first
        oFilter.offset(-fSize);
        
        // offset twice the size outwards next
        oFilter.offset(fSize * 2);
        
        // offset inwards again. Now we are back where we started
        // but have lost a lot of detail = smooth
        oFilter.offset(-fSize);
        
        RebuildGrid();
    }

    void RenderMesh(const Mesh& oMesh)
    {
        PKTRACE(Voxels_RenderMesh);
        
        FloatGrid::Ptr roVoxelized = roFloatGridFromMesh(   oMesh,
                                                            oVoxelSize(),
                                                            m_nSdfNarrowBand);
        
        openvdb::tools::csgUnion(*m_roGrid, *roVoxelized);
    }
    
    void RenderLattice(const Lattice& oLattice)
    {
        PKTRACE(Voxels_RenderLattice);
        
        auto oAccess = m_roGrid->getAccessor();
        
        for (auto roSphere : oLattice.oSpheres())
        {
            DoRenderLattice(&oAccess, *roSphere, oVoxelSize(), m_nSdfNarrowBand);
        }
        
        for (auto roBeam : oLattice.oBeams())
        {
            DoRenderLattice(&oAccess, *roBeam, oVoxelSize(), m_nSdfNarrowBand);
        }
    }
    
    void RenderImplicit(    const BBox3& oBBox,
                            PKPFnfSdf pfn)
    {
        PKTRACE(Voxels_RenderImplicit);

        // Coarse blocks are skipped only when samples are consistent with a
        // signed-distance Lipschitz bound and the whole block is strictly
        // outside the narrow band (left as +background) or strictly inside
        // (inactive -background). Anything else takes the original per-voxel
        // loop, so narrow-band values stay the callback's floats. The callback
        // stays on this thread; a worker pool would change the C# contract.
        Coord xyzMin = oVoxelSize().xyzToVoxels(oBBox.vecMin);
        Coord xyzMax = oVoxelSize().xyzToVoxels(oBBox.vecMax);
        const int32_t x0 = xyzMin.X - m_nSdfNarrowBand;
        const int32_t y0 = xyzMin.Y - m_nSdfNarrowBand;
        const int32_t z0 = xyzMin.Z - m_nSdfNarrowBand;
        const int32_t x1 = xyzMax.X + m_nSdfNarrowBand;
        const int32_t y1 = xyzMax.Y + m_nSdfNarrowBand;
        const int32_t z1 = xyzMax.Z + m_nSdfNarrowBand;

        const bool bReference = bEnvFlag("PICOGK_IMPLICIT_REFERENCE");
        const float fBackground = fBackgroundMM();
        const std::int64_t nDomain = nImplicitDomain(x0, x1, y0, y1, z0, z1);
        std::uint64_t nCalls = 0;

        if (bReference || fBackground <= 0.0f || nDomain < 512)
        {
            nCalls = RenderImplicitDense(x0, x1, y0, y1, z0, z1, pfn);
        }
        else
        {
            ImplicitCoarsen oCoarsen(this, pfn, fBackground, m_roGrid->tree().empty());
            oCoarsen.Run(x0, x1, y0, y1, z0, z1);
            nCalls = oCoarsen.m_nCalls;
        }

        if (bEnvFlag("PICOGK_IMPLICIT_STATS"))
        {
            const std::uint64_t nSkipped = (nCalls < static_cast<std::uint64_t>(nDomain))
                ? (static_cast<std::uint64_t>(nDomain) - nCalls)
                : 0;
            std::fprintf(stderr,
                "PICOGK_IMPLICIT_STATS domain=%lld calls=%llu skipped=%llu\n",
                static_cast<long long>(nDomain),
                static_cast<unsigned long long>(nCalls),
                static_cast<unsigned long long>(nSkipped));
        }

        RebuildGrid();
    }
    
    void IntersectImplicit(PKPFnfSdf pfn)
    {
        PKTRACE(Voxels_IntersectImplicit);
        
        Voxels oVox(oVoxelSize(), fBackgroundMM());
        
        CoordBBox oBBox = m_roGrid->evalActiveVoxelBoundingBox();
        
        BBox3 oBBoxMM;
        oBBoxMM.vecMin.X = oVoxelSize().fToMM(oBBox.min().x());
        oBBoxMM.vecMin.Y = oVoxelSize().fToMM(oBBox.min().y());
        oBBoxMM.vecMin.Z = oVoxelSize().fToMM(oBBox.min().z());
        
        oBBoxMM.vecMax.X = oVoxelSize().fToMM(oBBox.max().x());
        oBBoxMM.vecMax.Y = oVoxelSize().fToMM(oBBox.max().y());
        oBBoxMM.vecMax.Z = oVoxelSize().fToMM(oBBox.max().z());
        
        oVox.RenderImplicit(oBBoxMM, pfn);
        
        // Swap out the grids, so we keep using the "nice"
        // implict grid, and use our grid just as the mask
        // Not sure if this is really necessary, but the
        // implicit grid is "perfect"
        m_roGrid.swap(oVox.m_roGrid);
        
        BoolIntersect(oVox);
    }

    Mesh::Ptr roAsMesh() const
    {
        PKTRACE(Voxels_roAsMesh);
        
        Mesh::Ptr roMesh = std::make_shared<Mesh>();
        
    	std::vector< openvdb::Vec3s > oPoints;
    	std::vector< openvdb::Vec3I > oTriangles;
    	std::vector< openvdb::Vec4I > oQuads;
		
        openvdb::tools::volumeToMesh<openvdb::FloatGrid>(*m_roGrid,
                                                         oPoints,
                                                         oTriangles,
                                                         oQuads,
                                                         0.0f,
                                                         0.0,
                                                         false);

        for (const openvdb::Vec4I oQuad : oQuads)
        {
            openvdb::Vec3I o1(oQuad[0], oQuad[1], oQuad[2]);
            oTriangles.push_back(o1);

            openvdb::Vec3I o2(oQuad[2], oQuad[3], oQuad[0]);
            oTriangles.push_back(o2);
        }
	    
        for (const openvdb::Vec3s& v : oPoints)
        {
            roMesh->nAddVertex(Vector3(v.x(), v.y(), v.z()));
        }

        for (const openvdb::Vec3I oTri : oTriangles)
        {
            roMesh->nAddTriangle(   PicoGK::Triangle(   oTri[2],
                                                        oTri[1],
                                                        oTri[0]));
        }

        return roMesh;
    }
    
    void ProjectZSliceDn(   float fZStart,
                            float fZEnd)
    {
        assert(fZStart > fZEnd);
        
        int32_t iZStart = oVoxelSize().iToVoxels(fZStart);
        int32_t iZEnd   = oVoxelSize().iToVoxels(fZEnd);
        CoordBBox oBBox = m_roGrid->evalActiveVoxelBoundingBox();
        
        auto oAccess = m_roGrid->getAccessor();
        
        for(int32_t x = oBBox.min().x(); x <= oBBox.max().x(); x++)
        for(int32_t y = oBBox.min().y(); y <= oBBox.max().y(); y++)
        for(int32_t z = iZStart; z > iZEnd; z--)
        {
            
            openvdb::Coord xyz(x,y,z);
            openvdb::Coord xyzUnder(x,y,z-1);
            
            float fValue = std::min(    oAccess.getValue(xyzUnder),
                                        oAccess.getValue(xyz));
            
            SetSdValue(&oAccess, xyzUnder, fBackgroundMM(), fValue);
        }
        
        // Close the last slice, and update the background
        for(int32_t z = iZEnd; z > iZEnd - (int) (0.5f + m_roGrid->background()); z--)
        {
            for(int32_t x = oBBox.min().x(); x <= oBBox.max().x(); x++)
            for(int32_t y = oBBox.min().y(); y <= oBBox.max().y(); y++)
            {
                openvdb::Coord xyz(x,y,z);
                openvdb::Coord xyzUnder(x,y,z-1);
                
                float fValue = (oAccess.getValue(xyz) + oAccess.getValue(xyzUnder)) / 2.0f;
                SetSdValue(&oAccess, xyz, fBackgroundMM(), fValue);
            }
        }
        
        RebuildGrid();
    }
    
    void ProjectZSliceUp(   float fZStart,
                            float fZEnd)
    {
        assert(fZStart < fZEnd);
        
        int32_t iZStart = oVoxelSize().iToVoxels(fZStart);
        int32_t iZEnd   = oVoxelSize().iToVoxels(fZEnd);
        CoordBBox oBBox = m_roGrid->evalActiveVoxelBoundingBox();
        
        auto oAccess = m_roGrid->getAccessor();
        
        for(int32_t x = oBBox.min().x(); x <= oBBox.max().x(); x++)
        for(int32_t y = oBBox.min().y(); y <= oBBox.max().y(); y++)
        for(int32_t z = iZStart; z < iZEnd; z++)
        {
            openvdb::Coord xyz(x,y,z);
            openvdb::Coord xyzOver(x,y,z+1);
            
            float fValue = std::min(    oAccess.getValue(xyzOver),
                                        oAccess.getValue(xyz));
            
            SetSdValue(&oAccess, xyzOver, fBackgroundMM(), fValue);
        }
        
        // Close the last slice, and update the background
        for(int32_t z = iZEnd; z < iZEnd + (int) (0.5f + m_roGrid->background()); z++)
        {
            for(int32_t x = oBBox.min().x(); x <= oBBox.max().x(); x++)
            for(int32_t y = oBBox.min().y(); y <= oBBox.max().y(); y++)
            {
                openvdb::Coord xyz(x,y,z);
                openvdb::Coord xyzOver(x,y,z+1);
                
                float fValue = (oAccess.getValue(xyz) + oAccess.getValue(xyzOver)) / 2.0f;
                SetSdValue(&oAccess, xyz, fBackgroundMM(), fValue);
            }
        }
        
        RebuildGrid();
    }

    void ProjectZSlice( float fZStart,
                        float fZEnd)
    {
        if (fZStart > fZEnd)
            ProjectZSliceDn(fZStart, fZEnd);
        else
            ProjectZSliceUp(fZStart, fZEnd);
    }
    
    float fCalculateVolume()
    {
        PKTRACE(Voxels_fCalculateVolume);
        
        if (m_roGrid->tree().empty())
        {
            return 0.0f;
        }
        
        RebuildGrid();
        
        return openvdb::tools::levelSetVolume(*m_roGrid, true);
    }
    
    inline bool bIsInside(Vector3 vecTest)
    {
        auto oAccess = m_roGrid->getConstAccessor();
    
        openvdb::Coord xyz( oVoxelSize().iToVoxels(vecTest.X),
                            oVoxelSize().iToVoxels(vecTest.Y),
                            oVoxelSize().iToVoxels(vecTest.Z));
        
        return (oAccess.getValue(xyz) <= 0.0f);
    }
    
    inline void GetSurfaceNormal(   Vector3 vecPt,
                                    Vector3* pvecNormal)
    {
        math::GradStencil oStencil(*m_roGrid);
        Coord xyz = oVoxelSize().xyzToVoxels(vecPt);
    
        oStencil.moveTo(    openvdb::Coord( xyz.X,
                                            xyz.Y,
                                            xyz.Z));
        
        auto vecGradient = oStencil.gradient();
        vecGradient.normalize();
        
        pvecNormal->X = vecGradient.x();
        pvecNormal->Y = vecGradient.y();
        pvecNormal->Z = vecGradient.z();
    }
    
    inline bool bFindClosestPointOnSurface( Vector3 vecSearch,
                                            Vector3* pvecSurfacePoint)
    {
        Coord xyzSearch = oVoxelSize().xyzToVoxels(vecSearch);
        
        CoordBBox oBBox = m_roGrid->evalActiveVoxelBoundingBox();
        oBBox.expand(openvdb::Coord(xyzSearch.X, xyzSearch.Y, xyzSearch.Z));
        // Add the search point to the BBox and expand a little to avoid
        // stopping too soon
        
        auto oAccess    = m_roGrid->getConstAccessor();
        
        int32_t iMaxSearchRadius = (int32_t) std::ceil(sqrt(
                    oBBox.extents().x() * oBBox.extents().x() +
                    oBBox.extents().y() * oBBox.extents().y() +
                    oBBox.extents().z() * oBBox.extents().z()));
        
        bool bStartInside = oAccess.getValue(   
                                openvdb::Coord( xyzSearch.X,
                                                xyzSearch.Y,
                                                xyzSearch.Z)) <= 0.0f;
        
        for (int32_t iR=0; iR<iMaxSearchRadius; iR++)
        {
            bool bOutsideActiveBounds = true;
            Coord xyzSurfacePoint;
            
            if (bBresenhamSphereHitTest(    bStartInside,
                                            xyzSearch,
                                            iR,
                                            oAccess,
                                            oBBox,
                                            &xyzSurfacePoint,
                                            &bOutsideActiveBounds))
            {
                *pvecSurfacePoint = oVoxelSize().vecToMM(xyzSurfacePoint);
                return true;
            }
            
            if (bOutsideActiveBounds)
                return false;
        }
        
        return false;
    }
    
    inline bool bRayCastToSurface(  const Vector3& vecSearch,
                                    const Vector3& vecDirection,
                                    Vector3* pvecSurfacePoint)
    {
        tools::LevelSetRayIntersector oIntersector(*m_roGrid);
        math::Ray<Real> oRay(   Vec3f(  oVoxelSize().fToVoxels(vecSearch.X),
                                        oVoxelSize().fToVoxels(vecSearch.Y),
                                        oVoxelSize().fToVoxels(vecSearch.Z)),
                                Vec3f(  vecDirection.X,
                                        vecDirection.Y,
                                        vecDirection.Z));
        
        math::Vec3<Real> xyz;
        math::Vec3<Real> vecNormal;
        
        if (oIntersector.intersectsIS(oRay, xyz))
        {
            *pvecSurfacePoint = oVoxelSize().vecToMM( Coord(    xyz.x(),
                                                                xyz.y(),
                                                                xyz.z()));
            return true;
        }

        // If the ray reaches its maximum range without hitting a filled voxel,
        // return false to indicate that there was no intersection.
            
        return false;
    }
    
    void GetVoxelDimensions(    int32_t* pnXMin,
                                int32_t* pnYMin,
                                int32_t* pnZMin,
                                int32_t* pnXSize,
                                int32_t* pnYSize,
                                int32_t* pnZSize) const
    {
        CoordBBox oBBox = m_roGrid->evalActiveVoxelBoundingBox();
        
        *pnXMin     = oBBox.min().x();
        *pnYMin     = oBBox.min().y();
        *pnZMin     = oBBox.min().z();
        *pnXSize    = oBBox.extents().x();
        *pnYSize    = oBBox.extents().y();
        *pnZSize    = oBBox.extents().z();
    }
    
    void GetZSlice( int32_t nZSlice,
                    float* pfBuffer)
    {
        CoordBBox oBBox = m_roGrid->evalActiveVoxelBoundingBox();
        openvdb::Coord xyz(0, 0, nZSlice + oBBox.min().z());
        
        auto oAccess = m_roGrid->getConstAccessor();
        
        int32_t n=0;
        for (xyz.y()=oBBox.max().y(); xyz.y()>=oBBox.min().y(); xyz.y()--)
        for (xyz.x()=oBBox.min().x(); xyz.x()<=oBBox.max().x(); xyz.x()++)
        {
            pfBuffer[n] = oAccess.getValue(xyz);
            n++;
        }
    }
    
    void GetXSlice( int32_t nXSlice,
                    float* pfBuffer)
    {
        CoordBBox oBBox = m_roGrid->evalActiveVoxelBoundingBox();
        openvdb::Coord xyz(nXSlice + oBBox.min().x(), 0, 0);
        
        auto oAccess = m_roGrid->getConstAccessor();
        
        int32_t n=0;
        for (xyz.z()=oBBox.max().z(); xyz.z()>=oBBox.min().z(); xyz.z()--)
        for (xyz.y()=oBBox.min().y(); xyz.y()<=oBBox.max().y(); xyz.y()++)
        {
            pfBuffer[n] = oAccess.getValue(xyz);
            n++;
        }
    }
    
    void GetYSlice( int32_t nYSlice,
                    float* pfBuffer)
    {
        CoordBBox oBBox = m_roGrid->evalActiveVoxelBoundingBox();
        openvdb::Coord xyz(0, nYSlice + oBBox.min().y(), 0);
        
        auto oAccess = m_roGrid->getConstAccessor();
        
        int32_t n=0;
        for (xyz.z()=oBBox.max().z(); xyz.z()>=oBBox.min().z(); xyz.z()--)
        for (xyz.x()=oBBox.min().x(); xyz.x()<=oBBox.max().x(); xyz.x()++)
        {
            pfBuffer[n] = oAccess.getValue(xyz);
            n++;
        }
    }
    
    void GetInterpolatedZSlice( float fZSlice,
                                float* pfBuffer)
    {
        CoordBBox oBBox = m_roGrid->evalActiveVoxelBoundingBox();
        
        auto oAccess = m_roGrid->getConstAccessor();
        
        openvdb::Vec3R vec(0, 0, fZSlice + oBBox.min().z());
        openvdb::tools::BoxSampler oSampler;
        
        int32_t n=0;
        for (vec.y()=oBBox.max().y(); vec.y()>=oBBox.min().y(); vec.y()--)
        for (vec.x()=oBBox.min().x(); vec.x()<=oBBox.max().x(); vec.x()++)
        {
            pfBuffer[n] = oSampler.sample(oAccess, vec);
            n++;
        }
    }
    
    FloatGrid::Ptr roVdbGrid() const 	{return m_roGrid;}
    
    inline float fBackgroundMM() const  {return m_roGrid->background();}
    
    inline VoxelSize oVoxelSize() const
    {
        assert(bHasValidPicoGKTransform(m_roGrid));
        return VoxelSize(m_roGrid->voxelSize().x());
    }
    
protected:
    FloatGrid::Ptr      m_roGrid;
    int32_t             m_nSdfNarrowBand;
    
    bool bLevelSetNeedsRebuild() const
    {
        return strDiagnose().length() > 0;
    }
    
    /// Rebuilds the grid after possibly destructive changes to the signed distances
    void RebuildGrid(float fDistanceMM = 0.0f)
    {
        /// Disabled until we determine it is necessary
        return;
        
        PKTRACE(Voxels_RebuildGrid);
        
        /// TODO — this is probably triggering way too often.
        /// We will need a careful analysis of when this is necessary
        /// Also, we need to carefully evaluate what it does to internal cavities
        
        if ((fDistanceMM > 0.0f) || (bLevelSetNeedsRebuild()))
        {
            std::cerr << "- Rebuilding Level Set '" << strDiagnose() << " ... ";
            
            float fHalfVal = oVoxelSize().fToVoxels(fDistanceMM);
            
            if (fHalfVal < m_nSdfNarrowBand)
                fHalfVal = m_nSdfNarrowBand;
            
            m_roGrid = openvdb::tools::levelSetRebuild(*m_roGrid, 0.0f, fHalfVal, fHalfVal);
            
            std::cerr << "Done - Result: '" << strDiagnose() << "'\n";
        }
    }
    
    template<class TAccessor, class TLatticeBeam>
    static void DoRenderLattice(    TAccessor* poAccess,
                                    const TLatticeBeam& oLattice,
                                    VoxelSize oVoxelSize,
                                    int32_t nNarrowBand)
    {
        Vector3 vecMin(oLattice.vecMin());
        Vector3 vecMax(oLattice.vecMax());
            
        Coord xyzMin = oVoxelSize.xyzToVoxels(vecMin);
        Coord xyzMax = oVoxelSize.xyzToVoxels(vecMax);
        
        float fBackground = oVoxelSize.fToMM(nNarrowBand);
        
        for(int32_t x = xyzMin.X - nNarrowBand; x <= xyzMax.X + nNarrowBand; x++)
        for(int32_t y = xyzMin.Y - nNarrowBand; y <= xyzMax.Y + nNarrowBand; y++)
        for(int32_t z = xyzMin.Z - nNarrowBand; z <= xyzMax.Z + nNarrowBand; z++)
        {
            openvdb::Coord xyz(x,y,z);
            
            Vector3 vecSample = oVoxelSize.vecToMM(Coord(x,y,z));
            
            // Boolean add to existing value, if one exists
            float fValue = std::min(    oLattice.fSdValue(vecSample),
                                        poAccess->getValue(xyz));
            
            SetSdValue(poAccess, xyz, fBackground, fValue);
        }
    }
    
    static FloatGrid::Ptr roFloatGridFromMesh(  const Mesh& oMesh,
                                                VoxelSize oVoxelSize,
                                                int32_t nHalfWidth)
    {
        std::vector< openvdb::Vec3s >   oVertices;
        std::vector< openvdb::Vec3I >   oTriangles;
        
        for (int32_t n=0; n<oMesh.nVertexCount(); n++)
        {
            Vector3 v(0.0f,0.0f,0.0f);
            oMesh.GetVertex(n, (Vector3*)&v);
            oVertices.push_back(openvdb::Vec3s( v.X,
                                                v.Y,
                                                v.Z));
        }
        
        for (int32_t n=0; n<oMesh.nTriangleCount(); n++)
        {
            Triangle t;
            oMesh.GetTriangle(n, &t);
            oTriangles.push_back(openvdb::Vec3I(    (unsigned int) t.A,
                                                    (unsigned int) t.B,
                                                    (unsigned int) t.C));
        }

        openvdb::math::Transform::Ptr roTransform
            = openvdb::math::Transform::createLinearTransform(oVoxelSize);

        return openvdb::tools::meshToLevelSet<openvdb::FloatGrid>(  *roTransform,
                                                                    oVertices,
                                                                    oTriangles,
                                                                    nHalfWidth);
    }
    
    template <class TAccessor>
    inline bool bBresenhamSphereHitTest(    bool bReferenceInside,
                                            Coord xyzCenter,
                                            int iRadius,
                                            const TAccessor& oAccess,
                                            const openvdb::CoordBBox& oBB,
                                            Coord* pxyzPoint,
                                            bool* pbOutsideActiveBounds)
    {
        *pbOutsideActiveBounds = true;
        
        openvdb::Coord xyz;
        
        for (xyz.z() = xyzCenter.Z - iRadius; xyz.z() <= xyzCenter.Z + iRadius; xyz.z()++)
        {
            int r2 = iRadius * iRadius;
            for (xyz.y() = xyzCenter.Y - iRadius; xyz.y() <= xyzCenter.Y + iRadius; xyz.y()++)
            {
                for (xyz.x() = xyzCenter.X - iRadius; xyz.x() <= xyzCenter.X + iRadius; xyz.x()++)
                {
                    if (!oBB.isInside(xyz))
                        continue; // don't search outside active voxel area
                    
                    *pbOutsideActiveBounds = false; // we are still inside the bounds
                    
                    int dx = xyz.x() - xyzCenter.X;
                    int dy = xyz.y() - xyzCenter.Y;
                    int dz = xyz.z() - xyzCenter.Z;
                    if (dx * dx + dy * dy + dz * dz <= r2)
                    {
                        bool bInside = oAccess.getValue(xyz) <= 0.0f;
                        if (bInside != bReferenceInside)
                        {
                            pxyzPoint->X = xyz.x();
                            pxyzPoint->Y = xyz.y();
                            pxyzPoint->Z = xyz.z();
                            return true;
                        }
                    }
                }
            }
        }
        
        return false;
    }
    
    static bool bEnvFlag(const char* pszName)
    {
        const char* psz = std::getenv(pszName);
        return psz != nullptr && psz[0] == '1' && psz[1] == '\0';
    }

    static std::int64_t nImplicitDomain(   int32_t x0, int32_t x1,
                                            int32_t y0, int32_t y1,
                                            int32_t z0, int32_t z1)
    {
        if (x0 > x1 || y0 > y1 || z0 > z1)
            return 0;
        const std::int64_t nx = std::int64_t(x1) - x0 + 1;
        const std::int64_t ny = std::int64_t(y1) - y0 + 1;
        const std::int64_t nz = std::int64_t(z1) - z0 + 1;
        return nx * ny * nz;
    }

    // Original serial loop. Kept as the fallback and as the reference path.
    std::uint64_t RenderImplicitDense(  int32_t x0, int32_t x1,
                                        int32_t y0, int32_t y1,
                                        int32_t z0, int32_t z1,
                                        PKPFnfSdf pfn)
    {
        auto oAccess = m_roGrid->getAccessor();
        const float fBackground = fBackgroundMM();
        std::uint64_t nCalls = 0;
        for (int32_t x = x0; x <= x1; x++)
        for (int32_t y = y0; y <= y1; y++)
        for (int32_t z = z0; z <= z1; z++)
        {
            Vector3 vecSample = oVoxelSize().vecToMM(Coord(x, y, z));
            openvdb::Coord xyz(x, y, z);
            float fValue = std::min(    (*pfn)(&vecSample),
                                        oAccess.getValue(xyz));
            SetSdValue(&oAccess, xyz, fBackground, fValue);
            nCalls++;
        }
        return nCalls;
    }

    // Hierarchical skip for RenderImplicit. Probe samples stay in a map so a
    // voxel's callback runs at most once. Pruned outside voxels are not
    // written: on an empty tree that is the same +background inactive state
    // SetSdValue would store. Pruned inside voxels are inactive -background,
    // which is what SetSdValue clamps to when the sample is <= -background.
    struct ImplicitCoarsen
    {
        static constexpr float kLipschitz = 1.05f;
        static constexpr float kSlack = 1.0e-4f;
        static constexpr int kProbeCap = 24;

        struct Key
        {
            int32_t x;
            int32_t y;
            int32_t z;
            bool operator==(const Key& o) const
            {
                return x == o.x && y == o.y && z == o.z;
            }
        };

        struct KeyHash
        {
            std::size_t operator()(const Key& k) const
            {
                std::size_t h = static_cast<std::size_t>(static_cast<std::uint32_t>(k.x)) * 73856093u;
                h ^= static_cast<std::size_t>(static_cast<std::uint32_t>(k.y)) * 19349663u;
                h ^= static_cast<std::size_t>(static_cast<std::uint32_t>(k.z)) * 83492791u;
                return h;
            }
        };

        struct Probe
        {
            int32_t x;
            int32_t y;
            int32_t z;
            float value;
        };

        struct Box
        {
            int32_t x0, x1, y0, y1, z0, z1;
        };

        Voxels* m_poSelf;
        PKPFnfSdf m_pfn;
        float m_fH;
        float m_fB;
        bool m_bEmptyTree;
        std::unordered_map<Key, float, KeyHash> m_oCache;
        std::vector<Box> m_oInside;
        std::vector<Box> m_oDense;
        std::vector<Box> m_oOutsideFix;
        std::uint64_t m_nCalls;

        ImplicitCoarsen(Voxels* poSelf, PKPFnfSdf pfn, float fBackground, bool bEmptyTree)
        : m_poSelf(poSelf)
        , m_pfn(pfn)
        , m_fH(poSelf->oVoxelSize())
        , m_fB(fBackground)
        , m_bEmptyTree(bEmptyTree)
        , m_nCalls(0)
        {
            m_oCache.reserve(1 << 16);
        }

        void Run(int32_t x0, int32_t x1, int32_t y0, int32_t y1, int32_t z0, int32_t z1)
        {
            // Corners already inside the narrow band means a coarse skip cannot
            // fire on the samples we have (a gyroid is the usual case). The
            // original loop then runs once; those corner samples stay cached.
            bool bAllInBand = true;
            for (int ix = 0; ix < 2 && bAllInBand; ix++)
            for (int iy = 0; iy < 2 && bAllInBand; iy++)
            for (int iz = 0; iz < 2 && bAllInBand; iz++)
            {
                int32_t x = ix ? x1 : x0;
                int32_t y = iy ? y1 : y0;
                int32_t z = iz ? z1 : z0;
                if (std::fabs(fAt(x, y, z, true)) >= m_fB)
                    bAllInBand = false;
            }
            if (bAllInBand)
            {
                m_oDense.push_back(Box{x0, x1, y0, y1, z0, z1});
                auto oAccess = m_poSelf->m_roGrid->getAccessor();
                ApplyDense(&oAccess, m_oDense.back());
                return;
            }

            Visit(x0, x1, y0, y1, z0, z1);

            for (const Box& oBox : m_oInside)
            {
                openvdb::Coord xyzMin(oBox.x0, oBox.y0, oBox.z0);
                openvdb::Coord xyzMax(oBox.x1, oBox.y1, oBox.z1);
                m_poSelf->m_roGrid->fill(openvdb::CoordBBox(xyzMin, xyzMax), -m_fB, false);
            }

            auto oAccess = m_poSelf->m_roGrid->getAccessor();
            for (const Box& oBox : m_oDense)
                ApplyDense(&oAccess, oBox);
            for (const Box& oBox : m_oOutsideFix)
                ApplyOutsideFix(&oAccess, oBox);
        }

        float fAt(int32_t x, int32_t y, int32_t z, bool bStore)
        {
            Key oKey{x, y, z};
            auto it = m_oCache.find(oKey);
            if (it != m_oCache.end())
                return it->second;

            Vector3 vecSample = m_poSelf->oVoxelSize().vecToMM(Coord(x, y, z));
            float fValue = (*m_pfn)(&vecSample);
            m_nCalls++;
            if (bStore)
                m_oCache.emplace(oKey, fValue);
            return fValue;
        }

        static float fDist(const Probe& a, const Probe& b, float fH)
        {
            float dx = float(a.x - b.x) * fH;
            float dy = float(a.y - b.y) * fH;
            float dz = float(a.z - b.z) * fH;
            return std::sqrt(dx * dx + dy * dy + dz * dz);
        }

        bool bLipschitz(const Probe* p, int n) const
        {
            for (int i = 0; i < n; i++)
            for (int j = i + 1; j < n; j++)
            {
                float fD = fDist(p[i], p[j], m_fH);
                if (fD <= 0.0f)
                    continue;
                if (std::fabs(p[i].value - p[j].value) > kLipschitz * fD + 1.0e-3f)
                    return false;
            }
            return true;
        }

        void fRange(const Probe* p, int n,
                    int32_t x0, int32_t y0, int32_t z0,
                    int32_t x1, int32_t y1, int32_t z1,
                    float* pfMin, float* pfMax, float* pfDiag) const
        {
            float fMin = p[0].value;
            float fMax = p[0].value;
            for (int i = 1; i < n; i++)
            {
                fMin = std::min(fMin, p[i].value);
                fMax = std::max(fMax, p[i].value);
            }
            float dx = float(x1 - x0) * m_fH;
            float dy = float(y1 - y0) * m_fH;
            float dz = float(z1 - z0) * m_fH;
            *pfMin = fMin;
            *pfMax = fMax;
            *pfDiag = std::sqrt(dx * dx + dy * dy + dz * dz);
        }

        void Visit(int32_t x0, int32_t x1, int32_t y0, int32_t y1, int32_t z0, int32_t z1)
        {
            if (x0 > x1 || y0 > y1 || z0 > z1)
                return;

            const int32_t sx = x1 - x0 + 1;
            const int32_t sy = y1 - y0 + 1;
            const int32_t sz = z1 - z0 + 1;
            const int32_t nSide = std::max(sx, std::max(sy, sz));

            Probe oProbe[kProbeCap];
            int nProbe = 0;
            auto fAdd = [&](int32_t x, int32_t y, int32_t z)
            {
                if (x < x0 || x > x1 || y < y0 || y > y1 || z < z0 || z > z1)
                    return;
                for (int i = 0; i < nProbe; i++)
                {
                    if (oProbe[i].x == x && oProbe[i].y == y && oProbe[i].z == z)
                        return;
                }
                if (nProbe >= kProbeCap)
                    return;
                oProbe[nProbe++] = Probe{x, y, z, fAt(x, y, z, true)};
            };

            fAdd(x0, y0, z0); fAdd(x1, y0, z0); fAdd(x0, y1, z0); fAdd(x1, y1, z0);
            fAdd(x0, y0, z1); fAdd(x1, y0, z1); fAdd(x0, y1, z1); fAdd(x1, y1, z1);

            bool bLip = bLipschitz(oProbe, nProbe);
            float fMin = 0.0f, fMax = 0.0f, fDiag = 0.0f;
            fRange(oProbe, nProbe, x0, y0, z0, x1, y1, z1, &fMin, &fMax, &fDiag);
            const float fSafety = (nSide >= 8) ? 2.0f : 1.0f;
            const float fReach = fSafety * kLipschitz * fDiag;
            bool bOutside = bLip && (fMin - fReach >= m_fB + kSlack);
            bool bInside = bLip && (fMax + fReach <= -m_fB - kSlack);

            if (bOutside || bInside)
            {
                const int32_t cx = x0 + (x1 - x0) / 2;
                const int32_t cy = y0 + (y1 - y0) / 2;
                const int32_t cz = z0 + (z1 - z0) / 2;
                fAdd(cx, cy, cz);
                fAdd(cx, y0, cz); fAdd(cx, y1, cz);
                fAdd(x0, cy, cz); fAdd(x1, cy, cz);
                fAdd(cx, cy, z0); fAdd(cx, cy, z1);
                if (nSide >= 8)
                {
                    const int32_t ax[2] = { x0 + (cx - x0) / 2, cx + (x1 - cx) / 2 };
                    const int32_t ay[2] = { y0 + (cy - y0) / 2, cy + (y1 - cy) / 2 };
                    const int32_t az[2] = { z0 + (cz - z0) / 2, cz + (z1 - cz) / 2 };
                    for (int ix = 0; ix < 2; ix++)
                    for (int iy = 0; iy < 2; iy++)
                    for (int iz = 0; iz < 2; iz++)
                        fAdd(ax[ix], ay[iy], az[iz]);
                }
                bLip = bLipschitz(oProbe, nProbe);
                fRange(oProbe, nProbe, x0, y0, z0, x1, y1, z1, &fMin, &fMax, &fDiag);
                const float fReach2 = fSafety * kLipschitz * fDiag;
                bOutside = bLip && (fMin - fReach2 >= m_fB + kSlack);
                bInside = bLip && (fMax + fReach2 <= -m_fB - kSlack);
            }

            if (bOutside && bInside)
            {
                bOutside = false;
                bInside = false;
            }

            if (bOutside)
            {
                if (!m_bEmptyTree)
                    m_oOutsideFix.push_back(Box{x0, x1, y0, y1, z0, z1});
                return;
            }
            if (bInside)
            {
                m_oInside.push_back(Box{x0, x1, y0, y1, z0, z1});
                return;
            }
            if (nSide > 4)
            {
                Subdivide(x0, x1, y0, y1, z0, z1);
                return;
            }
            m_oDense.push_back(Box{x0, x1, y0, y1, z0, z1});
        }

        void Subdivide(int32_t x0, int32_t x1, int32_t y0, int32_t y1, int32_t z0, int32_t z1)
        {
            int32_t xs[2][2];
            int32_t ys[2][2];
            int32_t zs[2][2];
            int nx = 1, ny = 1, nz = 1;
            xs[0][0] = x0; xs[0][1] = x1;
            ys[0][0] = y0; ys[0][1] = y1;
            zs[0][0] = z0; zs[0][1] = z1;
            if (x1 > x0)
            {
                int32_t m = x0 + (x1 - x0) / 2;
                xs[0][0] = x0; xs[0][1] = m;
                xs[1][0] = m + 1; xs[1][1] = x1;
                nx = 2;
            }
            if (y1 > y0)
            {
                int32_t m = y0 + (y1 - y0) / 2;
                ys[0][0] = y0; ys[0][1] = m;
                ys[1][0] = m + 1; ys[1][1] = y1;
                ny = 2;
            }
            if (z1 > z0)
            {
                int32_t m = z0 + (z1 - z0) / 2;
                zs[0][0] = z0; zs[0][1] = m;
                zs[1][0] = m + 1; zs[1][1] = z1;
                nz = 2;
            }
            for (int ix = 0; ix < nx; ix++)
            for (int iy = 0; iy < ny; iy++)
            for (int iz = 0; iz < nz; iz++)
            {
                Visit(xs[ix][0], xs[ix][1], ys[iy][0], ys[iy][1], zs[iz][0], zs[iz][1]);
            }
        }

        void ApplyDense(FloatGrid::Accessor* poAccess, const Box& oBox)
        {
            for (int32_t x = oBox.x0; x <= oBox.x1; x++)
            for (int32_t y = oBox.y0; y <= oBox.y1; y++)
            for (int32_t z = oBox.z0; z <= oBox.z1; z++)
            {
                openvdb::Coord xyz(x, y, z);
                float fSample = fAt(x, y, z, false);
                float fValue = std::min(fSample, poAccess->getValue(xyz));
                SetSdValue(poAccess, xyz, m_fB, fValue);
            }
        }

        void ApplyOutsideFix(FloatGrid::Accessor* poAccess, const Box& oBox)
        {
            // f >= background everywhere in this box. min(f, existing) is
            // existing when existing <= background, and clamps to +background
            // otherwise. Match that without another callback.
            for (int32_t x = oBox.x0; x <= oBox.x1; x++)
            for (int32_t y = oBox.y0; y <= oBox.y1; y++)
            for (int32_t z = oBox.z0; z <= oBox.z1; z++)
            {
                openvdb::Coord xyz(x, y, z);
                float fExisting = poAccess->getValue(xyz);
                if (fExisting <= m_fB)
                {
                    bool bShouldOn = std::fabs(fExisting) < m_fB;
                    float fShould = std::clamp(fExisting, -m_fB, m_fB);
                    if (poAccess->isValueOn(xyz) != bShouldOn || poAccess->getValue(xyz) != fShould)
                        SetSdValue(poAccess, xyz, m_fB, fExisting);
                }
                else if (poAccess->isValueOn(xyz) || poAccess->getValue(xyz) != m_fB)
                {
                    SetSdValue(poAccess, xyz, m_fB, m_fB);
                }
            }
        }
    };

    static void SetSdValue( FloatGrid::Accessor* poAccess,
                            openvdb::Coord xyz,
                            float fBackground,
                            float fValue)
    {
        poAccess->setValue(xyz, std::clamp( fValue,
                                            -fBackground,
                                            fBackground));
        
        if (std::abs(fValue) >= fBackground)
            poAccess->setValueOff(xyz);
    }
    
    static bool bHasValidPicoGKTransform(const FloatGrid::Ptr roGrid)
    {
        if (!roGrid->transform().isLinear())
            return false;
            
        if (roGrid->voxelSize().x() != roGrid->voxelSize().y())
            return false;
        
        if (roGrid->voxelSize().x() != roGrid->voxelSize().z())
            return false;
        
        return true;
    }
    
};

} // PicoGK namespace

#endif // PICOGKVDBVOXELS_H_
