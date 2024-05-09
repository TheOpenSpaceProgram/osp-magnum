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

using osp::bitvector_resize;
using osp::Vector3;
using osp::Vector3l;

namespace planeta
{

void SubdivScratchpad::resize(SubdivTriangleSkeleton &rSkel, SkeletonVertexData &rSkData)
{
    auto const triCapacity = rSkel.tri_group_ids().capacity() * 4;

    // Using centers as 'previous capacity' to detect a reallocation
    if (triCapacity != rSkData.centers.size())
    {
        // note: Since all of these are the same size, it feel practical to put them all in a
        //       single unique_ptr allocation, and access it with array views

        rSkData.centers.resize(triCapacity);

        bitvector_resize(this->  distanceTestDone,  triCapacity);
        bitvector_resize(this->  tryUnsubdiv,       triCapacity);
        bitvector_resize(this->  cantUnsubdiv,      triCapacity);
        bitvector_resize(this->  surfaceAdded,      triCapacity);
        bitvector_resize(this->  surfaceRemoved,    triCapacity);

        for (int lvl = 0; lvl < this->levelMax+1; ++lvl)
        {
            bitvector_resize(rSkel.levels[lvl].hasSubdivedNeighbor,       triCapacity);
            bitvector_resize(rSkel.levels[lvl].hasNonSubdivedNeighbor,    triCapacity);
        }
    }

    auto const vrtxCapacity = rSkel.vrtx_ids().capacity();
    if (vrtxCapacity != rSkData.positions.size())
    {
        rSkData.positions.resize(vrtxCapacity);
        rSkData.normals  .resize(vrtxCapacity);
    }
}

void unsubdivide_select_by_distance(
        std::uint8_t             const lvl,
        osp::Vector3l            const pos,
        SubdivTriangleSkeleton   const &rSkel,
        SkeletonVertexData       const &rSkData,
        SubdivScratchpad               &rSP)
{
    SubdivTriangleSkeleton::Level const& rLvl   = rSkel.levels[lvl];
    SubdivScratchpadLevel&               rLvlSP = rSP  .levels[lvl];

    auto const maybe_distance_check = [&rSkel, &rSP, &rLvl, &rLvlSP] (SkTriId const sktriId)
    {
        if (rSP.distanceTestDone.test(sktriId.value))
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
        rSP.distanceTestDone.set(sktriId.value);
    };

    // Use a floodfill-style algorithm to avoid needing to check every triangle

    // Initial seed for floodfill are all subdivided triangles that neighbor a non-subdivided one
    for (std::size_t const sktriInt : rLvl.hasNonSubdivedNeighbor.ones())
    {
        maybe_distance_check(SkTriId(sktriInt));
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
                rSP.tryUnsubdiv.set(sktriId.value);

                // Floodfill by checking neighbors next
                SkeletonTriangle const& sktri = rSkel.tri_at(sktriId);
                for (SkTriId const neighbor : sktri.neighbors)
                if (neighbor.has_value())
                {
                    maybe_distance_check(neighbor);
                }
            }
        }
    }
}

void unsubdivide_deselect_invariant_violations(
        std::uint8_t              const lvl,
        SubdivTriangleSkeleton    const &rSkel,
        SkeletonVertexData        const &rSkData,
        SubdivScratchpad                &rSP)
{
    SubdivTriangleSkeleton::Level const& rLvl  = rSkel.levels[lvl];
    SubdivScratchpadLevel&        rLvlSP = rSP .levels[lvl];

    auto const violates_invariants = [&rSkel, &rLvl, &rSP] (SkTriId const sktriId, SkeletonTriangle const& sktri) noexcept -> bool
    {
        int subdivedNeighbors = 0;
        for (SkTriId const neighbor : sktri.neighbors)
        if (neighbor.has_value())
        {
            SkeletonTriangle const &rNeighbor = rSkel.tri_at(neighbor);
            // Pretend neighbor is unsubdivided when it's in tryUnsubdiv, overrided
            // by cantUnsubdiv
            if (rNeighbor.children.has_value()
                && (  ! rSP.tryUnsubdiv .test(neighbor.value)
                     || rSP.cantUnsubdiv.test(neighbor.value) ) )
            {
                // Neighbor is subdivided
                ++subdivedNeighbors;

                // Check Invariant B
                int        const  neighborEdge  = rNeighbor.find_neighbor_index(sktriId);
                SkTriGroup const& neighborGroup = rSkel.tri_group_at(rNeighbor.children);

                switch (neighborEdge)
                {
                case 0:
                    if (neighborGroup.triangles[0].children.has_value()) return true;
                    if (neighborGroup.triangles[1].children.has_value()) return true;
                    break;
                case 1:
                    if (neighborGroup.triangles[1].children.has_value()) return true;
                    if (neighborGroup.triangles[2].children.has_value()) return true;
                    break;
                case 2:
                    if (neighborGroup.triangles[2].children.has_value()) return true;
                    if (neighborGroup.triangles[0].children.has_value()) return true;
                    break;
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

        if (violates_invariants(sktriId, sktri))
        {
            rSP.cantUnsubdiv.set(sktriId.value);

            // Recurse into neighbors if they're also tryUnsubdiv
            for (SkTriId const neighbor : sktri.neighbors)
            if (rSP.tryUnsubdiv.test(neighbor.value) && ! rSP.cantUnsubdiv.test(neighbor.value))
            {
                self(self, neighbor);
            }
        }
    };

    for (std::size_t const sktriInt : rSP.tryUnsubdiv.ones())
    {
        if ( ! rSP.cantUnsubdiv.test(sktriInt) )
        {
            check_recurse(check_recurse, SkTriId::from_index(sktriInt));
        }
    }
}

void unsubdivide_level(
        std::uint8_t          const lvl,
        SubdivTriangleSkeleton      &rSkel,
        SkeletonVertexData          &rSkData,
        SubdivScratchpad            &rSP)
{
    auto const wont_unsubdivide = [&rSP] (SkTriId const sktriId) -> bool
    {
        return ( ! rSP.tryUnsubdiv.test(sktriId.value) || rSP.cantUnsubdiv.test(sktriId.value) );
    };

    SubdivTriangleSkeleton::Level   &rLvl   = rSkel.levels[lvl];
    SubdivScratchpadLevel           &rLvlSP = rSP  .levels[lvl];

    for (std::size_t const sktriInt : rSP.tryUnsubdiv.ones())
    if ( ! rSP.cantUnsubdiv.test(sktriInt) )
    {
        // All checks passed, 100% confirmed sktri will be unsubdivided

        SkTriId const sktriId = SkTriId::from_index(sktriInt);
        SkeletonTriangle &rTri = rSkel.tri_at(sktriId);

        LGRN_ASSERT(!rLvl.hasSubdivedNeighbor.test(sktriInt));
        for (SkTriId const neighborId : rTri.neighbors)
        if ( neighborId.has_value() && wont_unsubdivide(neighborId) )
        {
            SkeletonTriangle const& rNeighborTri = rSkel.tri_at(neighborId);
            if ( rNeighborTri.children.has_value() )
            {
                rLvl.hasNonSubdivedNeighbor.set(neighborId.value);
                rLvl.hasSubdivedNeighbor.set(sktriInt);
            }
            else
            {
                bool neighborHasSubdivedNeighbor = false;
                for (SkTriId const neighborNeighborId : rNeighborTri.neighbors)
                if (   neighborNeighborId.has_value()
                    && neighborNeighborId != sktriId
                    && wont_unsubdivide(neighborNeighborId)
                    && rSkel.is_tri_subdivided(neighborNeighborId) )
                {
                    neighborHasSubdivedNeighbor = true;
                    break;
                }

                if (neighborHasSubdivedNeighbor)
                {
                    rLvl.hasSubdivedNeighbor.set(neighborId.value);
                }
                else
                {
                    rLvl.hasSubdivedNeighbor.reset(neighborId.value);
                }
            }
        }

        rLvl.hasNonSubdivedNeighbor.reset(sktriInt);

        LGRN_ASSERT( ! rLvl.hasSubdivedNeighbor.test(tri_id(rTri.children, 0).value) );
        LGRN_ASSERT( ! rLvl.hasSubdivedNeighbor.test(tri_id(rTri.children, 1).value) );
        LGRN_ASSERT( ! rLvl.hasSubdivedNeighbor.test(tri_id(rTri.children, 2).value) );
        LGRN_ASSERT( ! rLvl.hasSubdivedNeighbor.test(tri_id(rTri.children, 3).value) );

        LGRN_ASSERT(!rSP.surfaceAdded.test(sktriInt));
        rSP.surfaceAdded.set(sktriInt);

        // If child is in surfaceAdded. This means it was just recently unsubdivided.
        // It will be removed right away and is an intermediate step, so don't include it in
        // surfaceAdded or surfaceRemoved.
        auto const check_surface = [&rSP] (SkTriId const child)
        {
            if (rSP.surfaceAdded.test(child.value))
            {
                rSP.surfaceAdded.reset(child.value);
            }
            else
            {
                rSP.surfaceRemoved.set(child.value);
            }
        };
        check_surface(tri_id(rTri.children, 0));
        check_surface(tri_id(rTri.children, 1));
        check_surface(tri_id(rTri.children, 2));
        check_surface(tri_id(rTri.children, 3));

        rSP.onUnsubdiv(sktriId, rTri, rSkel, rSkData, rSP.onUnsubdivUserData);

        rSkel.tri_unsubdiv(SkTriId(sktriInt), rTri);
    }

    rSP.tryUnsubdiv.reset();
    rSP.cantUnsubdiv.reset();
}

SkTriGroupId subdivide(
        SkTriId                     sktriId,
        SkeletonTriangle&           rSkTri,
        std::uint8_t          const lvl,
        bool                        hasNextLevel,
        SubdivTriangleSkeleton      &rSkel,
        SkeletonVertexData          &rSkData,
        SubdivScratchpad            &rSP)
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

    rSP.resize(rSkel, rSkData);

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
        rSP.distanceTestDone.set(tri_id(groupId, 0).value);
        rSP.distanceTestDone.set(tri_id(groupId, 1).value);
        rSP.distanceTestDone.set(tri_id(groupId, 2).value);
        rSP.distanceTestDone.set(tri_id(groupId, 3).value);
    }

    // sktri is recently unsubdivided or newly added
    // It will be removed right away and is an intermediate step, so don't include it
    // in surfaceAdded or surfaceRemoved.
    if (rSP.surfaceAdded.test(sktriId.value))
    {
        rSP.surfaceAdded.reset(sktriId.value);
    }
    else
    {
        rSP.surfaceRemoved.set(sktriId.value);
    }
    rSP.surfaceAdded.set(tri_id(groupId, 0).value);
    rSP.surfaceAdded.set(tri_id(groupId, 1).value);
    rSP.surfaceAdded.set(tri_id(groupId, 2).value);
    rSP.surfaceAdded.set(tri_id(groupId, 3).value);

    // hasSubdivedNeighbor is only for Non-subdivided triangles
    rLvl.hasSubdivedNeighbor.reset(sktriId.value);

    bool hasNonSubdivNeighbor = false;

    // Check neighbours along all 3 edges
    for (int selfEdgeIdx = 0; selfEdgeIdx < 3; ++selfEdgeIdx)
    if ( SkTriId const neighborId = neighbors[selfEdgeIdx];
         neighborId.has_value() )
    {
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
                    rNextLvl.hasSubdivedNeighbor.set(selfEdge.childA.value);
                    rNextLvl.hasNonSubdivedNeighbor.set(neighborEdge.childB.value);
                }

                if (rSkel.tri_at(neighborEdge.childA).children.has_value())
                {
                    rNextLvl.hasSubdivedNeighbor.set(selfEdge.childB.value);
                    rNextLvl.hasNonSubdivedNeighbor.set(neighborEdge.childA.value);
                }
            }

            bool neighborHasNonSubdivedNeighbor = false;
            for (SkTriId const neighborNeighborId : rNeighbor.neighbors)
            if (   neighborNeighborId.has_value()
                && neighborNeighborId != sktriId
                && ! rSkel.is_tri_subdivided(neighborNeighborId) )
            {
                neighborHasNonSubdivedNeighbor = true;
                break;
            }

            if (neighborHasNonSubdivedNeighbor)
            {
                rLvl.hasNonSubdivedNeighbor.set(neighborId.value);
            }
            else
            {
                rLvl.hasNonSubdivedNeighbor.reset(neighborId.value);
            }
        }
        else
        {
            // Neighbor is not subdivided
            hasNonSubdivNeighbor = true;
            rLvl.hasSubdivedNeighbor.set(neighborId.value);
        }
    }

    if (hasNonSubdivNeighbor)
    {
        rLvl.hasNonSubdivedNeighbor.set(sktriId.value);
    }
    else
    {
        rLvl.hasNonSubdivedNeighbor.reset(sktriId.value);
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
                rSP.distanceTestDone.set(neighborId.value);
            }
            else if (!rSP.distanceTestDone.test(neighborId.value))
            {
                // No Invariant A violation, but floodfill distance-test instead
                rSP.levels[lvl].distanceTestNext.push_back(neighborId);
                rSP.distanceTestDone.set(neighborId.value);
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
            rSP.distanceTestDone.set(neighborParent.value);

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
        SubdivScratchpad            &rSP)
{
    LGRN_ASSERT(lvl == rSP.levelNeedProcess);

    SubdivTriangleSkeleton::Level &rLvl   = rSkel.levels[lvl];
    SubdivScratchpadLevel  &rLvlSP = rSP .levels[lvl];

    bool const hasNextLevel = (lvl+1 < rSP.levelMax);

    while ( ! rSP.levels[lvl].distanceTestNext.empty() )
    {
        std::swap(rLvlSP.distanceTestProcessing, rLvlSP.distanceTestNext);
        rLvlSP.distanceTestNext.clear();

        for (SkTriId const sktriId : rLvlSP.distanceTestProcessing)
        {
            Vector3l const center = rSkData.centers[sktriId];

            LGRN_ASSERT(rSP.distanceTestDone.test(sktriId.value));
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
                        rSP.distanceTestDone.set(tri_id(children, 0).value);
                        rSP.distanceTestDone.set(tri_id(children, 1).value);
                        rSP.distanceTestDone.set(tri_id(children, 2).value);
                        rSP.distanceTestDone.set(tri_id(children, 3).value);
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
