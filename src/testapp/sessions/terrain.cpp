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

#include <planet-a/chunk_generate.h>
#include <planet-a/chunk_utils.h>
#include <planet-a/icosahedron.h>

#include <adera/drawing/CameraController.h>

#include <osp/core/math_2pow.h>
#include <osp/core/math_int64.h>
#include <osp/drawing/drawing.h>

#include <longeron/utility/asserts.hpp>

#include <chrono>
#include <fstream>

using namespace adera;
using namespace planeta;
using namespace osp;
using namespace osp::math;
using namespace osp::draw;

namespace testapp::scenes
{

Session setup_terrain(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any>  const topData,
        Session               const &scene,
        Session               const &commonScene)
{
    OSP_DECLARE_GET_DATA_IDS(commonScene, TESTAPP_DATA_COMMON_SCENE);
    auto const tgScn = scene.get_pipelines<PlScene>();

    auto &rDrawing = top_get<ACtxDrawing>(topData, idDrawing);

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_TERRAIN);
    auto const tgTrn = out.create_pipelines<PlTerrain>(rBuilder);

    rBuilder.pipeline(tgTrn.skeleton)       .parent(tgScn.update);
    rBuilder.pipeline(tgTrn.surfaceChanges) .parent(tgScn.update);
    rBuilder.pipeline(tgTrn.terrainFrame)   .parent(tgScn.update);
    rBuilder.pipeline(tgTrn.chunkMesh)      .parent(tgScn.update);

    auto &rTerrainFrame = top_emplace< ACtxTerrainFrame >(topData, idTerrainFrame);
    auto &rTerrain      = top_emplace< ACtxTerrain >     (topData, idTerrain);

    rTerrain.terrainMesh = rDrawing.m_meshRefCounts.ref_add(rDrawing.m_meshIds.create());

    rBuilder.task()
        .name       ("Clear surfaceAdded & surfaceRemoved once we're done with it")
        .run_on     ({tgTrn.surfaceChanges(Clear)})
        .push_to    (out.m_tasks)
        .args({               idTerrain })
        .func([] (ACtxTerrain &rTerrain) noexcept
    {
        rTerrain.scratchpad.surfaceAdded  .clear();
        rTerrain.scratchpad.surfaceRemoved.clear();
    });

    rBuilder.task()
        .name       ("Clean up terrain-related IdOwners")
        .run_on     ({tgScn.cleanup(Run_)})
        .push_to    (out.m_tasks)
        .args       ({        idTerrain,             idDrawing})
        .func([] (ACtxTerrain &rTerrain, ACtxDrawing &rDrawing) noexcept
    {
        // rTerrain.skChunks has owners referring to rTerrain.skeleton. A reference to
        // rTerrain.skeleton can't be obtained during destruction, we must clear it separately.
        rTerrain.skChunks.clear(rTerrain.skeleton);

        // rTerrain.skeleton will clean itself up in its destructor, since it only holds owners
        // referring to itself.

        rDrawing.m_meshRefCounts.ref_release(std::move(rTerrain.terrainMesh));
    });

    return out;
}

Session setup_terrain_icosahedron(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any>  const topData,
        Session               const &terrain)
{
    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_TERRAIN_ICO);
    top_emplace< ACtxTerrainIco >     (topData, idTerrainIco);
    return out;
}

Session setup_terrain_subdiv_dist(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any>  const topData,
        Session               const &scene,
        Session               const &terrain,
        Session               const &terrainIco)
{
    OSP_DECLARE_GET_DATA_IDS(terrain,    TESTAPP_DATA_TERRAIN);
    OSP_DECLARE_GET_DATA_IDS(terrainIco, TESTAPP_DATA_TERRAIN_ICO);

    auto const tgTrn = terrain  .get_pipelines<PlTerrain>();
    auto const tgScn = scene    .get_pipelines<PlScene>();

    Session out;

    rBuilder.task()
        .name       ("Subdivide triangle skeleton")
        .run_on     ({tgScn.update(Run)})
        .sync_with  ({tgTrn.terrainFrame(Ready), tgTrn.skeleton(New), tgTrn.surfaceChanges(Resize)})
        .push_to    (out.m_tasks)
        .args({                    idTerrainFrame,             idTerrain,                idTerrainIco })
        .func([] (ACtxTerrainFrame &rTerrainFrame, ACtxTerrain &rTerrain, ACtxTerrainIco &rTerrainIco) noexcept
    {
        if ( ! rTerrainFrame.active )
        {
            return;
        }

        SubdivTriangleSkeleton     &rSkel      = rTerrain.skeleton;
        SkeletonVertexData         &rSkData    = rTerrain.skData;
        BasicChunkMeshGeometry     &rChGeo     = rTerrain.chunkGeom;
        SkeletonSubdivScratchpad   &rSkSP      = rTerrain.scratchpad;

        Vector3l const& viewerPos = rTerrain.scratchpad.viewerPosition;

        // ## Unsubdivide triangles that are too far away

        // Unsubdivide is performed first, since it's better to remove stuff before adding new
        // stuff; this reduces peak memory requirements.

        // Unsubdividing is performed per-level, starting from the highest detail. In order to
        // respect invariants, triangles must be removed in groups (removing triangles 1-by-1 can
        // violate invariants mid-way).
        for (int level = rSkel.levelMax-1; level >= 0; --level)
        {
            // Select and deselect only modifies rSkSP
            unsubdivide_select_by_distance(level, viewerPos, rSkel, rSkData, rSkSP);
            unsubdivide_deselect_invariant_violations(level, rSkel, rSkData, rSkSP);

            // Perform changes on skeleton, delete selected triangles
            unsubdivide_level(level, rSkel, rSkData, rSkSP);
        }
        rSkSP.distanceTestDone.clear();

        // ## Subdivide nearby triangles

        // Distance testing is performed 'recursively' per level. A triangle within the subdivision
        // threshold and needs to be subdivided, will trigger a subdivision check for its children
        // on the next level. To start, we seed the distance checker with the root triangles.
        if (rSkel.levelMax > 0)
        {
            for (SkTriId const sktriId : rTerrainIco.icoTri)
            {
                rSkSP.levels[0].distanceTestNext.push_back(sktriId);
                rSkSP.distanceTestDone.insert(sktriId);
            }
            rSkSP.levelNeedProcess = 0;
        }

        // Do the subdivide for real
        for (int level = 0; level < rSkel.levelMax; ++level)
        {
            subdivide_level_by_distance(viewerPos, level, rSkel, rSkData, rSkSP);
        }
        rSkSP.distanceTestDone.clear();

        // Uncomment these if some new change breaks something
        //rSkel.debug_check_invariants();
    });

    rBuilder.task()
        .name       ("Update Terrain Chunks")
        .run_on     ({tgScn.update(Run)})
        .sync_with  ({tgTrn.terrainFrame(Ready), tgTrn.skeleton(Ready), tgTrn.surfaceChanges(UseOrRun), tgTrn.chunkMesh(Modify)})
        .push_to    (out.m_tasks)
        .args({                    idTerrainFrame,             idTerrain,                idTerrainIco })
        .func([] (ACtxTerrainFrame &rTerrainFrame, ACtxTerrain &rTerrain, ACtxTerrainIco &rTerrainIco) noexcept
    {
        if ( ! rTerrainFrame.active )
        {
            return;
        }

        SubdivTriangleSkeleton     &rSkel      = rTerrain.skeleton;
        SkeletonVertexData         &rSkData    = rTerrain.skData;
        ChunkSkeleton              &rSkCh      = rTerrain.skChunks;
        ChunkMeshBufferInfo        &rChInfo    = rTerrain.chunkInfo;
        BasicChunkMeshGeometry     &rChGeo     = rTerrain.chunkGeom;
        ChunkScratchpad            &rChSP      = rTerrain.chunkSP;
        SkeletonSubdivScratchpad   &rSkSP      = rTerrain.scratchpad;

        rChSP.chunksAdded       .clear();
        rChSP.chunksRemoved     .clear();
        rChSP.sharedNormalsDirty.clear();
        rChSP.sharedAdded       .clear();
        rChSP.sharedRemoved     .clear();

        // Delete chunks of now-deleted Skeleton Triangles
        for (SkTriId const sktriId : rSkSP.surfaceRemoved)
        {
            ChunkId const chunkId = rSkCh.m_triToChunk[sktriId];
            if (chunkId.has_value())
            {
                subtract_normal_contrib(chunkId, false, rChGeo, rChInfo, rChSP, rSkCh);
                rSkCh.chunk_remove(chunkId, sktriId, rChSP.sharedRemoved, rSkel);
                rChSP.chunksRemoved.insert(chunkId);
            }
        }

        auto const chLevel  = rSkCh.m_chunkSubdivLevel;
        auto const edgeSize = rSkCh.m_chunkEdgeVrtxCount-1;

        // Create new chunks for each new surface Skeleton Triangle added
        rSkCh.m_triToChunk.resize(rSkel.tri_group_ids().capacity() * 4);
        for (SkTriId const sktriId : rSkSP.surfaceAdded)
        {
            auto const &corners = rSkel.tri_at(sktriId).vertices;

            ArrayView< MaybeNewId<SkVrtxId> > const edgeVrtxView = rChSP.edgeVertices;
            ArrayView< MaybeNewId<SkVrtxId> > const edgeLft = edgeVrtxView.sliceSize(edgeSize * 0, edgeSize);
            ArrayView< MaybeNewId<SkVrtxId> > const edgeBtm = edgeVrtxView.sliceSize(edgeSize * 1, edgeSize);
            ArrayView< MaybeNewId<SkVrtxId> > const edgeRte = edgeVrtxView.sliceSize(edgeSize * 2, edgeSize);

            rSkel.vrtx_create_chunk_edge_recurse(chLevel, corners[0], corners[1], edgeLft);
            rSkel.vrtx_create_chunk_edge_recurse(chLevel, corners[1], corners[2], edgeBtm);
            rSkel.vrtx_create_chunk_edge_recurse(chLevel, corners[2], corners[0], edgeRte);

            ChunkId const chunkId = rSkCh.chunk_create(sktriId, rSkel, rChSP.sharedAdded, edgeLft, edgeBtm, edgeRte);

            rChSP.chunksAdded.insert(chunkId);

            // chunk_create creates new Skeleton Vertices. Resize is needed after each call
            rSkData.resize(rSkel);
            rSkSP.resize(rSkel);

            // Calculates positions and normals with spherical curvature
            ico_calc_chunk_edge(rTerrainIco.radius, chLevel, corners[0], corners[1], edgeLft, rSkData);
            ico_calc_chunk_edge(rTerrainIco.radius, chLevel, corners[1], corners[2], edgeBtm, rSkData);
            ico_calc_chunk_edge(rTerrainIco.radius, chLevel, corners[2], corners[0], edgeRte, rSkData);
        }

        for (ChunkId const chunkId : rChSP.chunksAdded)
        {
            restitch_check(chunkId, rSkCh.m_chunkToTri[chunkId], rSkCh, rSkel, rSkData, rChSP);
        }

        float const scalepow = std::pow(2.0f, -rSkData.precision);

        auto const vbufPosView = rChGeo.vbufPositions.view(rChGeo.vrtxBuffer, rChInfo.vrtxTotal);
        auto const vbufNrmView = rChGeo.vbufNormals  .view(rChGeo.vrtxBuffer, rChInfo.vrtxTotal);

        // TODO: temporary code of course
        auto const heightmap = [scalepow, h = rTerrainIco.height] (Vector3l posl) -> float
        {
            return h * std::clamp<double>(    0.1*(0.5 - 0.5*std::cos(0.000050*posl.x()*scalepow*2.0*3.14159))
                                            + 0.9*(0.5 - 0.5*std::cos(0.000005*posl.y()*scalepow*2.0*3.14159)) , 0.0, 1.0 );
        };

        auto const update_shared_vrtx_position
                = [&vbufPosView, &heightmap, scalepow, &rSkCh, &rChInfo, &rSkData, &rChGeo, &rTerrainIco]
                  (SharedVrtxId const sharedVrtxId)
        {
            SkVrtxId  const skelVrtx   = rSkCh.m_sharedToSkVrtx[sharedVrtxId];
            VertexIdx const vbufVertex = rChInfo.vbufSharedOffset + sharedVrtxId.value;
            Vector3l  const skPos      = rSkData.positions[skelVrtx];
            Vector3   const posOut     = Vector3{skPos - rChGeo.originSkelPos} * scalepow;
            Vector3   const radialDir  = Vector3{Vector3d(skPos) * scalepow / rTerrainIco.radius};

            rChGeo.sharedPosNoHeightmap[sharedVrtxId] = posOut;
            vbufPosView[vbufVertex]                   = posOut + radialDir * heightmap(skPos);
        };

        if (true || rChGeo.originSkelPos == rTerrainFrame.position)
        {
            // Copy offsetted positions from the skeleton for newly added shared vertices

            for (SharedVrtxId const sharedVrtxId : rChSP.sharedAdded)
            {
                update_shared_vrtx_position(sharedVrtxId);
            }
        }
        else
        {
            // The scene position relative to planet origin has changed.
            OSP_LOG_INFO("Translating Terrain Mesh");

            Vector3l const deltaOffset  = rChGeo.originSkelPos - rTerrainFrame.position;
            Vector3  const deltaOffsetF = Vector3(deltaOffset) * scalepow;
            rChGeo.originSkelPos = rTerrainFrame.position;

            // Refresh all shared vertex positions
            for (SharedVrtxId const sharedVrtxId : rSkCh.m_sharedIds)
            {
                update_shared_vrtx_position(sharedVrtxId);
            }

            // Translate all existing chunk fill vertices
            for (ChunkId const chunkId : rSkCh.m_chunkIds)
            {
                if (rChSP.chunksAdded.contains(chunkId))
                {
                    continue; // Not added yet, no need to translate
                }

                std::size_t const fillOffset = rChInfo.vbufFillOffset + chunkId.value*rChInfo.fillVrtxCount;
                for (Vector3 &rPos : vbufPosView.sliceSize(fillOffset, rChInfo.fillVrtxCount))
                {
                    rPos += deltaOffsetF;
                }
            }
        }

        Vector3d const center = -Vector3d(rChGeo.originSkelPos) * scalepow;

        // Calculate new fill vertex positions
        for (ChunkId const chunkId : rChSP.chunksAdded)
        {
            std::size_t const fillOffset = rChInfo.vbufFillOffset + chunkId.value*rChInfo.fillVrtxCount;
            osp::ArrayView<SharedVrtxOwner_t const> sharedUsed = rSkCh.shared_vertices_used(chunkId);

            // Use ChunkFillSubdivLUT to generate a spherically curved triangle fill through
            // building up and subdividing pairs of vertices. Don't apply heightmap yet, as this
            // will interfere with middle position and curvature calculations.
            for (ChunkFillSubdivLUT::ToSubdiv const& toSubdiv : rChSP.lut.data())
            {
                Vector3 const vrtxAPos = toSubdiv.aIsShared
                                       ? rChGeo.sharedPosNoHeightmap[sharedUsed[toSubdiv.vrtxA]]
                                       : vbufPosView[fillOffset + toSubdiv.vrtxA];
                Vector3 const vrtxBPos = toSubdiv.bIsShared
                                       ? rChGeo.sharedPosNoHeightmap[sharedUsed[toSubdiv.vrtxB]]
                                       : vbufPosView[fillOffset + toSubdiv.vrtxB];

                Vector3d    const middle     = 0.5*( Vector3d(vrtxAPos) + Vector3d(vrtxBPos) );
                Vector3d    const centerDiff = Vector3d(middle) - center;
                double      const centerDist = centerDiff.length();
                Vector3d    const radialDir  = centerDiff / centerDist;
                double      const roundness  = rTerrainIco.radius - centerDiff.length();
                Vector3d    const posOut     = middle + radialDir * roundness;

                vbufPosView[fillOffset + toSubdiv.fillOut] = Vector3(posOut);
            }

            // Apply heightmap afterwards
            for (Vector3 &rPos : vbufPosView.sliceSize(fillOffset, rChInfo.fillVrtxCount))
            {
                Vector3d   const centerDiff = Vector3d(rPos) - center;
                double     const centerDist = centerDiff.length();
                Vector3    const radialDir  = Vector3{centerDiff / centerDist};

                Vector3l const bigpos = Vector3l(rPos / scalepow) + rChGeo.originSkelPos;

                rPos += radialDir * heightmap(bigpos);
            }
        }

        // Normal is not cleaned up by the previous user; Initially set them to zero.
        // Face normals added in update_faces(...) will accumulate here.
        for (SharedVrtxId const sharedVrtxId : rChSP.sharedAdded)
        {
            rChGeo.sharedNormalSum[sharedVrtxId] = Vector3{ZeroInit};
        }

        // Update Index buffer

        // Add or remove faces according to chunk changes. This also calculates normals.
        // Vertex normals are calculated from a weighted sum of face normals of connected faces.
        // For shared vertices, we add or subtract face normals from rChGeo.sharedNormalSum.
        for (ChunkId const chunkId : rSkCh.m_chunkIds)
        {
            SkTriId const sktriId    = rSkCh.m_chunkToTri[chunkId];
            bool    const newlyAdded = rSkSP.surfaceAdded.contains(sktriId);

            update_faces(chunkId, sktriId, newlyAdded, rSkel, rSkData, rChGeo, rChInfo, rChSP, rSkCh);
        }
        std::fill(rChSP.stitchCmds.begin(), rChSP.stitchCmds.end(), ChunkStitch{});

        // Fill unused parts of the index buffer with zeros. This includes chunks that are deleted
        // but not (yet) reused.
        auto const ibuf2d = as_2d(rChGeo.indxBuffer, rChInfo.chunkMaxFaceCount);
        for (ChunkId const chunkId : rChSP.chunksRemoved)
        {
            if ( ! rSkCh.m_chunkIds.exists(chunkId) )
            {
                auto const indicesView = ibuf2d.row(chunkId.value);
                std::fill(indicesView.begin(), indicesView.end(), Vector3u{0, 0, 0});
            }
        }

        // Update vertex buffer normals of shared vertices, as rChGeo.sharedNormalSum was modified.
        for (SharedVrtxId const sharedId : rChSP.sharedNormalsDirty)
        {
            Vector3 const normalSum = rChGeo.sharedNormalSum[sharedId];
            vbufNrmView[rChInfo.vbufSharedOffset + sharedId.value] = normalSum.normalized();
        }

        // Uncomment these if some new change breaks something
        //debug_check_invariants(rChGeo, rChInfo, rSkCh);


        static unsigned int fish = 1;
        ++fish;

        // TODO: temporary, write statistics about every second
        if (fish % 60 == 0)
        {
            OSP_LOG_INFO("Terrain stats: \n"
                         "* Skeleton Triangles:   {}\n"
                         "* Skeleton Vertices:    {}\n"
                         "* Chunks:               {}/{}\n"
                         "* Shared Vertices:      {}/{}\n",
                         rSkel.tri_group_ids().size()*4, rSkel.vrtx_ids().size(),
                         rSkCh.m_chunkIds.size(), rSkCh.m_chunkIds.capacity(),
                         rSkCh.m_sharedIds.size(), rSkCh.m_sharedIds.capacity());
        }

        /*
        // TODO: temporary, write debug obj file every ~10 seconds
        if (fish % (60*10) == 0)
        {
            auto        const time     = std::chrono::system_clock::now().time_since_epoch().count();
            std::string const filename = fmt::format("planetdebug_{}.obj", time);

            OSP_LOG_INFO("Writing planet terrain obj: {}\n"
                         "* Chunks:          {}/{}\n"
                         "* Shared Vertices: {}/{}\n",
                         filename,
                         rSkCh.m_chunkIds.size(), rSkCh.m_chunkIds.capacity(),
                         rSkCh.m_sharedIds.size(), rSkCh.m_sharedIds.capacity() );

            std::ofstream objfile;
            objfile.open(filename);
            write_obj(objfile, rTerrain.chunkGeom, rChInfo, rSkCh);
        }
        */
    });

    return out;
}

void initialize_ico_terrain(
        ArrayView<entt::any>   const topData,
        Session                const &terrain,
        Session                const &terrainIco,
        TerrainTestPlanetSpecs const specs)
{
    OSP_DECLARE_GET_DATA_IDS(terrain,    TESTAPP_DATA_TERRAIN);
    OSP_DECLARE_GET_DATA_IDS(terrainIco, TESTAPP_DATA_TERRAIN_ICO);

    auto &rTerrain          = top_get<ACtxTerrain>      (topData, idTerrain);
    auto &rTerrainFrame     = top_get<ACtxTerrainFrame> (topData, idTerrainFrame);
    auto &rTerrainIco       = top_get<ACtxTerrainIco>   (topData, idTerrainIco);

    rTerrainFrame.active = true;

    // ## Create initial icosahedron skeleton

    rTerrainIco.radius          = specs.radius;
    rTerrainIco.height          = specs.height;
    rTerrain.skData.precision   = specs.skelPrecision;
    rTerrain.skeleton = create_skeleton_icosahedron(
            rTerrainIco.radius,
            rTerrainIco.icoVrtx,
            rTerrainIco.icoGroups,
            rTerrainIco.icoTri,
            rTerrain.skData);

    rTerrain.skeleton.levelMax = specs.skelMaxSubdivLevels;

    // ## Assign skeleton icosahedron position data

    rTerrain.skData.resize(rTerrain.skeleton);

    double const scale = std::pow(2.0, rTerrain.skData.precision);
    double const maxRadius = rTerrainIco.radius + rTerrainIco.height;

    for (SkTriGroupId const groupId : rTerrainIco.icoGroups)
    {
        ico_calc_sphere_tri_center(groupId, maxRadius, rTerrainIco.height, rTerrain.skeleton, rTerrain.skData);
    }

    // ## Prepare the skeleton subdiv scratchpad.
    // This contains intermediate variables used when subdividing the triangle skeleton.

    SkeletonSubdivScratchpad &rSP = rTerrain.scratchpad;
    rSP.resize(rTerrain.skeleton);
    for (SkTriGroupId const groupId : rTerrainIco.icoGroups)
    {
        // Notify subsequent functions of the newly added initial icosahedron faces
        rSP.surfaceAdded.insert(tri_id(groupId, 0));
        rSP.surfaceAdded.insert(tri_id(groupId, 1));
        rSP.surfaceAdded.insert(tri_id(groupId, 2));
        rSP.surfaceAdded.insert(tri_id(groupId, 3));
    }

    // Set function pointer to apply spherical curvature to the skeleton on subdivision.
    // Spherical planets are not hard-coded into subdivision logic, it's intended to work
    // to work for non-spherical shapes too.
    rTerrain.scratchpad.onSubdivUserData[0] = &rTerrainIco;
    rSP.onSubdiv = [] (
            SkTriId                             tri,
            SkTriGroupId                        groupId,
            std::array<SkVrtxId, 3>             corners,
            std::array<MaybeNewId<SkVrtxId>, 3> middles,
            SubdivTriangleSkeleton              &rSkel,
            SkeletonVertexData                  &rSkData,
            SkeletonSubdivScratchpad::UserData_t userData) noexcept
    {
        auto const& rTerrainIco = *reinterpret_cast<ACtxTerrainIco*>(userData[0]);
        ico_calc_middles(rTerrainIco.radius, corners, middles, rSkData);
        ico_calc_sphere_tri_center(groupId, rTerrainIco.radius + rTerrainIco.height, rTerrainIco.height, rSkel, rSkData);
    };

    // Nothing to do on un-subdivide
    rSP.onUnsubdiv = [] (
            SkTriId                         tri,
            SkeletonTriangle                &rTri,
            SubdivTriangleSkeleton          &rSkel,
            SkeletonVertexData              &rSkData,
            SkeletonSubdivScratchpad::UserData_t userData) noexcept
    { };

    // Calculate distance thresholds for when skeleton triangles should be subdivided and
    // unsubdivided. These threshold values are used by
    // subdivide_level_by_distance(...) and unsubdivide_select_by_distance(...)
    for (int level = 0; level < gc_maxSubdivLevels; ++level)
    {
        // Good-enough bounding sphere is ~75% of the edge length (determined using Blender)
        float const edgeLength = gc_icoMaxEdgeVsLevel[level] * rTerrainIco.radius * scale;
        float const subdivRadius = 0.75f * edgeLength;

        // TODO: Pick thresholds based on the angular diameter (size on screen) of the
        //       chunk triangle mesh that will actually be rendered.
        rSP.distanceThresholdSubdiv[level] = std::uint64_t(subdivRadius);

        // Unsubdivide thresholds should be slightly larger (arbitrary x2) to avoid rapid
        // terrain changes when moving back and forth quickly
        rSP.distanceThresholdUnsubdiv[level] = std::uint64_t(2.0f * subdivRadius);
    }

    // ## Prepare Chunk Skeleton

    std::uint8_t const chunkSubdivLevels = specs.chunkSubdivLevels;

    rTerrain.skChunks = make_skeleton_chunks(chunkSubdivLevels);

    // Approximate max number of chunks. Determined experimentally with margin. Surprisingly linear.
    std::uint32_t const maxChunksApprox = 42 * specs.skelMaxSubdivLevels + 30;

    // Approximate max number of shared vertices. Determined experimentally, roughly 60% of all
    // vertices end up being shared. Margin is inherited from maxChunksApprox.
    std::uint32_t const maxVrtxApprox = maxChunksApprox * rTerrain.skChunks.m_chunkSharedCount;
    std::uint32_t const maxSharedVrtxApprox = 0.6f * maxVrtxApprox;

    rTerrain.skChunks.chunk_reserve(std::uint16_t(maxChunksApprox));
    rTerrain.skChunks.shared_reserve(maxSharedVrtxApprox);

    // ## Prepare Chunk geometry and buffer information

    rTerrain.chunkInfo = make_chunk_mesh_buffer_info(rTerrain.skChunks);
    rTerrain.chunkGeom.resize(rTerrain.skChunks, rTerrain.chunkInfo);

    // ## Prepare Chunk scratchpad

    rTerrain.chunkSP.lut = make_chunk_vrtx_subdiv_lut(chunkSubdivLevels);
    rTerrain.chunkSP.resize(rTerrain.skChunks);

    OSP_LOG_INFO("Terrain Chunk Properties:\n"
                 "* MaxChunks: {}\n"
                 "* FillVerticesPerChunk: {}\n"
                 "* SharedVerticesPerChunk: {}\n"
                 "* MaxTrianglesPerChunk: {}\n"
                 "* MaxSharedVertices: {}\n"
                 "* VertexBufferSize: {} bytes\n"
                 "* IndexBufferSize: {} bytes",
                 rTerrain.skChunks.m_chunkIds.capacity(),
                 rTerrain.chunkInfo.fillVrtxCount,
                 rTerrain.skChunks.m_chunkSharedCount,
                 rTerrain.chunkInfo.chunkMaxFaceCount,
                 rTerrain.skChunks.m_sharedIds.capacity(),
                 fmt::group_digits(rTerrain.chunkGeom.vrtxBuffer.size()),
                 fmt::group_digits(rTerrain.chunkGeom.indxBuffer.size() * sizeof(Vector3u)));
}



struct TerrainDebugDraw
{
    KeyedVec<SkVrtxId, osp::draw::DrawEnt> verts;
    MaterialId mat;
    DrawEnt surface;
};

Session setup_terrain_debug_draw(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any>  const topData,
        Session               const &windowApp,
        Session               const &scene,
        Session               const &sceneRenderer,
        Session               const &cameraCtrl,
        Session               const &commonScene,
        Session               const &terrain,
        Session               const &terrainIco,
        MaterialId            const mat)
{
    OSP_DECLARE_GET_DATA_IDS(scene,          TESTAPP_DATA_SCENE);
    OSP_DECLARE_GET_DATA_IDS(commonScene,    TESTAPP_DATA_COMMON_SCENE);
    OSP_DECLARE_GET_DATA_IDS(sceneRenderer,  TESTAPP_DATA_SCENE_RENDERER);
    OSP_DECLARE_GET_DATA_IDS(cameraCtrl,     TESTAPP_DATA_CAMERA_CTRL);
    OSP_DECLARE_GET_DATA_IDS(terrain,        TESTAPP_DATA_TERRAIN);
    OSP_DECLARE_GET_DATA_IDS(terrainIco,     TESTAPP_DATA_TERRAIN_ICO);

    auto const tgScn    = scene         .get_pipelines<PlScene>();
    auto const tgWin    = windowApp     .get_pipelines<PlWindowApp>();
    auto const tgScnRdr = sceneRenderer .get_pipelines<PlSceneRenderer>();
    auto const tgCmCt   = cameraCtrl    .get_pipelines<PlCameraCtrl>();
    auto const tgTrn    = terrain       .get_pipelines<PlTerrain>();

    Session out;
    auto const [idTrnDbgDraw] = out.acquire_data<1>(topData);

    auto &rTrnDbgDraw = top_emplace< TerrainDebugDraw > (topData, idTrnDbgDraw,
                                                         TerrainDebugDraw{.mat = mat});

    auto &rDrawing   = top_get< ACtxDrawing >       (topData, idDrawing);
    auto &rScnRender = top_get< ACtxSceneRender >   (topData, idScnRender);
    auto &rTerrain   = top_get< ACtxTerrain >       (topData, idTerrain);

    rTrnDbgDraw.surface = rScnRender.m_drawIds.create();
    rScnRender.resize_draw();

    rScnRender.m_visible.insert(rTrnDbgDraw.surface);
    rScnRender.m_opaque .insert(rTrnDbgDraw.surface);
    rScnRender.m_materials[mat].m_ents.insert(rTrnDbgDraw.surface);
    rScnRender.m_materials[mat].m_dirty.push_back(rTrnDbgDraw.surface);
    rScnRender.m_mesh[rTrnDbgDraw.surface] = rDrawing.m_meshRefCounts.ref_add(rTerrain.terrainMesh);

    rBuilder.task()
        .name       ("Handle Scene<-->Terrain positioning and floating origin")
        .run_on     ({tgScn.update(Run)})
        .sync_with  ({tgCmCt.camCtrl(Modify), tgTrn.terrainFrame(Modify)})
        .push_to    (out.m_tasks)
        .args       ({                 idCamCtrl,           idDeltaTimeIn,                  idTerrainFrame,             idTerrain,                idTerrainIco })
        .func([] (ACtxCameraController& rCamCtrl, float const deltaTimeIn, ACtxTerrainFrame &rTerrainFrame, ACtxTerrain &rTerrain, ACtxTerrainIco &rTerrainIco) noexcept
    {
        using Magnum::Math::abs;
        using Magnum::Math::cross;
        using Magnum::Math::dot;
        using Magnum::Math::floor;
        using Magnum::Math::sign;
        using Magnum::Math::sqrt;

        if ( ! rCamCtrl.m_target.has_value() )
        {
            return;
        }

        // Camera translation with controls
        SysCameraController::update_move(rCamCtrl, deltaTimeIn, true);

        int const scale = int_2pow<int>(rTerrain.skData.precision);

        Vector3 &rCamPos = rCamCtrl.m_target.value();

        constexpr float maxDist = 65565.0f;

        // Do a floating origin translation if required

        // Determine if x, y, or z in rCamPos goes further than maxDist, and by how much.
        // Round up/down towards zero to the closest multiple of maxDist.
        // Zero if rCamPos is (maxDist) meters away.
        Vector3 const translateOrigin = sign(rCamPos) * floor(abs(rCamPos) / maxDist) * maxDist;
        if ( ! translateOrigin.isZero() )
        {
            // Origin translation involves translating everything in the scene, but in this case
            // it's just the camera. Terrain will respond accordingly to changes in rTerrainFrame.
            rCamPos -= translateOrigin;

            // Scene has moved relative to terrain
            rTerrainFrame.position += Vector3l{translateOrigin} * scale;
        }

        // Set position of camera target relative to terrain, used for LOD distance checking
        rTerrain.scratchpad.viewerPosition = rTerrainFrame.position + Vector3l(rCamPos * scale);

        Vector3d const viewerPosD          = Vector3d{rTerrain.scratchpad.viewerPosition};
        double   const distanceToCenter    = viewerPosD.length();
        double   const minDistanceToCenter = (rTerrainIco.radius + rTerrainIco.height) * scale;

        // Enforce minimum distance to center for viewerPosition. This makes it so that moving the
        // camera below the surface will still show the highest level of detail.
        if (distanceToCenter < minDistanceToCenter)
        {
            rTerrain.scratchpad.viewerPosition *= minDistanceToCenter / distanceToCenter;
        }

        // Set camera controller's 'up' direction to gravity direction

        Vector3 const upOld = rCamCtrl.m_up;
        Vector3 const upNew = Vector3{viewerPosD / distanceToCenter};
        rCamCtrl.m_up = upNew;

        // A bit hacky: Rotate around the target to account for change in 'up' to prevent weird
        //              behaviour with fast (zoomed-out) camera movement.
        // return;   // Hard to explain, uncomment this return to see what I mean :>

        // Rotation required to rotate upOld into upNew
        float const w        = sqrt(upNew.dot() * upNew.dot()) + dot(upOld, upNew);
        auto  const rotation = Quaternion{cross(upOld, upNew), w}.normalized();

        Vector3 const pivot = rCamCtrl.m_target.value();
        rCamCtrl.m_transform.translation() -= pivot;
        rCamCtrl.m_transform = Matrix4{rotation.toMatrix()} * rCamCtrl.m_transform;
        rCamCtrl.m_transform.translation() += pivot;

        SysCameraController::update_view(rCamCtrl, deltaTimeIn);
    });

    rBuilder.task()
        .name       ("Reposition terrain surface mesh")
        .run_on     ({tgScnRdr.render(Run)})
        .sync_with  ({tgTrn.terrainFrame(Ready), tgTrn.chunkMesh(Ready), tgScnRdr.drawEnt(Ready), tgScnRdr.drawTransforms(Modify_), tgScnRdr.drawEntResized(Done)})
        .push_to    (out.m_tasks)
        .args       ({             idTrnDbgDraw,                  idTerrainFrame,             idTerrain,                 idScnRender })
        .func([] (TerrainDebugDraw& rTrnDbgDraw, ACtxTerrainFrame &rTerrainFrame, ACtxTerrain &rTerrain, ACtxSceneRender &rScnRender) noexcept
    {
        int const scale = int_2pow<int>(rTerrain.skData.precision);

        Vector3 const pos = Vector3(rTerrain.chunkGeom.originSkelPos-rTerrainFrame.position) / scale;

        rScnRender.m_drawTransform[rTrnDbgDraw.surface] = Matrix4::translation(pos);
    });

#if 0
    // Setup skeleton vertex visualizer

    rBuilder.task()
        .name       ("Create or delete DrawEnts for each SkVrtxId in the terrain skeleton")
        .run_on     ({tgScnRdr.render(Run)})
        .sync_with  ({tgTrn.skeleton(Ready), tgScnRdr.drawEnt(Delete), tgScnRdr.entMeshDirty(Modify_), tgScnRdr.materialDirty(Modify_), tgScnRdr.drawEntResized(ModifyOrSignal)})
        .push_to    (out.m_tasks)
        .args       ({        idDrawing,                 idScnRender,             idNMesh,                  idTrnDbgDraw,                   idTerrain,                      idTerrainIco })
        .func([] (ACtxDrawing& rDrawing, ACtxSceneRender& rScnRender, NamedMeshes& rNMesh, TerrainDebugDraw& rTrnDbgDraw, ACtxTerrain const &rTerrain, ACtxTerrainIco const &rTerrainIco) noexcept
    {
        Material &rMatPlanet = rScnRender.m_materials[rTrnDbgDraw.mat];
        SubdivIdRegistry<SkVrtxId> const& vrtxIds = rTerrain.skeleton.vrtx_ids();
        rTrnDbgDraw.verts.resize(vrtxIds.capacity());

        // Iterate all existing and non-existing SkVrtxIDs. Create or delete a DrawEnt for it
        // accordingly. This is probably very inefficient :)
        for (std::size_t skVertInt = 0; skVertInt < vrtxIds.capacity(); ++skVertInt)
        {
            auto    const skVert    = SkVrtxId(skVertInt);
            DrawEnt       &rDrawEnt = rTrnDbgDraw.verts[skVert];

            if (vrtxIds.exists(skVert))
            {
                if ( ! rDrawEnt.has_value())
                {
                    rDrawEnt = rScnRender.m_drawIds.create();
                }
            }
            else
            {
                if (rDrawEnt.has_value())
                {
                    if (rScnRender.m_mesh[rDrawEnt].has_value())
                    {
                        rDrawing.m_meshRefCounts.ref_release(std::exchange(rScnRender.m_mesh[rDrawEnt], {}));
                    }
                    rScnRender.m_meshDirty  .push_back  (rDrawEnt);
                    rScnRender.m_visible    .erase      (rDrawEnt);
                    rMatPlanet.m_ents       .erase      (rDrawEnt);
                    rMatPlanet.m_dirty      .push_back  (rDrawEnt);

                    rScnRender.m_drawIds.remove(std::exchange(rDrawEnt, {}));
                }
            }
        }

        // New DrawEnts may have been created. We can't access rScnRender's members with the new
        // entities yet, since they have not yet been resized.
        //
        // rScnRender will be resized afterwards by a task with tgScnRdr.drawEntResized(Run),
        // before moving on to "Manage DrawEnts for each terrain SkVrtxId"
    });

    rBuilder.task()
        .name       ("Arrange SkVrtxId DrawEnts as tiny cubes")
        .run_on     ({tgScnRdr.render(Run)})
        .sync_with  ({tgTrn.skeleton(Ready), tgScnRdr.drawEnt(Ready), tgScnRdr.drawTransforms(Modify_), tgScnRdr.entMeshDirty(Modify_), tgScnRdr.materialDirty(Modify_), tgScnRdr.drawEntResized(Done)})
        .push_to    (out.m_tasks)
        .args       ({        idDrawing,                 idScnRender,             idNMesh,                        idTrnDbgDraw,                   idTerrain,                      idTerrainIco })
        .func([] (ACtxDrawing& rDrawing, ACtxSceneRender& rScnRender, NamedMeshes& rNMesh, TerrainDebugDraw const& rTrnDbgDraw, ACtxTerrain const &rTerrain, ACtxTerrainIco const &rTerrainIco) noexcept
    {
        Material                         &rMatPlanet = rScnRender.m_materials[rTrnDbgDraw.mat];
        MeshId                     const cubeMeshId  = rNMesh.m_shapeToMesh.at(EShape::Box);
        SubdivIdRegistry<SkVrtxId> const &vrtxIds    = rTerrain.skeleton.vrtx_ids();

        for (SkVrtxId const skVert : vrtxIds)
        {
            DrawEnt  const drawEnt = rTrnDbgDraw.verts[skVert];

            if ( ! rScnRender.m_mesh[drawEnt].has_value() )
            {
                rScnRender.m_mesh[drawEnt] = rDrawing.m_meshRefCounts.ref_add(cubeMeshId);
                rScnRender.m_meshDirty.push_back(drawEnt);
                rScnRender.m_visible.insert(drawEnt);
                rScnRender.m_opaque.insert(drawEnt);
                rMatPlanet.m_ents.insert(drawEnt);
                rMatPlanet.m_dirty.push_back(drawEnt);
            }

            rScnRender.m_drawTransform[drawEnt]
                = Matrix4::translation(Vector3(rTerrain.skData.positions[skVert]) / int_2pow<int>(rTerrain.skData.precision))
                * Matrix4::scaling({0.05f, 0.05f, 0.05f});
        }
    });
#endif

    return out;
}


} // namespace testapp::scenes
