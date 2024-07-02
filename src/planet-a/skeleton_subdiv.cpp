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
#include "skeleton_subdiv.h"

using osp::Vector3;
using osp::Vector3l;

namespace planeta
{

void SkeletonSubdivScratchpad::resize(SubdivTriangleSkeleton &rSkel)
{
    auto const triCapacity = rSkel.tri_group_ids().capacity() * 4;

    distanceTestDone.resize(triCapacity);
    tryUnsubdiv     .resize(triCapacity);
    cantUnsubdiv    .resize(triCapacity);
    surfaceAdded    .resize(triCapacity);
    surfaceRemoved  .resize(triCapacity);
}

void unsubdivide_select_by_distance(
        std::uint8_t             const lvl,
        osp::Vector3l            const pos,
        SubdivTriangleSkeleton   const &rSkel,
        SkeletonVertexData       const &rSkData,
        SkeletonSubdivScratchpad       &rSP)
{
    SubdivTriangleSkeleton::Level const& rLvl   = rSkel.levels[lvl];
    SubdivScratchpadLevel&               rLvlSP = rSP  .levels[lvl];

    auto const maybe_distance_check = [&rSkel, &rSP, &rLvl, &rLvlSP] (SkTriId const sktriId)
    {
        if (rSP.distanceTestDone.contains(sktriId))
        {
            return; // Already checked
        }

        SkTriGroupId const childrenId = rSkel.tri_at(sktriId).children;
        if ( ! childrenId.has_value() )
        {
            return; // Must be subdivided to be considered for unsubdivision lol
        }

        SkTriGroup const& children = rSkel.tri_group_at(childrenId);
        if (   children.triangles[0].children.has_value()
            || children.triangles[1].children.has_value()
            || children.triangles[2].children.has_value()
            || children.triangles[3].children.has_value() )
        {
            return; // For parents to unsubdivide, all children must be unsubdivided too
        }

        rLvlSP.distanceTestNext.push_back(sktriId);
        rSP.distanceTestDone.insert(sktriId);
    };

    // Use a floodfill-style algorithm to avoid needing to check every triangle

    // Initial seed for floodfill are all subdivided triangles that neighbor a non-subdivided one
    for (SkTriId const sktriId : rLvl.hasNonSubdivedNeighbor)
    {
        maybe_distance_check(sktriId);
    }

    while (rLvlSP.distanceTestNext.size() != 0)
    {
        std::swap(rLvlSP.distanceTestProcessing, rLvlSP.distanceTestNext);
        rLvlSP.distanceTestNext.clear();

        for (SkTriId const sktriId : rLvlSP.distanceTestProcessing)
        {
            Vector3l const center = rSkData.centers[sktriId];
            bool const tooFar = ! osp::is_distance_near(pos, center, rSP.distanceThresholdUnsubdiv[lvl]);

            LGRN_ASSERTM(rSkel.tri_at(sktriId).children.has_value(),
                         "Non-subdivided triangles must not be added to distance test.");

            if (tooFar)
            {
                // All checks passed
                rSP.tryUnsubdiv.insert(sktriId);

                // Floodfill by checking neighbors next
                SkeletonTriangle const& sktri = rSkel.tri_at(sktriId);
                for (SkTriId const neighbor : sktri.neighbors)
                {
                    if (neighbor.has_value())
                    {
                        maybe_distance_check(neighbor);
                    }
                }
            }
        }
    }
}

void unsubdivide_deselect_invariant_violations(
        std::uint8_t              const lvl,
        SubdivTriangleSkeleton    const &rSkel,
        SkeletonVertexData        const &rSkData,
        SkeletonSubdivScratchpad        &rSP)
{
    SubdivTriangleSkeleton::Level const& rLvl  = rSkel.levels[lvl];
    SubdivScratchpadLevel&        rLvlSP = rSP .levels[lvl];

    auto const violates_invariants = [&rSkel, &rLvl, &rSP] (SkTriId const sktriId, SkeletonTriangle const& sktri) noexcept -> bool
    {
        int subdivedNeighbors = 0;
        for (SkTriId const neighbor : sktri.neighbors)
        {
            if (!neighbor.has_value())
            {
                continue;
            }

            SkeletonTriangle const &rNeighbor = rSkel.tri_at(neighbor);
            // Pretend neighbor is unsubdivided when it's in tryUnsubdiv, overrided
            // by cantUnsubdiv
            if (rNeighbor.children.has_value()
                && (  ! rSP.tryUnsubdiv .contains(neighbor)
                     || rSP.cantUnsubdiv.contains(neighbor) ) )
            {
                // Neighbor is subdivided
                ++subdivedNeighbors;

                // Check Invariant B
                int        const  neighborEdge  = rNeighbor.find_neighbor_index(sktriId);
                SkTriGroup const& neighborGroup = rSkel.tri_group_at(rNeighbor.children);

                switch (neighborEdge)
                {
                case 0:
                    if (neighborGroup.triangles[0].children.has_value()) { return true; }
                    if (neighborGroup.triangles[1].children.has_value()) { return true; }
                    break;
                case 1:
                    if (neighborGroup.triangles[1].children.has_value()) { return true; }
                    if (neighborGroup.triangles[2].children.has_value()) { return true; }
                    break;
                case 2:
                    if (neighborGroup.triangles[2].children.has_value()) { return true; }
                    if (neighborGroup.triangles[0].children.has_value()) { return true; }
                    break;
                default:
                    LGRN_ASSERTM(false, "This should never happen. Triangles only have 3 sides!");
                }
            }
        }

        // Invariant A
        if (subdivedNeighbors >= 2)
        {
            return true;
        }

        return false;
    };

    auto const check_recurse = [&violates_invariants, &rSkel, &rLvl, &rSP] (auto const& self, SkTriId const sktriId) -> void
    {
        SkeletonTriangle const& sktri = rSkel.tri_at(sktriId);

        if ( ! violates_invariants(sktriId, sktri) )
        {
            return;
        }

        rSP.cantUnsubdiv.insert(sktriId);

        // Recurse into neighbors if they're also tryUnsubdiv
        for (SkTriId const neighbor : sktri.neighbors)
        {
            if (rSP.tryUnsubdiv.contains(neighbor) && ! rSP.cantUnsubdiv.contains(neighbor))
            {
                self(self, neighbor);
            }
        }
    };

    for (SkTriId const sktriId : rSP.tryUnsubdiv)
    {
        if ( ! rSP.cantUnsubdiv.contains(sktriId) )
        {
            check_recurse(check_recurse, sktriId);
        }
    }
}

void unsubdivide_level(
        std::uint8_t          const lvl,
        SubdivTriangleSkeleton      &rSkel,
        SkeletonVertexData          &rSkData,
        SkeletonSubdivScratchpad    &rSP)
{
    auto const wont_unsubdivide = [&rSP] (SkTriId const sktriId) -> bool
    {
        return ( ! rSP.tryUnsubdiv.contains(sktriId) || rSP.cantUnsubdiv.contains(sktriId) );
    };

    SubdivTriangleSkeleton::Level   &rLvl   = rSkel.levels[lvl];
    SubdivScratchpadLevel           &rLvlSP = rSP  .levels[lvl];

    for (SkTriId const sktriId : rSP.tryUnsubdiv)
    {
        if ( rSP.cantUnsubdiv.contains(sktriId) )
        {
            continue;
        }

        // All checks passed, 100% confirmed sktri will be unsubdivided
        SkeletonTriangle &rTri = rSkel.tri_at(sktriId);

        LGRN_ASSERT(!rLvl.hasSubdivedNeighbor.contains(sktriId));
        for (SkTriId const neighborId : rTri.neighbors)
        {
            if ( ! ( neighborId.has_value() && wont_unsubdivide(neighborId) ) )
            {
                continue;
            }

            SkeletonTriangle const& rNeighborTri = rSkel.tri_at(neighborId);
            if ( rNeighborTri.children.has_value() )
            {
                rLvl.hasNonSubdivedNeighbor.insert(neighborId);
                rLvl.hasSubdivedNeighbor.insert(sktriId);
            }
            else
            {
                auto const neighbor_has_subdivided_neighbor = [sktriId, &wont_unsubdivide, &rSkel]
                        (SkTriId const neighborNeighborId)
                {
                    return    neighborNeighborId.has_value()
                           && neighborNeighborId != sktriId
                           && wont_unsubdivide(neighborNeighborId)
                           && rSkel.is_tri_subdivided(neighborNeighborId);
                };

                if (   neighbor_has_subdivided_neighbor(rNeighborTri.neighbors[0])
                    || neighbor_has_subdivided_neighbor(rNeighborTri.neighbors[1])
                    || neighbor_has_subdivided_neighbor(rNeighborTri.neighbors[2]) )
                {
                    rLvl.hasSubdivedNeighbor.insert(neighborId);
                }
                else
                {
                    rLvl.hasSubdivedNeighbor.erase(neighborId);
                }
            }
        }

        rLvl.hasNonSubdivedNeighbor.erase(sktriId);

        LGRN_ASSERT( ! rLvl.hasSubdivedNeighbor.contains(tri_id(rTri.children, 0)) );
        LGRN_ASSERT( ! rLvl.hasSubdivedNeighbor.contains(tri_id(rTri.children, 1)) );
        LGRN_ASSERT( ! rLvl.hasSubdivedNeighbor.contains(tri_id(rTri.children, 2)) );
        LGRN_ASSERT( ! rLvl.hasSubdivedNeighbor.contains(tri_id(rTri.children, 3)) );

        LGRN_ASSERT(!rSP.surfaceAdded.contains(sktriId));
        rSP.surfaceAdded.insert(sktriId);

        // If child is in surfaceAdded. This means it was just recently unsubdivided.
        // It will be removed right away and is an intermediate step, so don't include it in
        // surfaceAdded or surfaceRemoved.
        auto const check_surface = [&rSP] (SkTriId const child)
        {
            if (rSP.surfaceAdded.contains(child))
            {
                rSP.surfaceAdded.erase(child);
            }
            else
            {
                rSP.surfaceRemoved.insert(child);
            }
        };
        check_surface(tri_id(rTri.children, 0));
        check_surface(tri_id(rTri.children, 1));
        check_surface(tri_id(rTri.children, 2));
        check_surface(tri_id(rTri.children, 3));

        rSP.onUnsubdiv(sktriId, rTri, rSkel, rSkData, rSP.onUnsubdivUserData);

        rSkel.tri_unsubdiv(sktriId, rTri);
    }

    rSP.tryUnsubdiv.clear();
    rSP.cantUnsubdiv.clear();
}

SkTriGroupId subdivide(
        SkTriId                     sktriId,
        SkeletonTriangle&           rSkTri,
        std::uint8_t          const lvl,
        bool                        hasNextLevel,
        SubdivTriangleSkeleton      &rSkel,
        SkeletonVertexData          &rSkData,
        SkeletonSubdivScratchpad    &rSP)
{
    LGRN_ASSERTM(rSkel.tri_group_ids().exists(tri_group_id(sktriId)), "SkTri does not exist");
    LGRN_ASSERTM(!rSkTri.children.has_value(), "Already subdivided");

    SubdivTriangleSkeleton::Level& rLvl = rSkel.levels[lvl];

    std::array<SkTriId,  3> const neighbors = { rSkTri.neighbors[0], rSkTri.neighbors[1], rSkTri.neighbors[2] };
    std::array<SkVrtxId, 3> const corners   = { rSkTri .vertices[0], rSkTri .vertices[1], rSkTri .vertices[2] };

    // Create or get vertices between the 3 corners
    std::array<osp::MaybeNewId<SkVrtxId>, 3> const middlesNew = rSkel.vrtx_create_middles(corners);
    std::array<SkVrtxId, 3>                  const middles    = {middlesNew[0].id, middlesNew[1].id, middlesNew[2].id};

    // Actually do the subdivision ( create a new group (4 triangles) as children )
    // manual borrow checker hint: rSkTri becomes invalid here >:)
    auto const [groupId, rGroup] = rSkel.tri_subdiv(sktriId, rSkTri, middles);

    rSkData.resize(rSkel);
    rSP.resize(rSkel);

    rSP.onSubdiv(sktriId, groupId, corners, middlesNew, rSkel, rSkData, rSP.onSubdivUserData);

    if (hasNextLevel)
    {
        rSP.levels[lvl+1].distanceTestNext.insert(rSP.levels[lvl+1].distanceTestNext.end(), {
            tri_id(groupId, 0),
            tri_id(groupId, 1),
            tri_id(groupId, 2),
            tri_id(groupId, 3),
        });
        rSP.levels[lvl+1].distanceTestNext.insert(rSP.levels[lvl+1].distanceTestNext.end(), {
            tri_id(groupId, 0),
            tri_id(groupId, 1),
            tri_id(groupId, 2),
            tri_id(groupId, 3),
        });
        rSP.distanceTestDone.insert(tri_id(groupId, 0));
        rSP.distanceTestDone.insert(tri_id(groupId, 1));
        rSP.distanceTestDone.insert(tri_id(groupId, 2));
        rSP.distanceTestDone.insert(tri_id(groupId, 3));
    }

    // sktri is recently unsubdivided or newly added
    // It will be removed right away and is an intermediate step, so don't include it
    // in surfaceAdded or surfaceRemoved.
    if (rSP.surfaceAdded.contains(sktriId))
    {
        rSP.surfaceAdded.erase(sktriId);
    }
    else
    {
        rSP.surfaceRemoved.insert(sktriId);
    }
    rSP.surfaceAdded.insert(tri_id(groupId, 0));
    rSP.surfaceAdded.insert(tri_id(groupId, 1));
    rSP.surfaceAdded.insert(tri_id(groupId, 2));
    rSP.surfaceAdded.insert(tri_id(groupId, 3));

    // hasSubdivedNeighbor is only for Non-subdivided triangles
    rLvl.hasSubdivedNeighbor.erase(sktriId);

    bool hasNonSubdivNeighbor = false;

    // Check neighbours along all 3 edges
    for (int selfEdgeIdx = 0; selfEdgeIdx < 3; ++selfEdgeIdx)
    {
        SkTriId const neighborId = neighbors[selfEdgeIdx];
        if ( ! neighborId.has_value() )
        {
            continue; // Neighbor does not exist
        }

        SkeletonTriangle& rNeighbor = rSkel.tri_at(neighborId);
        if (rNeighbor.children.has_value())
        {
            // Assign bi-directional connection (neighbor's neighbor)
            int const neighborEdgeIdx = rNeighbor.find_neighbor_index(sktriId);

            SkTriGroup &rNeighborGroup = rSkel.tri_group_at(rNeighbor.children);

            auto const [selfEdge, neighborEdge]
                = rSkel.tri_group_set_neighboring(
                    {.id = groupId,            .rGroup = rGroup,         .edge = selfEdgeIdx},
                    {.id = rNeighbor.children, .rGroup = rNeighborGroup, .edge = neighborEdgeIdx});

            if (hasNextLevel)
            {
                SubdivTriangleSkeleton::Level &rNextLvl = rSkel.levels[lvl+1];
                if (rSkel.tri_at(neighborEdge.childB).children.has_value())
                {
                    rNextLvl.hasSubdivedNeighbor.insert(selfEdge.childA);
                    rNextLvl.hasNonSubdivedNeighbor.insert(neighborEdge.childB);
                }

                if (rSkel.tri_at(neighborEdge.childA).children.has_value())
                {
                    rNextLvl.hasSubdivedNeighbor.insert(selfEdge.childB);
                    rNextLvl.hasNonSubdivedNeighbor.insert(neighborEdge.childA);
                }
            }

            auto const neighbor_has_subdivided_neighbor = [sktriId, &rSkel]
                    (SkTriId const neighborNeighborId)
            {
                return    neighborNeighborId.has_value()
                       && neighborNeighborId != sktriId
                       && ! rSkel.is_tri_subdivided(neighborNeighborId);
            };

            if (   neighbor_has_subdivided_neighbor(rNeighbor.neighbors[0])
                || neighbor_has_subdivided_neighbor(rNeighbor.neighbors[1])
                || neighbor_has_subdivided_neighbor(rNeighbor.neighbors[2]))
            {
                rLvl.hasNonSubdivedNeighbor.insert(neighborId);
            }
            else
            {
                rLvl.hasNonSubdivedNeighbor.erase(neighborId);
            }
        }
        else
        {
            // Neighbor is not subdivided
            hasNonSubdivNeighbor = true;
            rLvl.hasSubdivedNeighbor.insert(neighborId);
        }
    }

    if (hasNonSubdivNeighbor)
    {
        rLvl.hasNonSubdivedNeighbor.insert(sktriId);
    }
    else
    {
        rLvl.hasNonSubdivedNeighbor.erase(sktriId);
    }

    // Check and immediately fix Invariant A and B violations.
    // This will subdivide other triangles recursively if found.
    // Invariant A: if neighbour has 2 subdivided neighbours, subdivide it too
    // Invariant B: for corner children (childIndex != 3), parent's neighbours must be subdivided
    for (int selfEdgeIdx = 0; selfEdgeIdx < 3; ++selfEdgeIdx)
    {
        SkTriId const neighborId = rSkel.tri_at(sktriId).neighbors[selfEdgeIdx];
        if (neighborId.has_value())
        {
            SkeletonTriangle& rNeighbor = rSkel.tri_at(neighborId);
            if ( rNeighbor.children.has_value() )
            {
                continue; // Neighbor already subdivided. nothing to do
            }

            // Check Invariant A by seeing if any other neighbor's neighbors are subdivided
            auto const is_other_subdivided = [&rSkel = rSkel, &rNeighbor, sktriId] (SkTriId const other)
            {
                return    other != sktriId
                       && other.has_value()
                       && rSkel.is_tri_subdivided(other);
            };

            if (   is_other_subdivided(rNeighbor.neighbors[0])
                || is_other_subdivided(rNeighbor.neighbors[1])
                || is_other_subdivided(rNeighbor.neighbors[2]) )
            {
                // Invariant A violation, more than 2 neighbors subdivided
                subdivide(neighborId, rSkel.tri_at(neighborId), lvl, hasNextLevel, rSkel, rSkData, rSP);
                rSP.distanceTestDone.insert(neighborId);
            }
            else if (!rSP.distanceTestDone.contains(neighborId))
            {
                // No Invariant A violation, but floodfill distance-test instead
                rSP.levels[lvl].distanceTestNext.push_back(neighborId);
                rSP.distanceTestDone.insert(neighborId);
            }

        }
        else // Neighbour doesn't exist, its parent is not subdivided. Invariant B violation
        {
            LGRN_ASSERTM(tri_sibling_index(sktriId) != 3,
                         "Center triangles are always surrounded by their siblings");
            LGRN_ASSERTM(lvl != 0, "No level above level 0");

            SkTriId const parent = rSkel.tri_group_at(tri_group_id(sktriId)).parent;

            LGRN_ASSERTM(parent.has_value(), "bruh");

            auto const& parentNeighbors = rSkel.tri_at(parent).neighbors;

            LGRN_ASSERTM(parentNeighbors[selfEdgeIdx].has_value(),
                         "something screwed up XD");

            SkTriId const neighborParent = parentNeighbors[selfEdgeIdx].value();

            // Adds to ctx.rTerrain.levels[level-1].distanceTestNext
            subdivide(neighborParent, rSkel.tri_at(neighborParent), lvl-1, true, rSkel, rSkData, rSP);
            rSP.distanceTestDone.insert(neighborParent);

            rSP.levelNeedProcess = std::min<uint8_t>(rSP.levelNeedProcess, lvl-1);
        }
    }

    return groupId;
}


void subdivide_level_by_distance(
        Vector3l              const pos,
        std::uint8_t          const lvl,
        SubdivTriangleSkeleton      &rSkel,
        SkeletonVertexData          &rSkData,
        SkeletonSubdivScratchpad    &rSP)
{
    LGRN_ASSERT(lvl == rSP.levelNeedProcess);

    SubdivTriangleSkeleton::Level &rLvl   = rSkel.levels[lvl];
    SubdivScratchpadLevel         &rLvlSP = rSP  .levels[lvl];

    bool const hasNextLevel = lvl+1 < rSkel.levelMax;

    while ( ! rSP.levels[lvl].distanceTestNext.empty() )
    {
        std::swap(rLvlSP.distanceTestProcessing, rLvlSP.distanceTestNext);
        rLvlSP.distanceTestNext.clear();

        for (SkTriId const sktriId : rLvlSP.distanceTestProcessing)
        {
            Vector3l const center = rSkData.centers[sktriId];

            LGRN_ASSERT(rSP.distanceTestDone.contains(sktriId));
            bool const distanceNear = osp::is_distance_near(pos, center, rSP.distanceThresholdSubdiv[lvl]);
            ++rSP.distanceCheckCount;

            if (distanceNear)
            {
                SkeletonTriangle &rTri = rSkel.tri_at(sktriId);
                if (rTri.children.has_value())
                {
                    if (hasNextLevel)
                    {
                        SkTriGroupId const children = rTri.children;
                        rSP.levels[lvl+1].distanceTestNext.insert(rSP.levels[lvl+1].distanceTestNext.end(), {
                            tri_id(children, 0),
                            tri_id(children, 1),
                            tri_id(children, 2),
                            tri_id(children, 3),
                        });
                        rSP.distanceTestDone.insert(tri_id(children, 0));
                        rSP.distanceTestDone.insert(tri_id(children, 1));
                        rSP.distanceTestDone.insert(tri_id(children, 2));
                        rSP.distanceTestDone.insert(tri_id(children, 3));
                    }
                }
                else
                {
                    subdivide(sktriId, rTri, lvl, hasNextLevel, rSkel, rSkData, rSP);
                }
            }

            // Fix up Invariant B violations
            while (rSP.levelNeedProcess != lvl)
            {
                subdivide_level_by_distance(pos, rSP.levelNeedProcess, rSkel, rSkData, rSP);
            }
        }
    }

    LGRN_ASSERT(lvl == rSP.levelNeedProcess);
    ++rSP.levelNeedProcess;
}

} // namespace planeta
