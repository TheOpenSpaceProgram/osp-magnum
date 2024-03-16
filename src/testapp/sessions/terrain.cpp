/**
 * Open Space Program
 * Copyright © 2019-2023 Open Space Program Project
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
#include "terrain.h"
#include "common.h"
#include "longeron/utility/asserts.hpp"

#include <planet-a/icosahedron.h>
#include <osp/core/math_2pow.h>
#include <osp/drawing/drawing.h>

#include <fstream>
#include <iostream>
#include <span>

using namespace planeta;
using namespace osp;
using namespace osp::math;
using namespace osp::draw;

namespace testapp::scenes
{


struct PlanetVertex
{
    osp::Vector3 pos;
    osp::Vector3 nrm;
};

using Vector2ui = Magnum::Math::Vector2<Magnum::UnsignedInt>;


void calculate_centers(SkTriGroupId const groupId, ACtxTerrain& rTerrain, float const maxRadius, float const height)
{

    SkTriGroup const &group = rTerrain.skeleton.tri_group_at(groupId);

    for (int i = 0; i < 4; ++i)
    {
        SkTriId          const  sktriId = tri_id(groupId, i);
        SkeletonTriangle const& tri     = group.triangles[i];

        SkVrtxId const va = tri.vertices[0].value();
        SkVrtxId const vb = tri.vertices[1].value();
        SkVrtxId const vc = tri.vertices[2].value();

        // average without overflow
        Vector3l const posAvg = rTerrain.skPositions[va] / 3
                              + rTerrain.skPositions[vb] / 3
                              + rTerrain.skPositions[vc] / 3;

        Vector3 const nrmSum = rTerrain.skNormals[va]
                             + rTerrain.skNormals[vb]
                             + rTerrain.skNormals[vc];

        float const terrainMaxHeight = height + maxRadius * gc_icoTowerOverHorizonVsLevel[group.depth];

        // 0.5 * terrainMaxHeight           : halve for middle
        // int_2pow<int>(rTerrain.scale)    : Vector3l conversion factor
        // / 3.0f                           : average from sum of 3 values
        Vector3l const riseToMid = Vector3l(nrmSum * (0.5f * terrainMaxHeight * int_2pow<int>(rTerrain.scale) / 3.0f));

        rTerrain.sktriCenter[sktriId] = posAvg + riseToMid;
    }
}

void init_ico_terrain(ACtxTerrain& rTerrain, ACtxTerrainIco& rTerrainIco, float radius, float height, int scale)
{
    rTerrainIco.radius = radius;
    rTerrain.scale  = scale;

    auto &rSkel = rTerrain.skeleton;

    rSkel = create_skeleton_icosahedron(radius, scale, rTerrainIco.icoVrtx, rTerrainIco.icoGroups, rTerrainIco.icoTri, rTerrain.skPositions, rTerrain.skNormals);

    rTerrain.sktriCenter.resize(rTerrain.skeleton.tri_group_ids().capacity() * 4);

    for (SkTriGroupId const groupId : rTerrainIco.icoGroups)
    {
        calculate_centers(groupId, rTerrain, radius + height, height);
    }
}

struct SkTriNewSubdiv
{
    std::array<SkVrtxId, 3> corners;
    std::array<MaybeNewId<SkVrtxId>, 3> middles;
    SkTriId id;
    SkTriGroupId group;
};

constexpr std::uint64_t absdelta(std::int64_t lhs, std::int64_t rhs) noexcept
{
    bool const lhsPositive = lhs > 0;
    bool const rhsPositive = rhs > 0;
    if      (   lhsPositive && ! rhsPositive )
    {
        return std::uint64_t(lhs) + std::uint64_t(-rhs);
    }
    else if ( ! lhsPositive &&   rhsPositive )
    {
        return std::uint64_t(-lhs) + std::uint64_t(rhs);
    }
    // else, lhs and rhs are the same sign. no risk of overflow

    return std::uint64_t((lhs > rhs) ? (lhs - rhs) : (rhs - lhs));
};

/**
 * @return (distance between a and b) > threshold
 */
constexpr bool is_distance_near(Vector3l const a, Vector3l const b, std::uint64_t const threshold)
{
    std::uint64_t const dx = absdelta(a.x(), b.x());
    std::uint64_t const dy = absdelta(a.y(), b.y());
    std::uint64_t const dz = absdelta(a.z(), b.z());

    // 1431655765 = sqrt(2^64)/3 = max distance without risk of overflow
    constexpr std::uint64_t dmax = 1431655765ul;

    if (dx > dmax || dy > dmax || dz > dmax)
    {
        return false;
    }

    std::uint64_t const magnitudeSqr = (dx*dx + dy*dy + dz*dz);

    return magnitudeSqr < threshold*threshold;
}

struct TplIterEdge
{
    SkTriId neighborId;
    SkTriOwner_t &rChildNeighbor0;
    SkTriOwner_t &rChildNeighbor1;
};

bool violates_rule_a(SkTriId const sktri, SkeletonTriangle &rSkTri, SkTriId const requester, SubdivTriangleSkeleton& rSkeleton)
{
    int subdivedNeighbors = 1;

    for (SkTriId const neighborId : rSkTri.neighbors)
    {
        subdivedNeighbors += neighborId.has_value() && int( (neighborId != requester) && (rSkeleton.tri_at(neighborId).children.has_value()) );
    }

    return subdivedNeighbors >= 2;
}

struct SubdivCtxArgs
{
    ACtxTerrain&                rTerrain;
    ACtxTerrainIco&             rTerrainIco;
    ACtxSurfaceFrame&           rSurfaceFrame;
    std::vector<SkTriNewSubdiv>& rNewSubdiv;
    BitVector_t&                rDistanceTestDone;
    int& rDistanceCheckCounts;
    int& rSubdivLevelCount;
};

void unsubdivide()
{
    // TODO
    //   Rule A: dont unsubdiv if there's 2 subdivided neightbours that won't be subdivided
    //           (may need a funny recursive algorithm)
    //   Rule B: don't unsubdivide if there is a subdivided neighbour yada yada

}

void subdivide(SkTriId const sktriId, SkeletonTriangle &rTri, int level, PerSubdivLevel &rLevel, PerSubdivLevel &rNextLevel, SubdivCtxArgs ctx)
{
    // Rule A: if neighbour has 2 subdivided neighbours, subdivide it too
    // Rule B: for corner children (childIndex != 3), parent's neighbours must be subdivided

    SubdivTriangleSkeleton &rSkeleton = ctx.rTerrain.skeleton;
    //SkeletonTriangle &rTri = rSkeleton.tri_at(sktriId);

    LGRN_ASSERTM(!rTri.children.has_value(), "Already subdivided!");

    //LGRN_ASSERTM(! ctx.rDistanceTestDone.test(sktriId.value), "oops! that's not intended to happen");

    std::array<SkTriId, 3> const neighbors { rTri.neighbors[0], rTri.neighbors[1], rTri.neighbors[2] };
    std::array<SkVrtxId, 3> const corners { rTri.vertices[0], rTri.vertices[1], rTri.vertices[2] };

    // Actually do the subdivision
    auto const middles           = std::array<MaybeNewId<SkVrtxId>, 3>{rSkeleton.vrtx_create_middles(corners)};
    auto const [groupId, rGroup] = rSkeleton.tri_subdiv(sktriId, {middles[0].id, middles[1].id, middles[2].id});

    // manual borrow checker hint: rTri becomes invalid here >:)

    // kinda stupid to resize these here, but WHO CARES LOL XD
    bitvector_resize(ctx       .rDistanceTestDone,      rSkeleton.tri_group_ids().capacity() * 4);
    bitvector_resize(rLevel    .hasSubdivedNeighbor,    rSkeleton.tri_group_ids().capacity() * 4);
    bitvector_resize(rLevel    .hasNonSubdivedNeighbor, rSkeleton.tri_group_ids().capacity() * 4);
    bitvector_resize(rNextLevel.hasSubdivedNeighbor,    rSkeleton.tri_group_ids().capacity() * 4);

    ctx.rNewSubdiv.push_back({corners, middles, sktriId, groupId});

    rLevel.hasSubdivedNeighbor.reset(sktriId.value);

    bool hasNonSubdivNeighbor = false;

    // Check neighbours along all 3 edges
    for (int selfEdgeIdx = 0; selfEdgeIdx < 3; ++selfEdgeIdx)
    {
        SkTriId const neighborId = neighbors[selfEdgeIdx];
        if (neighborId.has_value())
        {
            SkeletonTriangle& rNeighbor = rSkeleton.tri_at(neighborId);
            if (rNeighbor.children.has_value())
            {
                // Assign bi-directional connection (neighbor's neighbor)
                int const neighborEdgeIdx = [&neighbors = rNeighbor.neighbors, sktriId] ()
                {
                    if        (neighbors[0].value() == sktriId)   { return 0; }
                    else if   (neighbors[1].value() == sktriId)   { return 1; }
                    else /*if (neighbors[2].value() == sktriId)*/ { return 2; }
                }();

                SkTriGroup &rNeighborGroup = rSkeleton.tri_group_at(rNeighbor.children);

                auto const [selfEdge, neighborEdge]
                    = rSkeleton.tri_group_set_neighboring(
                        {.id = groupId,            .rGroup = rGroup,         .edge = selfEdgeIdx},
                        {.id = rNeighbor.children, .rGroup = rNeighborGroup, .edge = neighborEdgeIdx});

                if (rSkeleton.tri_at(neighborEdge.childB).children.has_value())
                {
                    rNextLevel.hasSubdivedNeighbor.set(selfEdge.childA.value);
                }

                if (rSkeleton.tri_at(neighborEdge.childA).children.has_value())
                {
                    rNextLevel.hasSubdivedNeighbor.set(selfEdge.childB.value);
                }
            }
            else
            {
                // Neighbor is not subdivided
                hasNonSubdivNeighbor = true;
                rLevel.hasSubdivedNeighbor.set(neighborId.value);
            }
        }
    }

    if (hasNonSubdivNeighbor)
    {
        rLevel.hasNonSubdivedNeighbor.set(sktriId.value);
    }
    else
    {
        rLevel.hasNonSubdivedNeighbor.reset(sktriId.value);
    }

    // Check for rule A and rule B violations
    // This can immediately subdivide other triangles recursively
    for (int selfEdgeIdx = 0; selfEdgeIdx < 3; ++selfEdgeIdx)
    {
        SkTriId const neighborId = rSkeleton.tri_at(sktriId).neighbors[selfEdgeIdx];
        if (neighborId.has_value())
        {
            SkeletonTriangle& rNeighbor = rSkeleton.tri_at(neighborId);
            if ( ! rNeighbor.children.has_value() )
            {
                // Non-subdivided neighbor
                if (violates_rule_a(neighborId, rNeighbor, sktriId, rSkeleton))
                {

                    subdivide(neighborId, rSkeleton.tri_at(neighborId), level, rLevel, rNextLevel, ctx);
                    bitvector_resize(ctx.rDistanceTestDone, rSkeleton.tri_group_ids().capacity() * 4);
                    ctx.rDistanceTestDone.set(neighborId.value);
                }
                else
                {
                    if (!ctx.rDistanceTestDone.test(neighborId.value))
                    {
                        rLevel.distanceTestNext.push_back(neighborId);
                        ctx.rDistanceTestDone.set(neighborId.value);
                    }
                }
            }
        }
        else // Neighbour doesn't exist, its parent is not subdivided. Rule B violation
        {
            LGRN_ASSERTM(tri_sibling_index(sktriId) != 3,
                         "Center triangles are always surrounded by their siblings");
            LGRN_ASSERTM(level != 0,
                         "No level above level 0");

            SkTriId const parent = rSkeleton.tri_group_at(tri_group_id(sktriId)).parent;

            LGRN_ASSERTM(parent.has_value(), "bruh");

            auto const& parentNeighbors = rSkeleton.tri_at(parent).neighbors;

            LGRN_ASSERTM(parentNeighbors[selfEdgeIdx].has_value(),
                         "something screwed up XD");

            SkTriId const neighborParent = parentNeighbors[selfEdgeIdx].value();

            // Adds to ctx.rTerrain.levels[level-1].distanceTestNext
            subdivide(neighborParent, rSkeleton.tri_at(neighborParent), level-1, ctx.rTerrain.levels[level-1], rLevel, ctx);
            ctx.rDistanceTestDone.set(neighborParent.value);

            ctx.rTerrain.levelNeedProcess = std::min(ctx.rTerrain.levelNeedProcess, level-1);
        }
    }
};

void subdivide_level(int level, SubdivCtxArgs ctx)
{
    LGRN_ASSERTM(level+1 < ctx.rTerrain.levels.size(), "oops!");
    LGRN_ASSERT(level == ctx.rTerrain.levelNeedProcess);

    // Good-enough bounding sphere is ~75% of the edge length (determined using Blender)
    float         const boundRadiusF   = gc_icoMaxEdgeVsLevel[level] * ctx.rTerrainIco.radius * 0.75f;
    std::uint64_t const boundRadiusU64 = std::uint64_t(boundRadiusF * int_2pow<int>(ctx.rTerrain.scale));

    PerSubdivLevel &rLevel     = ctx.rTerrain.levels[level];
    PerSubdivLevel &rNextLevel = ctx.rTerrain.levels[level+1];

    while ( ! rLevel.distanceTestNext.empty() )
    {
        std::swap(rLevel.distanceTestProcessing, rLevel.distanceTestNext);
        rLevel.distanceTestNext.clear();

        bitvector_resize(ctx.rDistanceTestDone, ctx.rTerrain.skeleton.tri_group_ids().capacity() * 4);
        ctx.rTerrain.sktriCenter.resize(ctx.rTerrain.skeleton.tri_group_ids().capacity() * 4);

        for (SkTriId const sktriId : rLevel.distanceTestProcessing)
        {
            Vector3l const center = ctx.rTerrain.sktriCenter[sktriId];

            LGRN_ASSERT(ctx.rDistanceTestDone.test(sktriId.value));
            bool const distanceNear = is_distance_near(ctx.rSurfaceFrame.position, center, boundRadiusU64);
            ++ctx.rDistanceCheckCounts;

            if (distanceNear)
            {
                bool distanceCheckChildren = false;

                SkeletonTriangle &rTri = ctx.rTerrain.skeleton.tri_at(sktriId);
                if ( rTri.children.has_value() )
                {
                    distanceCheckChildren = true;
                }
                else
                {
                    subdivide(sktriId, rTri, level, rLevel, rNextLevel, ctx);
                    distanceCheckChildren = true;
                }

                SkeletonTriangle &rTriB = ctx.rTerrain.skeleton.tri_at(sktriId);

                if (distanceCheckChildren && level != 8)
                {
                    rNextLevel.distanceTestNext.insert(rNextLevel.distanceTestNext.end(), {
                        tri_id(rTriB.children, 0),
                        tri_id(rTriB.children, 1),
                        tri_id(rTriB.children, 2),
                        tri_id(rTriB.children, 3),
                    });
                    ctx.rDistanceTestDone.set(tri_id(rTriB.children, 0).value);
                    ctx.rDistanceTestDone.set(tri_id(rTriB.children, 1).value);
                    ctx.rDistanceTestDone.set(tri_id(rTriB.children, 2).value);
                    ctx.rDistanceTestDone.set(tri_id(rTriB.children, 3).value);
                }
            }

            // Fix up Rule B violations
            while (ctx.rTerrain.levelNeedProcess != level)
            {
                PerSubdivLevel &rPrevLevel = ctx.rTerrain.levels[ctx.rTerrain.levelNeedProcess];
                subdivide_level(ctx.rTerrain.levelNeedProcess, ctx);
            }
        }
    }

    LGRN_ASSERT(level == ctx.rTerrain.levelNeedProcess);
    ++ctx.rTerrain.levelNeedProcess;

    ++ctx.rSubdivLevelCount;
}

Session setup_terrain(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              scene)
{
    auto const tgScn    = scene         .get_pipelines<PlScene>();

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_TERRAIN);
    auto const tgTrn = out.create_pipelines<PlTerrain>(rBuilder);

    rBuilder.pipeline(tgTrn.skSubdivLoop).parent(tgScn.update);
    rBuilder.pipeline(tgTrn.skeleton)    .parent(tgScn.update);
    rBuilder.pipeline(tgTrn.surfaceFrame).parent(tgScn.update);

    auto &rSurfaceFrame = top_emplace< ACtxSurfaceFrame >(topData, idSurfaceFrame);
    auto &rTerrain      = top_emplace< ACtxTerrain >     (topData, idTerrain);
    auto &rTerrainIco   = top_emplace< ACtxTerrainIco >  (topData, idTerrainIco);

    rBuilder.task()
        .name       ("Initialize terrain when entering planet coordinate space")
        .run_on     ({tgScn.update(Run)})
        .sync_with  ({tgTrn.surfaceFrame(Modify)})
        .push_to    (out.m_tasks)
        .args({                    idSurfaceFrame,             idTerrain,                idTerrainIco })
        .func([] (ACtxSurfaceFrame &rSurfaceFrame, ACtxTerrain &rTerrain, ACtxTerrainIco &rTerrainIco) noexcept
    {
        if ( ! rSurfaceFrame.active )
        {
            rSurfaceFrame.active = true;

            init_ico_terrain(rTerrain, rTerrainIco, 50.0f, 2.0f, 10);
        }
    });

    static bool lazy = false;

    rBuilder.task()
        .name       ("subdivide triangle skeleton")
        .run_on     ({tgTrn.skSubdivLoop(Run_)})
        .sync_with  ({tgTrn.surfaceFrame(Ready), tgTrn.skeleton(New)})
        .push_to    (out.m_tasks)
        .args({                    idSurfaceFrame,             idTerrain,                idTerrainIco })
        .func([] (ACtxSurfaceFrame &rSurfaceFrame, ACtxTerrain &rTerrain, ACtxTerrainIco &rTerrainIco) noexcept -> TaskActions
    {
        if ( ! rSurfaceFrame.active )
        {
            return TaskAction::Cancel;
        }

        static float t = 0;
        t += 0.005f;

        //rSurfaceFrame.position = Vector3l(Vector3(0, 0, 2.0f - t) * rTerrainIco.radius * int_2pow<int>(rTerrain.scale));

        auto a = Quaternion::rotation(Rad{t},                   Vector3::zAxis())
               * Quaternion::rotation(Rad{std::cos(5.0f*t)},    Vector3::yAxis())
               * Quaternion::rotation(Rad{0.0f},                Vector3::xAxis());

        rSurfaceFrame.position = Vector3l(a.transformVector(Vector3(0.0f, 0.0f, 1.0f)) * rTerrainIco.radius * int_2pow<int>(rTerrain.scale));

        if (lazy) {
        //    return TaskActions{};
        }
        lazy = true;

        SubdivTriangleSkeleton &rSkeleton = rTerrain.skeleton;

        BitVector_t tryUnsubdiv;
        bitvector_resize(tryUnsubdiv, rTerrain.skeleton.tri_group_ids().capacity() * 4);

        BitVector_t distanceTestDone;
        bitvector_resize(distanceTestDone, rTerrain.skeleton.tri_group_ids().capacity() * 4);

        std::vector<SkTriId> selected;

        for (int level = rTerrain.levels.size()-1; level >= 0; --level)
        {
            // Good-enough bounding sphere is ~75% of the edge length (determined using Blender)
            float         const boundRadiusF   = gc_icoMaxEdgeVsLevel[level] * rTerrainIco.radius * 0.75f;
            std::uint64_t const boundRadiusU64 = std::uint64_t(boundRadiusF * int_2pow<int>(rTerrain.scale));

            PerSubdivLevel &rLevel = rTerrain.levels[level];

            LGRN_ASSERT(rLevel.distanceTestNext.size() == 0);

            // Step 1: Floodfill-select all triangles in this level that might be unsubdivided
            //         (distance too far)

            tryUnsubdiv.reset();

            for (std::size_t const sktriInt : rLevel.hasNonSubdivedNeighbor.ones())
            {
                rLevel.distanceTestNext.push_back(SkTriId(sktriInt));
                distanceTestDone.set(sktriInt);
            }

            while (rLevel.distanceTestNext.size() != 0)
            {
                std::swap(rLevel.distanceTestProcessing, rLevel.distanceTestNext);
                rLevel.distanceTestNext.clear();

                for (SkTriId const sktriId : rLevel.distanceTestProcessing)
                {
                    Vector3l const center = rTerrain.sktriCenter[sktriId];
                    bool const tooFar = ! is_distance_near(rSurfaceFrame.position, center, boundRadiusU64);

                    LGRN_ASSERTM(rTerrain.skeleton.tri_at(sktriId).children.has_value(),
                                 "Non-subdivided triangles must not be added to distance test.");

                    if (tooFar)
                    {
                        tryUnsubdiv.set(sktriId.value);

                        SkeletonTriangle const& sktri = rTerrain.skeleton.tri_at(sktriId);
                        for (int edge = 0; edge < 3; ++edge)
                        {
                            SkTriId const neighbor = sktri.neighbors[edge];
                            if (   neighbor.has_value()
                                && distanceTestDone.test(neighbor.value) == false
                                && rTerrain.skeleton.tri_at(neighbor).children.has_value())
                            {
                                // Neighbor is subdivided, distance test them next
                                rLevel.distanceTestNext.push_back(neighbor);
                                distanceTestDone.set(neighbor.value);
                            }
                        }
                    }
                }
            }

            std::cout << "lvl:" << level << " filled:" << tryUnsubdiv.count() << "\n";

        }

        distanceTestDone.reset();

        for (int level = 0; level < rTerrain.levels.size(); ++level)
        {
            LGRN_ASSERT(rTerrain.levels[level].distanceTestNext.empty());
        }


        std::vector<SkTriNewSubdiv> calc;

        int distanceCheckCount = 0;
        int subdivLevelCount = 0;
        SubdivCtxArgs ctx { rTerrain, rTerrainIco, rSurfaceFrame, calc, distanceTestDone, distanceCheckCount, subdivLevelCount };

//        for (int level = 0; level < rTerrain.levels.size()-1; ++level)
//        {
//            PerSubdivLevel &rLevel = rTerrain.levels[level];

//            bool nothingHappened = true;
//            for (std::size_t const sktriInt : rLevel.hasSubdivedNeighbor.ones())
//            {
//                rLevel.distanceTestNext.push_back(SkTriId(sktriInt));
//                distanceTestDone.set(sktriInt);
//                nothingHappened = false;
//            }

//            if (nothingHappened)
//            {
//                if (level == 0)
//                {
//                    rTerrain.levels[0].distanceTestNext.assign(rTerrainIco.icoTri.begin(), rTerrainIco.icoTri.end());
//                    for (SkTriId const id : rTerrainIco.icoTri)
//                    {
//                        distanceTestDone.set(id.value);
//                    }
//                }
//            }
//        }

        PerSubdivLevel &rRootLevel = rTerrain.levels[0];
        for (SkTriId const sktriId : rTerrainIco.icoTri)
        {
            rRootLevel.distanceTestNext.push_back(sktriId);
            distanceTestDone.set(sktriId.value);
        }

        ctx.rTerrain.levelNeedProcess = 0;

        for (int level = 0; level < rTerrain.levels.size()-1; ++level)
        {
            if (level > 6)
            {
                // >:)
                rTerrain.levels[level].distanceTestNext.clear();
            }

            subdivide_level(level, ctx);

            LGRN_ASSERT(rTerrain.levels[level].distanceTestNext.empty());

            for (int levelb = 0; levelb < level+1; ++levelb)
            {
                // this asserts since some subdivide calls request distance-checks with lower levels without calling subdivide_level afterwards
                LGRN_ASSERT(rTerrain.levels[levelb].distanceTestNext.empty());
            }

            auto const capacity = rTerrain.skeleton.vrtx_ids().capacity();
            rTerrain.skPositions.resize(capacity);
            rTerrain.skNormals  .resize(capacity);

            // stupid reminder: each triangle group is 4 triangles
            rTerrain.sktriCenter.resize(rTerrain.skeleton.tri_group_ids().capacity() * 4);

            for (auto const& [corners, middles, sktriId, groupId] : calc)
            {
                ico_calc_middles(rTerrainIco.radius, rTerrain.scale, corners, middles, rTerrain.skPositions, rTerrain.skNormals);
                calculate_centers(groupId, rTerrain, rTerrainIco.radius + rTerrainIco.height, rTerrainIco.height);
            }
        }

        // Verify rules are satisfied
        // iterate all existing triangles
        for (std::size_t i = 0; i < rTerrain.skeleton.tri_group_ids().capacity() * 4; ++i)
        if (SkTriId const sktriId = SkTriId(i);
            rTerrain.skeleton.tri_group_ids().exists(tri_group_id(sktriId)))
        {
            SkeletonTriangle const& sktri = rTerrain.skeleton.tri_at(sktriId);

            if (!sktri.children.has_value()) // if not subdivided
            {
                int subdivedNeighbors = 0;
                for (int edge = 0; edge < 3; ++edge)
                {
                    SkTriId const neighbor = sktri.neighbors[edge];
                    if (neighbor.has_value())
                    {
                        if (rTerrain.skeleton.tri_at(neighbor).children.has_value())
                        {
                            ++subdivedNeighbors;
                        }
                    }
                    else
                    {
                        // Neighbor doesn't exist. parent MUST have neighbor
                        SkTriId const parent = rSkeleton.tri_group_at(tri_group_id(sktriId)).parent;
                        LGRN_ASSERTM(parent.has_value(), "bruh");
                        auto const& parentNeighbors = rSkeleton.tri_at(parent).neighbors;
                        LGRN_ASSERTM(parentNeighbors[edge].has_value(), "Rule B Violation");

                        LGRN_ASSERTM(rTerrain.skeleton.tri_at(parentNeighbors[edge]).children.has_value() == false,
                                     "Incorrectly set neighbors");
                    }
                }

                LGRN_ASSERTM(subdivedNeighbors < 2, "Rule A Violation");
            }
        }

        std::cout << "total distance checks: " << distanceCheckCount << "\n";
        std::cout << "total subdiv levels: " << subdivLevelCount << "\n";

        return true ? TaskActions{} : TaskAction::Cancel;
    });

    return out;
}


struct TerrainDebugDraw
{
    KeyedVec<SkVrtxId, osp::draw::DrawEnt> verts;
    MaterialId mat;
};

Session setup_terrain_debug_draw(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              windowApp,
        Session const&              sceneRenderer,
        Session const&              cameraCtrl,
        Session const&              commonScene,
        Session const&              terrain,
        MaterialId const            mat)
{
    OSP_DECLARE_GET_DATA_IDS(commonScene,    TESTAPP_DATA_COMMON_SCENE);
    OSP_DECLARE_GET_DATA_IDS(sceneRenderer,  TESTAPP_DATA_SCENE_RENDERER);
    OSP_DECLARE_GET_DATA_IDS(cameraCtrl,     TESTAPP_DATA_CAMERA_CTRL);
    OSP_DECLARE_GET_DATA_IDS(terrain,        TESTAPP_DATA_TERRAIN);

    auto const tgScnRdr = sceneRenderer.get_pipelines<PlSceneRenderer>();

    Session out;
    auto const [idTrnDbgDraw] = out.acquire_data<1>(topData);

    auto &rTrnDbgDraw = top_emplace< TerrainDebugDraw > (topData, idTrnDbgDraw);

    rTrnDbgDraw.mat = mat;

    rBuilder.task()
        .name       ("do spooky scary skeleton stuff")
        .run_on     ({tgScnRdr.render(Run)})
        .sync_with  ({tgScnRdr.drawTransforms(Modify_), tgScnRdr.entMeshDirty(Modify_), tgScnRdr.drawEntResized(ModifyOrSignal)})
        .push_to    (out.m_tasks)
        .args       ({        idDrawing,                 idScnRender,             idNMesh,                  idTrnDbgDraw,             idTerrain,                idTerrainIco})
        .func([] (ACtxDrawing& rDrawing, ACtxSceneRender& rScnRender, NamedMeshes& rNMesh, TerrainDebugDraw& rTrnDbgDraw, ACtxTerrain &rTerrain, ACtxTerrainIco &rTerrainIco) noexcept
    {

        Material &rMatPlanet = rScnRender.m_materials[rTrnDbgDraw.mat];
        MeshId const cubeMeshId   = rNMesh.m_shapeToMesh.at(EShape::Box);

        rTrnDbgDraw.verts.resize(rTerrain.skeleton.vrtx_ids().capacity());

        auto const view = rTerrain.skeleton.vrtx_ids().bitview();
        for (std::size_t const skVertInt : view.zeros())
        if (auto const skVert = SkVrtxId(skVertInt);
            skVert != lgrn::id_null<SkVrtxId>())
        {
            DrawEnt &rDrawEnt = rTrnDbgDraw.verts[skVert];
            if (rDrawEnt == lgrn::id_null<DrawEnt>())
            {
                rDrawEnt = rScnRender.m_drawIds.create();
            }
        }

        rScnRender.resize_draw(); // >:)

        for (std::size_t const skVertInt : view.zeros())
        if (auto const skVert = SkVrtxId(skVertInt);
            skVert != lgrn::id_null<SkVrtxId>())
        {
            DrawEnt const drawEnt = rTrnDbgDraw.verts[skVert];

            if (rScnRender.m_mesh[drawEnt].has_value() == false)
            {
                rScnRender.m_mesh[drawEnt] = rDrawing.m_meshRefCounts.ref_add(cubeMeshId);
                rScnRender.m_meshDirty.push_back(drawEnt);
                rScnRender.m_visible.set(drawEnt.value);
                rScnRender.m_opaque.set(drawEnt.value);
                rMatPlanet.m_ents.set(drawEnt.value);
                rMatPlanet.m_dirty.push_back(drawEnt);
            }


            rScnRender.m_drawTransform[drawEnt]
                = Matrix4::translation(Vector3(rTerrain.skPositions[skVert]) / int_2pow<int>(rTerrain.scale))
                * Matrix4::scaling({0.1f, 0.1f, 0.1f});
        }
    });

    return out;
}

} // namespace testapp::scenes
