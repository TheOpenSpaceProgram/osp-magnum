/**
 * Open Space Program
 * Copyright © 2019-2024 Open Space Program Project
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#pragma once

#include <osp/core/bitvector.h>
#include <osp/core/math_int64.h>

#include "SubdivSkeleton.h"
#include "SubdivTriangleMesh.h"

namespace planeta
{

inline constexpr std::size_t gc_maxSubdivLevels = 9;

/**
 * @brief A subdividable triangle mesh intended as a structural frame for detailed terrain mesh
 *
 * Uses int64 coordinates capable of representing entire planets.
 *
 * The included subdivision functionality constrains triangles under certain rules to support
 * seamless transitions between levels of detail for ChunkedTriangleMesh:
 *
 * * Rule A: Non-subdivided triangles with 2 subdivided neighbours are not allowed
 * * Rule B: Edges acting as a transition between levels of detail can only differ by 1 level
 *
 *
 * This is intended for spherical planets, but can easily be used for flat terrain or other
 * weirder shapes.
 */
struct TerrainSkeleton
{
    struct Level
    {
        /// Subdivided triangles that neighbor a non-subdivided one
        osp::BitVector_t hasNonSubdivedNeighbor;

        /// Non-subdivided triangles that neighbor a subdivided one
        osp::BitVector_t hasSubdivedNeighbor;
    };

    SubdivTriangleSkeleton skel;

    osp::KeyedVec<planeta::SkVrtxId, osp::Vector3l> positions;
    osp::KeyedVec<planeta::SkVrtxId, osp::Vector3>  normals;

    osp::KeyedVec<planeta::SkTriId,  osp::Vector3l> centers;

    std::array<Level, gc_maxSubdivLevels> levels;

    int scale{};
};

struct SubdivScratchpadLevel
{
    std::vector<planeta::SkTriId> distanceTestProcessing;
    std::vector<planeta::SkTriId> distanceTestNext;
};

/**
 * @brief Temporary data needed to subdivide/unsubdivide a TerrainSkeleton
 *
 * This can be kept around to avoid reallocations
 */
struct SubdivScratchpad
{
    using UserData_t = std::array<void*, 4>;
    using OnUnsubdivideFunc_t = void (*)(SkTriId, SkeletonTriangle&, TerrainSkeleton&, UserData_t) noexcept;
    using OnSubdivideFunc_t   = void (*)(SkTriId, SkTriGroupId, std::array<SkVrtxId, 3>, std::array<MaybeNewId<SkVrtxId>, 3>, TerrainSkeleton&, UserData_t) noexcept;

    void resize(TerrainSkeleton& rTrn);

    std::array<std::uint64_t, gc_maxSubdivLevels> distanceThresholdSubdiv;
    std::array<std::uint64_t, gc_maxSubdivLevels> distanceThresholdUnsubdiv;

    std::array<SubdivScratchpadLevel, gc_maxSubdivLevels> levels;

    osp::BitVector_t distanceTestDone;
    osp::BitVector_t tryUnsubdiv;
    osp::BitVector_t cantUnsubdiv;

    /// Non-subdivided triangles recently added, excluding intermediate triangles removed
    /// directly after creation.
    osp::BitVector_t surfaceAdded;
    /// Non-subdivided triangles recently removed, excluding intermediate triangles removed
    /// directly after creation.
    osp::BitVector_t surfaceRemoved;

    std::uint8_t levelNeedProcess = 7;
    std::uint8_t levelMax {7};

    OnSubdivideFunc_t onSubdiv      {nullptr};
    UserData_t onSubdivUserData     {{nullptr, nullptr, nullptr, nullptr}};

    OnUnsubdivideFunc_t onUnsubdiv  {nullptr};
    UserData_t onUnsubdivUserData   {{nullptr, nullptr, nullptr, nullptr}};

    std::uint32_t distanceCheckCount;
};


struct ChunkScratchpad
{
    ChunkVrtxSubdivLUT lut;

    osp::KeyedVec<ChunkId, ChunkStitch> stitchCmds;

    /// Newly added shared vertices, position needs to be copied from skeleton
    std::vector<SharedVrtxId>    sharedNewlyAdded;
};

/**
 * @brief Selects triangles (within a subdiv level) that are too far away from pos
 *
 * Populates SubdivScratchpad::tryUnsubdiv
 */
void unsubdivide_level_by_distance(std::uint8_t const lvl, osp::Vector3l const pos, TerrainSkeleton const& rTrn, SubdivScratchpad& rSP);

/**
 * @brief Tests which triangles in SubdivScratchpad::tryUnsubdiv are not allowed to un-subdivide
 *
 * Populates SubdivScratchpad::cantUnsubdiv
 */
void unsubdivide_level_check_rules(std::uint8_t const lvl, TerrainSkeleton const& rTrn, SubdivScratchpad& rSP);

/**
 * @brief Performs unsubdivision on triangles in scratchpad's tryUnsubdiv and not in cantUnsubdiv
 */
void unsubdivide_level(std::uint8_t const lvl, TerrainSkeleton& rTrn, SubdivScratchpad& rSP);

/**
 * @brief Subdivide a triangle forming a group of 4 new triangles on the next subdiv level
 *
 * May recursively call other subdivisions in same or level to enforce rules.
 */
SkTriGroupId subdivide(SkTriId sktriId, SkeletonTriangle& rSkTri, std::uint8_t lvl, bool hasNextLevel, TerrainSkeleton& rTrn, SubdivScratchpad& rSP);

/**
 * @brief Subdivide all triangles (within a subdiv level) too close to pos
 */
void subdivide_level_by_distance(osp::Vector3l const pos, std::uint8_t const lvl, TerrainSkeleton& rTrn, SubdivScratchpad& rSP);

/**
 * @brief Calculate center of a triangle given a spherical terrain mesh, writes to sktriCenter
 *
 * This accounts for the min/max height of terrain elevation
 */
void calc_sphere_tri_center(SkTriGroupId const groupId, TerrainSkeleton& rTrn, float const maxRadius, float const height);

/**
 * @brief Does a lot of asserts
 */
void debug_check_rules(TerrainSkeleton &rTrn);


} // namespace planeta
