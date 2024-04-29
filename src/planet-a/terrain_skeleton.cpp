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
#include "terrain_skeleton.h"
#include "icosahedron.h"

using osp::bitvector_resize;
using osp::Vector3l;
using osp::Vector3u;
using osp::Vector3;

namespace planeta
{

void SubdivScratchpad::resize(TerrainSkeleton& rTrn)
{
    auto const triCapacity = rTrn.skel.tri_group_ids().capacity() * 4;

    // Using centers as 'previous capacity' to detect a reallocation
    if (triCapacity != rTrn.centers.size())
    {
        // note: Since all of these are the same size, it feel practical to put them all in a
        //       single unique_ptr allocation, and access it with array views

        rTrn.centers.resize(triCapacity);

        bitvector_resize(this->  distanceTestDone,  triCapacity);
        bitvector_resize(this->  tryUnsubdiv,       triCapacity);
        bitvector_resize(this->  cantUnsubdiv,      triCapacity);
        bitvector_resize(this->  surfaceAdded,      triCapacity);
        bitvector_resize(this->  surfaceRemoved,    triCapacity);

        for (int lvl = 0; lvl < this->levelMax+1; ++lvl)
        {
            bitvector_resize(rTrn.levels[lvl].hasSubdivedNeighbor,       triCapacity);
            bitvector_resize(rTrn.levels[lvl].hasNonSubdivedNeighbor,    triCapacity);
        }
    }

    auto const vrtxCapacity = rTrn.skel.vrtx_ids().capacity();
    if (vrtxCapacity != rTrn.positions.size())
    {
        rTrn.positions.resize(vrtxCapacity);
        rTrn.normals  .resize(vrtxCapacity);
    }
}

void unsubdivide_select_by_distance(std::uint8_t const lvl, osp::Vector3l const pos, TerrainSkeleton const& rTrn, SubdivScratchpad& rSP)
{
    TerrainSkeleton::Level const& rLvl   = rTrn.levels[lvl];
    SubdivScratchpadLevel&        rLvlSP = rSP .levels[lvl];

    auto const maybe_distance_check = [&rTrn, &rSP, &rLvl, &rLvlSP] (SkTriId const sktriId)
    {
        if (rSP.distanceTestDone.test(sktriId.value))
        {
            return; // Already checked
        }

        SkTriGroupId const childrenId = rTrn.skel.tri_at(sktriId).children;
        if ( ! childrenId.has_value() )
        {
            return; // Must be subdivided to be considered for unsubdivision lol
        }

        SkTriGroup const& children = rTrn.skel.tri_group_at(childrenId);
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
            Vector3l const center = rTrn.centers[sktriId];
            bool const tooFar = ! osp::is_distance_near(pos, center, rSP.distanceThresholdUnsubdiv[lvl]);

            LGRN_ASSERTM(rTrn.skel.tri_at(sktriId).children.has_value(),
                         "Non-subdivided triangles must not be added to distance test.");

            if (tooFar)
            {
                // All checks passed
                rSP.tryUnsubdiv.set(sktriId.value);

                // Floodfill by checking neighbors next
                SkeletonTriangle const& sktri = rTrn.skel.tri_at(sktriId);
                for (SkTriId const neighbor : sktri.neighbors)
                if (neighbor.has_value())
                {
                    maybe_distance_check(neighbor);
                }
            }
        }
    }
}

void unsubdivide_deselect_invariant_violations(std::uint8_t const lvl, TerrainSkeleton const& rTrn, SubdivScratchpad& rSP)
{
    TerrainSkeleton::Level const& rLvl   = rTrn.levels[lvl];
    SubdivScratchpadLevel&        rLvlSP = rSP .levels[lvl];

    auto const violates_invariants = [&rTrn, &rLvl, &rSP] (SkTriId const sktriId, SkeletonTriangle const& sktri) noexcept -> bool
    {
        int subdivedNeighbors = 0;
        for (SkTriId const neighbor : sktri.neighbors)
        if (neighbor.has_value())
        {
            SkeletonTriangle const &rNeighbor = rTrn.skel.tri_at(neighbor);
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
                SkTriGroup const& neighborGroup = rTrn.skel.tri_group_at(rNeighbor.children);

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

    auto const check_recurse = [&violates_invariants, &rTrn, &rLvl, &rSP] (auto const& self, SkTriId const sktriId) -> void
    {
        SkeletonTriangle const& sktri = rTrn.skel.tri_at(sktriId);

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

void unsubdivide_level(std::uint8_t const lvl, TerrainSkeleton& rTrn, SubdivScratchpad& rSP)
{
    auto const wont_unsubdivide = [&rSP] (SkTriId const sktriId) -> bool
    {
        return ( ! rSP.tryUnsubdiv.test(sktriId.value) || rSP.cantUnsubdiv.test(sktriId.value) );
    };

    TerrainSkeleton::Level& rLvl   = rTrn.levels[lvl];
    SubdivScratchpadLevel&   rLvlSP = rSP .levels[lvl];

    for (std::size_t const sktriInt : rSP.tryUnsubdiv.ones())
    if ( ! rSP.cantUnsubdiv.test(sktriInt) )
    {
        // All checks passed, 100% confirmed sktri will be unsubdivided

        SkTriId const sktriId = SkTriId::from_index(sktriInt);
        SkeletonTriangle &rTri = rTrn.skel.tri_at(sktriId);

        LGRN_ASSERT(!rLvl.hasSubdivedNeighbor.test(sktriInt));
        for (SkTriId const neighborId : rTri.neighbors)
        if ( neighborId.has_value() && wont_unsubdivide(neighborId) )
        {
            SkeletonTriangle const& rNeighborTri = rTrn.skel.tri_at(neighborId);
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
                    && rTrn.skel.is_tri_subdivided(neighborNeighborId) )
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

        rSP.onUnsubdiv(sktriId, rTri, rTrn, rSP.onUnsubdivUserData);

        rTrn.skel.tri_unsubdiv(SkTriId(sktriInt), rTri);
    }

    rSP.tryUnsubdiv.reset();
    rSP.cantUnsubdiv.reset();
}

SkTriGroupId subdivide(
        SkTriId             sktriId,
        SkeletonTriangle&   rSkTri,
        std::uint8_t        lvl,
        bool                hasNextLevel,
        TerrainSkeleton&    rTrn,
        SubdivScratchpad&   rSP)
{
    LGRN_ASSERTM(rTrn.skel.tri_group_ids().exists(tri_group_id(sktriId)), "SkTri does not exist");
    LGRN_ASSERTM(!rSkTri.children.has_value(), "Already subdivided");

    TerrainSkeleton::Level& rLvl = rTrn.levels[lvl];

    std::array<SkTriId,  3> const neighbors = { rSkTri.neighbors[0], rSkTri.neighbors[1], rSkTri.neighbors[2] };
    std::array<SkVrtxId, 3> const corners   = { rSkTri .vertices[0], rSkTri .vertices[1], rSkTri .vertices[2] };

    // Create or get vertices between the 3 corners
    std::array<MaybeNewId<SkVrtxId>, 3> const middlesNew = rTrn.skel.vrtx_create_middles(corners);
    std::array<SkVrtxId, 3>             const middles    = {middlesNew[0].id, middlesNew[1].id, middlesNew[2].id};

    // Actually do the subdivision ( create a new group (4 triangles) as children )
    // manual borrow checker hint: rSkTri becomes invalid here >:)
    auto const [groupId, rGroup] = rTrn.skel.tri_subdiv(sktriId, rSkTri, middles);

    rSP.resize(rTrn);

    rSP.onSubdiv(sktriId, groupId, corners, middlesNew, rTrn, rSP.onSubdivUserData);

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
        SkeletonTriangle& rNeighbor = rTrn.skel.tri_at(neighborId);
        if (rNeighbor.children.has_value())
        {
            // Assign bi-directional connection (neighbor's neighbor)
            int const neighborEdgeIdx = rNeighbor.find_neighbor_index(sktriId);

            SkTriGroup &rNeighborGroup = rTrn.skel.tri_group_at(rNeighbor.children);

            auto const [selfEdge, neighborEdge]
                = rTrn.skel.tri_group_set_neighboring(
                    {.id = groupId,            .rGroup = rGroup,         .edge = selfEdgeIdx},
                    {.id = rNeighbor.children, .rGroup = rNeighborGroup, .edge = neighborEdgeIdx});

            if (hasNextLevel)
            {
                TerrainSkeleton::Level &rNextLvl = rTrn.levels[lvl+1];
                if (rTrn.skel.tri_at(neighborEdge.childB).children.has_value())
                {
                    rNextLvl.hasSubdivedNeighbor.set(selfEdge.childA.value);
                    rNextLvl.hasNonSubdivedNeighbor.set(neighborEdge.childB.value);
                }

                if (rTrn.skel.tri_at(neighborEdge.childA).children.has_value())
                {
                    rNextLvl.hasSubdivedNeighbor.set(selfEdge.childB.value);
                    rNextLvl.hasNonSubdivedNeighbor.set(neighborEdge.childA.value);
                }
            }

            bool neighborHasNonSubdivedNeighbor = false;
            for (SkTriId const neighborNeighborId : rNeighbor.neighbors)
            if (   neighborNeighborId.has_value()
                && neighborNeighborId != sktriId
                && ! rTrn.skel.is_tri_subdivided(neighborNeighborId) )
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
        SkTriId const neighborId = rTrn.skel.tri_at(sktriId).neighbors[selfEdgeIdx];
        if (neighborId.has_value())
        {
            SkeletonTriangle& rNeighbor = rTrn.skel.tri_at(neighborId);
            if ( rNeighbor.children.has_value() )
            {
                continue; // Neighbor already subdivided. nothing to do
            }

            // Check Invariant A by seeing if any other neighbor's neighbors are subdivided
            auto const is_other_subdivided = [&rSkel = rTrn.skel, &rNeighbor, sktriId] (SkTriId const other)
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
                subdivide(neighborId, rTrn.skel.tri_at(neighborId), lvl, hasNextLevel, rTrn, rSP);
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

            SkTriId const parent = rTrn.skel.tri_group_at(tri_group_id(sktriId)).parent;

            LGRN_ASSERTM(parent.has_value(), "bruh");

            auto const& parentNeighbors = rTrn.skel.tri_at(parent).neighbors;

            LGRN_ASSERTM(parentNeighbors[selfEdgeIdx].has_value(),
                         "something screwed up XD");

            SkTriId const neighborParent = parentNeighbors[selfEdgeIdx].value();

            // Adds to ctx.rTerrain.levels[level-1].distanceTestNext
            subdivide(neighborParent, rTrn.skel.tri_at(neighborParent), lvl-1, true, rTrn, rSP);
            rSP.distanceTestDone.set(neighborParent.value);

            rSP.levelNeedProcess = std::min<uint8_t>(rSP.levelNeedProcess, lvl-1);
        }
    }

    return groupId;
}


void subdivide_level_by_distance(Vector3l const pos, std::uint8_t const lvl, TerrainSkeleton& rTrn, SubdivScratchpad& rSP)
{
    LGRN_ASSERT(lvl == rSP.levelNeedProcess);

    TerrainSkeleton::Level &rLvl   = rTrn.levels[lvl];
    SubdivScratchpadLevel  &rLvlSP = rSP .levels[lvl];

    bool const hasNextLevel = (lvl+1 < rSP.levelMax);

    while ( ! rSP.levels[lvl].distanceTestNext.empty() )
    {
        std::swap(rLvlSP.distanceTestProcessing, rLvlSP.distanceTestNext);
        rLvlSP.distanceTestNext.clear();

        for (SkTriId const sktriId : rLvlSP.distanceTestProcessing)
        {
            Vector3l const center = rTrn.centers[sktriId];

            LGRN_ASSERT(rSP.distanceTestDone.test(sktriId.value));
            bool const distanceNear = osp::is_distance_near(pos, center, rSP.distanceThresholdSubdiv[lvl]);
            ++rSP.distanceCheckCount;

            if (distanceNear)
            {
                SkeletonTriangle &rTri = rTrn.skel.tri_at(sktriId);
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
                    subdivide(sktriId, rTri, lvl, hasNextLevel, rTrn, rSP);
                }
            }

            // Fix up Invariant B violations
            while (rSP.levelNeedProcess != lvl)
            {
                subdivide_level_by_distance(pos, rSP.levelNeedProcess, rTrn, rSP);
            }
        }
    }

    LGRN_ASSERT(lvl == rSP.levelNeedProcess);
    ++rSP.levelNeedProcess;
}

void calc_sphere_tri_center(SkTriGroupId const groupId, TerrainSkeleton& rTrn, float const maxRadius, float const height)
{
    using osp::math::int_2pow;

    SkTriGroup const &group = rTrn.skel.tri_group_at(groupId);

    for (int i = 0; i < 4; ++i)
    {
        SkTriId          const  sktriId = tri_id(groupId, i);
        SkeletonTriangle const& tri     = group.triangles[i];

        SkVrtxId const va = tri.vertices[0].value();
        SkVrtxId const vb = tri.vertices[1].value();
        SkVrtxId const vc = tri.vertices[2].value();

        // average without overflow
        Vector3l const posAvg = rTrn.positions[va] / 3
                              + rTrn.positions[vb] / 3
                              + rTrn.positions[vc] / 3;

        Vector3 const nrmSum = rTrn.normals[va]
                             + rTrn.normals[vb]
                             + rTrn.normals[vc];

        LGRN_ASSERT(group.depth < gc_icoTowerOverHorizonVsLevel.size());
        float const terrainMaxHeight = height + maxRadius * gc_icoTowerOverHorizonVsLevel[group.depth];

        // 0.5 * terrainMaxHeight           : halve for middle
        // int_2pow<int>(rTerrain.scale)    : Vector3l conversion factor
        // / 3.0f                           : average from sum of 3 values
        Vector3l const riseToMid = Vector3l(nrmSum * (0.5f * terrainMaxHeight * osp::math::int_2pow<int>(rTrn.scale) / 3.0f));

        rTrn.centers[sktriId] = posAvg + riseToMid;
    }
}

void debug_check_invariants(TerrainSkeleton &rTrn)
{
    // iterate all existing triangles
    for (std::size_t sktriInt = 0; sktriInt < rTrn.skel.tri_group_ids().capacity() * 4; ++sktriInt)
    if (SkTriId const sktriId = SkTriId(sktriInt);
        rTrn.skel.tri_group_ids().exists(tri_group_id(sktriId)))
    {
        SkeletonTriangle const& sktri = rTrn.skel.tri_at(sktriId);
        SkTriGroup       const& group = rTrn.skel.tri_group_at(tri_group_id(sktriId));

        int subdivedNeighbors = 0;
        int nonSubdivedNeighbors = 0;
        for (int edge = 0; edge < 3; ++edge)
        {
            SkTriId const neighbor = sktri.neighbors[edge];
            if (neighbor.has_value())
            {
                if (rTrn.skel.is_tri_subdivided(neighbor))
                {
                    ++subdivedNeighbors;
                }
                else
                {
                    ++nonSubdivedNeighbors;
                }
            }
            else
            {
                // Neighbor doesn't exist. parent MUST have neighbor
                SkTriId const parent = rTrn.skel.tri_group_at(tri_group_id(sktriId)).parent;
                LGRN_ASSERTM(parent.has_value(), "bruh");
                auto const& parentNeighbors = rTrn.skel.tri_at(parent).neighbors;
                LGRN_ASSERTM(parentNeighbors[edge].has_value(), "Invariant B Violation");

                LGRN_ASSERTM(rTrn.skel.is_tri_subdivided(parentNeighbors[edge]) == false,
                             "Incorrectly set neighbors");
            }
        }

        if ( ! sktri.children.has_value() )
        {
            LGRN_ASSERTM(subdivedNeighbors < 2, "Invariant A Violation");
        }

        // Verify hasSubdivedNeighbor and hasNonSubdivedNeighbor bitvectors
        if (group.depth < rTrn.levels.size())
        {
            TerrainSkeleton::Level& rLvl = rTrn.levels[group.depth];

            auto const triCapacity  = rTrn.skel.tri_group_ids().capacity() * 4;

            if (sktri.children.has_value())
            {
                LGRN_ASSERTMV(rLvl.hasNonSubdivedNeighbor.test(sktriInt) == (nonSubdivedNeighbors != 0),
                              "Incorrectly set hasNonSubdivedNeighbor",
                              sktriInt,
                              int(group.depth),
                              rLvl.hasNonSubdivedNeighbor.test(sktriInt),
                              nonSubdivedNeighbors);
                LGRN_ASSERTM(rLvl.hasSubdivedNeighbor.test(sktriInt) == false,
                            "hasSubdivedNeighbor is only for non-subdivided tris");
            }
            else
            {
                LGRN_ASSERTMV(rLvl.hasSubdivedNeighbor.test(sktriInt) == (subdivedNeighbors != 0),
                              "Incorrectly set hasSubdivedNeighbor",
                              sktriInt,
                              int(group.depth),
                              rLvl.hasSubdivedNeighbor.test(sktriInt),
                              subdivedNeighbors);
                LGRN_ASSERTM(rLvl.hasNonSubdivedNeighbor.test(sktriInt) == false,
                            "hasNonSubdivedNeighbor is only for subdivided tris");
            }
        }
    }
}


void chunkgen_restitch_check(
        ChunkId       const chunkId,
        SkTriId       const sktriId,
        ChunkSkeleton&      rSkCh,
        TerrainSkeleton&    rSkTrn,
        ChunkScratchpad&    rChSP)
{
    ChunkStitch ownCmd = {.enabled = true, .detailX2 = false};

    auto const& neighbors = rSkTrn.skel.tri_at(sktriId).neighbors;

    for (int selfEdgeIdx = 0; selfEdgeIdx < 3; ++selfEdgeIdx)
    {
        SkTriId const neighborId = neighbors[selfEdgeIdx];
        if ( neighborId.has_value() )
        {
            ChunkId const neighborChunk = rSkCh.m_triToChunk[neighborId];
            if ( neighborChunk.has_value() )
            {
                // In cases where high-detail chunks were in sktriId's position previously,
                // but were then unsubdivided and replaced with one low-detail chunk,
                // remove any detailX2 (low-to-high detail) stitches from neighbors.
                ChunkStitch &rNeighborStitchCmd = rChSP.stitchCmds[neighborChunk];
                if (rNeighborStitchCmd.enabled)
                {
                    continue; // Command already issued by neighbor's neighbor who happens to
                              // be in surfaceAdded
                }
                ChunkStitch const neighborStitch = rSkCh.m_chunkStitch[neighborChunk];
                if (neighborStitch.enabled && ! neighborStitch.detailX2)
                {
                    continue; // Neighbor stitch is up-to-date
                }
                if (neighborStitch.detailX2 && rSkTrn.skel.tri_at(neighborId).neighbors[neighborStitch.x2ownEdge] != sktriId)
                {
                    continue; // Neighbor has detailX2 stitch but for an unrelated chunk
                }

                rNeighborStitchCmd = { .enabled = true, .detailX2 = false };
            }
            else
            {
                // Neighbor doesn't have chunk. It is either a hole in the terrain, or it has
                // chunked children which requires a detailX2 (low-to-high detail) stitch.
                SkeletonTriangle const& neighbor = rSkTrn.skel.tri_at(neighborId);

                if ( ! neighbor.children.has_value() )
                {
                    continue; // Hole in terrain
                }

                int  const neighborEdgeIdx = neighbor.find_neighbor_index(sktriId);
                bool const neighborHasChunkedChildren
                        =  rSkCh.m_triToChunk[tri_id(neighbor.children, neighborEdgeIdx) ]       .has_value()
                        && rSkCh.m_triToChunk[tri_id(neighbor.children, (neighborEdgeIdx+1) % 3)].has_value();

                if ( ! neighborHasChunkedChildren )
                {
                    continue; // Both neighboring children are holes in the terrain
                }

                ownCmd = {
                        .enabled        = true,
                        .detailX2       = true,
                        .x2ownEdge      = static_cast<unsigned char>(selfEdgeIdx),
                        .x2neighborEdge = static_cast<unsigned char>(neighborEdgeIdx)
                };
            }
        }
        else if (tri_sibling_index(sktriId) != 3)
        {
            // Check parent's neighbors for lower-detail chunks and make sure they have an
            // x2detail stitch towards this new chunk.
            // Sibling 3 triangles are skipped since it's surrounded by its siblings, and
            // isn't touching any of its parent's neighbors.

            // Assumes Invariant A isn't broken, these don't need checks
            SkTriId const parent                = rSkTrn.skel.tri_group_at(tri_group_id(sktriId)).parent;
            SkTriId const parentNeighbor        = rSkTrn.skel.tri_at(parent).neighbors[selfEdgeIdx];
            ChunkId const parentNeighborChunk   = rSkCh.m_triToChunk[parentNeighbor];
            int     const neighborEdge          = rSkTrn.skel.tri_at(parentNeighbor).find_neighbor_index(parent);

            ChunkStitch const desiredStitch =  {
                    .enabled        = true,
                    .detailX2       = true,
                    .x2ownEdge      = static_cast<unsigned char>(neighborEdge),
                    .x2neighborEdge = static_cast<unsigned char>(selfEdgeIdx)
            };

            ChunkStitch &rStitchCmd = rChSP.stitchCmds[parentNeighborChunk];
            LGRN_ASSERT(   (rStitchCmd.enabled == false)
                        || (rStitchCmd.detailX2 == false)
                        || (rStitchCmd == desiredStitch));
            rStitchCmd = desiredStitch;
        }
    }

    rChSP.stitchCmds[chunkId] = ownCmd;
}

/*
void chunkgen_create(SkTriId const sktriId,std::uint8_t  const chLevel,  ChunkSkeleton&      rSkCh, TerrainSkeleton&    rSkTrn, ChunkScratchpad&    rChSP)
{
    LGRN_ASSERTV(!rSkCh.m_triToChunk[sktriId].has_value(), sktriId.value);

    auto const &tri = rSkTrn.skel.tri_at(sktriId);
    auto const& corners = tri.vertices;

    osp::ArrayView< MaybeNewId<SkVrtxId> > const edgeVrtxView = rChSP.edgeVertices;
    osp::ArrayView< MaybeNewId<SkVrtxId> > const edgeLft = edgeVrtxView.sliceSize(edgeSize * 0, edgeSize);
    osp::ArrayView< MaybeNewId<SkVrtxId> > const edgeBtm = edgeVrtxView.sliceSize(edgeSize * 1, edgeSize);
    osp::ArrayView< MaybeNewId<SkVrtxId> > const edgeRte = edgeVrtxView.sliceSize(edgeSize * 2, edgeSize);

    rSkTrn.skel.vrtx_create_chunk_edge_recurse(chLevel, corners[0], corners[1], edgeLft);
    rSkTrn.skel.vrtx_create_chunk_edge_recurse(chLevel, corners[1], corners[2], edgeBtm);
    rSkTrn.skel.vrtx_create_chunk_edge_recurse(chLevel, corners[2], corners[0], edgeRte);

    ChunkId const chunk = rSkCh.chunk_create(sktriId, rSkTrn.skel, rChSP.sharedNewlyAdded, edgeLft, edgeBtm, edgeRte);

    rChSP.stitchCmds[chunk] = {.enabled = true, .detailX2 = false};

    rSP.resize(rSkTrn);

    ico_calc_chunk_edge_recurse(rTerrainIco.radius, rSkTrn.scale, chLevel, corners[0], corners[1], edgeLft, rSkTrn.positions, rSkTrn.normals);
    ico_calc_chunk_edge_recurse(rTerrainIco.radius, rSkTrn.scale, chLevel, corners[1], corners[2], edgeBtm, rSkTrn.positions, rSkTrn.normals);
    ico_calc_chunk_edge_recurse(rTerrainIco.radius, rSkTrn.scale, chLevel, corners[2], corners[0], edgeRte, rSkTrn.positions, rSkTrn.normals);

}

*/

struct TerrainFaceWriter
{
    // 'iterators' used by ArrayView
    using IndxIt_t      = osp::Vector3u*;
    using FanNormalIt_t = osp::Vector3*;
    using ContribIt_t   = SharedNormalContribution*;

    void emit_face(VertexIdx a, VertexIdx b, VertexIdx c) noexcept
    {
        Vector3 const u = vbufPos[b] - vbufPos[a];
        Vector3 const v = vbufPos[c] - vbufPos[a];

        selectedFaceIndx   = {a, b, c};
        selectedFaceNormal = Magnum::Math::cross(u, v).normalized();

        *currentFace       = selectedFaceIndx;

        std::advance(currentFace, 1);
    }

//    void add_normal_fill_vrtx(VertexIdx const vertex, bool const detailX2Half)
//    {
//        // All fill vertices have 6 connected faces
//        // (just look at a picture of a triangular tiling)

//        float const mul = detailX2Half ? (1.0f/12.0f) : (1.0f/6.0f);
//        vbufNrm[vertex] += mul * prevFaceNormal;
//    }

    void add_normal_shared_vrtx(VertexIdx const vertex, SharedVrtxId const shared)
    {
        std::uint8_t &rFaceCount = sharedFaceCount[shared.value];
        rFaceCount ++;

        sharedNormals[shared.value] += selectedFaceNormal;

        // Record contributions to shared vertex normal, since this needs to be subtracted when
        // the associated chunk is removed or restitched.

        // Find an existing SharedNormalContribution for the given shared vertex.
        // Since each triangle added is in contact with the previous triangle added, we only need
        // to linear-search the previous few (4) contributions added. We also need to consider the
        // first few (4), since the last triangle added will loop around and touch the start,
        // forming a ring of triangles.
        bool found = false;
        SharedNormalContribution &rContrib = [this, shared, &found] () -> SharedNormalContribution&
        {
            auto const matches = [shared] (SharedNormalContribution const& x) noexcept { return x.shared == shared; };

            ContribIt_t const searchAFirst = std::max<ContribIt_t>(std::prev(contribLast, 4), normalContrib.begin());
            ContribIt_t const searchALast  = contribLast;
            ContribIt_t const searchBFirst = normalContrib.begin();
            ContribIt_t const searchBLast  = std::min<ContribIt_t>(std::next(normalContrib.begin(), 4), searchAFirst);

            if (ContribIt_t const foundTemp = std::find_if(searchAFirst, searchALast, matches);
                foundTemp != searchALast)
            {
                found = true;
                return *foundTemp;
            }

            if (ContribIt_t const foundTemp = std::find_if(searchBFirst, searchBLast, matches);
                foundTemp != searchBLast)
            {
                found = true;
                return *foundTemp;
            }

            LGRN_ASSERTM(std::none_of(searchALast, searchALast, matches), "search code above is broken XD");

            return *contribLast;
        }();

        if ( ! found )
        {
            rContrib.shared = shared;
            std::advance(contribLast, 1);
            LGRN_ASSERT(contribLast != normalContrib.end());
            rSharedNormalsDirty.set(shared.value);
        }

        rContrib.sum += selectedFaceNormal;



        // Add new face normal to the average
//        Vector3 &rVrtxNormal = vbufNrm[vertex];
//        rVrtxNormal = (rVrtxNormal * rFaceCount + prevFaceNormal) / (rFaceCount + 1);
//        rFaceCount ++;
    }

    osp::ArrayView<osp::Vector3 const>          vbufPos;
    osp::ArrayView<std::uint8_t>                sharedFaceCount;
    osp::ArrayView<osp::Vector3>                sharedNormals;
    osp::ArrayView<SharedNormalContribution>    normalContrib;

    Vector3          selectedFaceNormal;
    Vector3u         selectedFaceIndx;
    IndxIt_t         currentFace;
    ContribIt_t      contribLast;
    osp::BitVector_t &rSharedNormalsDirty;
};
static_assert(CFaceWriter<TerrainFaceWriter>, "TerrainFaceWriter must satisfy concept CFaceWriter");

void chunkgen_update_faces(
        ChunkId                            const chunkId,
        SkTriId                            const sktriId,
        bool                               const newlyAdded,
        osp::ArrayView<osp::Vector3u>      const ibuf,
        osp::ArrayView<osp::Vector3 const> const vbufPos,
        osp::ArrayView<osp::Vector3>       const vbufNrm,
        osp::ArrayView<osp::Vector3>       const sharedNormals,
        osp::ArrayView<osp::Vector3>       const chunkFillSharedNormals,
        osp::ArrayView<SharedNormalContribution> const chunkFanNormalContrib,
        TerrainSkeleton                    const &rSkTrn,
        ChunkedTriangleMeshInfo            const &rChInfo,
        ChunkScratchpad                          &rChSP,
        ChunkSkeleton                            &rSkCh)
{
    using namespace osp;

    auto const ibufSlice         = as_2d(ibuf,                   rChInfo.chunkMaxFaceCount) .row(chunkId.value);
    auto const fanNormalContrib  = as_2d(chunkFanNormalContrib,  rChInfo.fanExtraFaceCount) .row(chunkId.value);
    auto const fillNormalContrib = as_2d(chunkFillSharedNormals, rSkCh.m_chunkSharedCount) .row(chunkId.value);
    auto const sharedUsed        = rSkCh.shared_vertices_used(chunkId);

    TerrainFaceWriter writer{
        .vbufPos             = vbufPos,
        .sharedFaceCount     = rSkCh.m_sharedFaceCount.base(),
        .sharedNormals       = sharedNormals,
        .normalContrib       = fanNormalContrib,
        .currentFace         = ibufSlice.begin(),
        .contribLast         = fanNormalContrib.begin(),
        .rSharedNormalsDirty = rChSP.sharedNormalsDirty
    };

    auto const add_fill_normal = [&sharedUsed, &fillNormalContrib, &sharedNormals, &rChSP, &vbufNrm]
                                 (ChunkLocalSharedId const local, VertexIdx const vertex, Vector3 const faceNormal, bool const onEdge)
    {
        if (local.has_value())
        {
            if ( ! onEdge )
            {
                SharedVrtxId const shared = sharedUsed[local.value];

                fillNormalContrib[local.value]  += faceNormal;
                sharedNormals    [shared.value] += faceNormal;

                rChSP.sharedNormalsDirty.set(shared.value);
            }
        }
        else
        {
            vbufNrm[vertex] += faceNormal;
        }
    };

    // top, left, right
    auto const add_fill_tri = [&add_fill_normal, &rSkCh, &rChInfo, &writer, &vbufPos, chunkId]
            (uint16_t const aX, uint16_t const aY,
             uint16_t const bX, uint16_t const bY,
             uint16_t const cX, uint16_t const cY,
             bool const onEdge)
    {
        auto const [shLocalA, vrtxA] = chunk_coord_to_vrtx(rSkCh, rChInfo, chunkId, aX, aY);
        auto const [shLocalB, vrtxB] = chunk_coord_to_vrtx(rSkCh, rChInfo, chunkId, bX, bY);
        auto const [shLocalC, vrtxC] = chunk_coord_to_vrtx(rSkCh, rChInfo, chunkId, cX, cY);

        Vector3 const u      = vbufPos[vrtxB] - vbufPos[vrtxA];
        Vector3 const v      = vbufPos[vrtxC] - vbufPos[vrtxA];
        Vector3 const normal = Magnum::Math::cross(u, v).normalized();

        add_fill_normal(shLocalA, vrtxA, normal, onEdge);
        add_fill_normal(shLocalB, vrtxB, normal, onEdge);
        add_fill_normal(shLocalC, vrtxC, normal, onEdge);

        if ( ! onEdge )
        {
            writer.emit_face(vrtxA, vrtxB, vrtxC);
        }
    };


    if (newlyAdded)
    {
        // Reset fill normals to zero, as values are left over from a previously deleted chunk
        auto const fillNormals = as_2d(vbufNrm.exceptPrefix(rChInfo.vbufFillOffset), rChInfo.fillVrtxCount).row(chunkId.value);
        std::fill(fillNormals.begin(),       fillNormals.end(),       Vector3{ZeroInit});
        std::fill(fillNormalContrib.begin(), fillNormalContrib.end(), Vector3{ZeroInit});

        for (unsigned int y = 0; y < rSkCh.m_chunkEdgeVrtxCount; ++y)
        {
            add_fill_tri(0, y,      0, y+1,     1, y+1,  true);

            for (unsigned int x = 0; x < y; ++x)
            {
                // down-pointing
                add_fill_tri(   x+1, y+1,      x+1, y,         x, y,        false);

                // up pointing
                bool const onEdge = (x == y-1) || y == rSkCh.m_chunkEdgeVrtxCount - 1;
                add_fill_tri(   x+1, y,        x+1, y+1,       x+2, y+1,    onEdge);
            }
        }

        LGRN_ASSERT(writer.currentFace == std::next(ibufSlice.begin(), rChInfo.fillFaceCount));

        for (Vector3 &rNormal : fillNormals)
        {
            rNormal = rNormal.normalized();
        }
    }

    ChunkStitch const cmd = rChSP.stitchCmds[chunkId];

    if (cmd.enabled)
    {
        std::fill(fanNormalContrib.begin(), fanNormalContrib.end(), SharedNormalContribution{});

        ChunkStitch &rCurrentStitch = rSkCh.m_chunkStitch[chunkId];

        if (rCurrentStitch.enabled)
        {
            // TODO: Subtract normal contributions, Delete previous stitch
        }
        rSkCh.m_chunkStitch[chunkId] = cmd;

        writer.currentFace = std::next(ibufSlice.begin(), rChInfo.fillFaceCount);

        ArrayView<SharedVrtxOwner_t const> detailX2Edge0;
        ArrayView<SharedVrtxOwner_t const> detailX2Edge1;

        if (cmd.detailX2)
        {
            SkTriId          const  neighborId = rSkTrn.skel.tri_at(sktriId).neighbors[cmd.x2ownEdge];
            SkeletonTriangle const& neighbor   = rSkTrn.skel.tri_at(neighborId);

            auto const child_chunk_edge = [&rSkCh, children = neighbor.children, edgeIdx = std::uint32_t(cmd.x2neighborEdge)]
                                          (std::uint8_t siblingIdx) -> ArrayView<SharedVrtxOwner_t const>
            {
                ChunkId const chunk = rSkCh.m_triToChunk[tri_id(children, siblingIdx)];
                return rSkCh.shared_vertices_used(chunk).sliceSize(edgeIdx * rSkCh.m_chunkEdgeVrtxCount, rSkCh.m_chunkEdgeVrtxCount);
            };

            detailX2Edge0 = child_chunk_edge(cmd.x2neighborEdge);
            detailX2Edge1 = child_chunk_edge((cmd.x2neighborEdge + 1) % 3);
        }

        auto const stitcher = make_chunk_fan_stitcher<TerrainFaceWriter&>(writer, chunkId, detailX2Edge0, detailX2Edge1, rSkCh, rChInfo);

        using enum ECornerDetailX2;

        if (cmd.detailX2)
        {
            switch (cmd.x2ownEdge)
            {
            case 0:
                stitcher.corner <0, Left >();
                stitcher.edge   <0, true >();
                stitcher.corner <1, Right>();
                stitcher.edge   <1, false>();
                stitcher.corner <2, None >();
                stitcher.edge   <2, false>();
                break;
            case 1:
                stitcher.corner <0, None >();
                stitcher.edge   <0, false>();
                stitcher.corner <1, Left >();
                stitcher.edge   <1, true >();
                stitcher.corner <2, Right>();
                stitcher.edge   <2, false>();
                break;
            case 2:
                stitcher.corner <0, Right>();
                stitcher.edge   <0, false>();
                stitcher.corner <1, None >();
                stitcher.edge   <1, false>();
                stitcher.corner <2, Left >();
                stitcher.edge   <2, true >();
                break;
            }
        }
        else
        {
            stitcher.corner <0, None>();
            stitcher.edge   <0, false>();
            stitcher.corner <1, None>();
            stitcher.edge   <1, false>();
            stitcher.corner <2, None>();
            stitcher.edge   <2, false>();
        }

        std::fill(writer.currentFace, ibufSlice.end(), Vector3u{0, 0, 0});
    }
}

void chunkgen_debug_check_invariants(
        osp::ArrayView<osp::Vector3u>      const ibuf,
        osp::ArrayView<osp::Vector3 const> const vbufPos,
        osp::ArrayView<osp::Vector3 const> const vbufNrm,
        ChunkedTriangleMeshInfo            const &rChInfo,
        ChunkSkeleton                      const &rSkCh)
{
    auto const check_vertex = [&] (VertexIdx vertex, SharedVrtxId sharedId, ChunkId chunkId)
    {
        Vector3 const normal = vbufNrm[vertex];
        float   const length = normal.length();

        LGRN_ASSERTMV(std::abs(length - 1.0f) < 0.05f, "Normal isn't normalized", length, vertex, sharedId.value, chunkId.value);
    };

    for (std::size_t const sharedInt : rSkCh.m_sharedIds.bitview().zeros())
    {
        check_vertex(rChInfo.vbufSharedOffset + sharedInt, SharedVrtxId(sharedInt), {});
    }

    for (std::size_t const chunkInt : rSkCh.m_chunkIds.bitview().zeros())
    {
        VertexIdx const first = rChInfo.vbufFillOffset + chunkInt * rChInfo.fillVrtxCount;
        VertexIdx const last  = first + rChInfo.fillVrtxCount;

        for (VertexIdx vertex = first; vertex < last; ++vertex)
        {
            check_vertex(vertex, {}, ChunkId(chunkInt));
        }
    }
}

void chunkgen_write_obj(
        std::ostream                             &rStream,
        osp::ArrayView<osp::Vector3u>      const ibuf,
        osp::ArrayView<osp::Vector3 const> const vbufPos,
        osp::ArrayView<osp::Vector3 const> const vbufNrm,
        ChunkedTriangleMeshInfo            const &rChInfo,
        ChunkSkeleton                      const &rSkCh)
{
    auto const chunkCount = rSkCh.m_chunkIds.size();
    auto const sharedCount = rSkCh.m_sharedIds.size();

    rStream << "# Terrain mesh debug output\n"
            << "# Chunks: "          << chunkCount  << "/" << rSkCh.m_chunkIds.capacity() << "\n"
            << "# Shared Vertices: " << sharedCount << "/" << rSkCh.m_sharedIds.capacity() << "\n";

    rStream << "o Planet\n";

    for (Vector3 v : vbufPos)
    {
        rStream << "v " << v.x() << " " << v.y() << " "  << v.z() << "\n";
    }

    for (Vector3 v : vbufNrm)
    {
        rStream << "vn " << v.x() << " " << v.y() << " "  << v.z() << "\n";
    }

    for (std::size_t const chunkIdInt : rSkCh.m_chunkIds.bitview().zeros())
    {
        auto const view = ibuf.sliceSize(chunkIdInt * rChInfo.chunkMaxFaceCount, rChInfo.chunkMaxFaceCount);

        for (osp::Vector3u const faceIdx : view)
        {
            // Indexes start at 1 for .obj files
            // Format: "f vertex1//normal1 vertex2//normal2 vertex3//normal3"
            rStream << "f "
                    << (faceIdx.x()+1) << "//" << (faceIdx.x()+1) << " "
                    << (faceIdx.y()+1) << "//" << (faceIdx.y()+1) << " "
                    << (faceIdx.z()+1) << "//" << (faceIdx.z()+1) << "\n";
        }
    }
}


} // namespace planeta
