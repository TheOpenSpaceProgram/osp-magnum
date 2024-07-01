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

/**
 * @file
 * @brief Functions and data for subdividing a SubdivTriangleSkeleton
 */
#pragma once

#include "skeleton.h"
#include "geometry.h"

namespace planeta
{

struct SubdivScratchpadLevel
{
    std::vector<planeta::SkTriId> distanceTestProcessing;
    std::vector<planeta::SkTriId> distanceTestNext;
};


/**
 * @brief Temporary data needed to subdivide/unsubdivide a SkeletonVertexData
 *
 * This is intended to be kept around to avoid reallocations
 */
struct SkeletonSubdivScratchpad
{
    using UserData_t = std::array<void*, 4>;
    using OnUnsubdivideFunc_t = void (*)(SkTriId, SkeletonTriangle&, SubdivTriangleSkeleton&, SkeletonVertexData&, UserData_t) noexcept;
    using OnSubdivideFunc_t   = void (*)(SkTriId, SkTriGroupId, std::array<SkVrtxId, 3>, std::array<osp::MaybeNewId<SkVrtxId>, 3>, SubdivTriangleSkeleton&, SkeletonVertexData&, UserData_t) noexcept;

    void resize(SubdivTriangleSkeleton &rSkel);

    std::array<double, gc_maxSubdivLevels> distanceThresholdSubdiv{{}};
    std::array<double, gc_maxSubdivLevels> distanceThresholdUnsubdiv{{}};

    std::array<SubdivScratchpadLevel, gc_maxSubdivLevels> levels;

    /// Used to record which Skeleton triangles have already been distance-checked. Used for both
    /// subdividing and unsubdividing
    lgrn::IdSetStl<SkTriId> distanceTestDone;

    lgrn::IdSetStl<SkTriId> tryUnsubdiv;
    lgrn::IdSetStl<SkTriId> cantUnsubdiv;

    /// Non-subdivided triangles recently added, excluding intermediate triangles removed
    /// directly after creation.
    lgrn::IdSetStl<SkTriId> surfaceAdded;
    /// Non-subdivided triangles recently removed, excluding intermediate triangles removed
    /// directly after creation.
    lgrn::IdSetStl<SkTriId> surfaceRemoved;

    std::uint8_t levelNeedProcess   {};

    OnSubdivideFunc_t onSubdiv      {nullptr};
    UserData_t onSubdivUserData     {{nullptr, nullptr, nullptr, nullptr}};

    OnUnsubdivideFunc_t onUnsubdiv  {nullptr};
    UserData_t onUnsubdivUserData   {{nullptr, nullptr, nullptr, nullptr}};

    osp::Vector3l viewerPosition;

    std::uint32_t distanceCheckCount{};
};


/**
 * @brief Selects triangles (within a subdiv level) that are too far away from pos
 *
 * Populates SubdivScratchpad::tryUnsubdiv
 */
void unsubdivide_select_by_distance(
        std::uint8_t                    lvl,
        osp::Vector3l                   pos,
        SubdivTriangleSkeleton    const &rSkel,
        SkeletonVertexData        const &rSkData,
        SkeletonSubdivScratchpad        &rSP);

/**
 * @brief Tests which triangles in SubdivScratchpad::tryUnsubdiv are not allowed to un-subdivide
 *
 * Populates SubdivScratchpad::cantUnsubdiv
 */
void unsubdivide_deselect_invariant_violations(
        std::uint8_t                    lvl,
        SubdivTriangleSkeleton    const &rSkel,
        SkeletonVertexData        const &rSkData,
        SkeletonSubdivScratchpad        &rSP);

/**
 * @brief Performs unsubdivision on triangles in scratchpad's tryUnsubdiv and not in cantUnsubdiv
 */
void unsubdivide_level(
        std::uint8_t                    lvl,
        SubdivTriangleSkeleton          &rSkel,
        SkeletonVertexData              &rSkData,
        SkeletonSubdivScratchpad        &rSP);

/**
 * @brief Subdivide a triangle forming a group of 4 new triangles on the next subdiv level
 *
 * May recursively call other subdivisions in same or level to enforce invariants.
 */
SkTriGroupId subdivide(
        SkTriId                         sktriId,
        SkeletonTriangle&               rSkTri,
        std::uint8_t                    lvl,
        bool                            hasNextLevel,
        SubdivTriangleSkeleton          &rSkel,
        SkeletonVertexData              &rSkData,
        SkeletonSubdivScratchpad        &rSP);

/**
 * @brief Subdivide all triangles (within a subdiv level) too close to pos
 */
void subdivide_level_by_distance(
        osp::Vector3l                   pos,
        std::uint8_t                    lvl,
        SubdivTriangleSkeleton          &rSkel,
        SkeletonVertexData              &rSkData,
        SkeletonSubdivScratchpad        &rSP);

} // namespace planeta
