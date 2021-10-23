/**
 * Open Space Program
 * Copyright © 2019-2020 Open Space Program Project
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
#include "../Satellites/SatPlanet.h"
#include "SysPlanetA.h"

#include "../icosahedron.h"

#include <osp/Active/ActiveScene.h>
#include <osp/Active/SysHierarchy.h>
#include <osp/Active/SysPhysics.h>
#include <osp/Active/SysRender.h>
#include <osp/Universe.h>
#include <osp/string_concat.h>
#include <osp/logging.h>

#include <Corrade/Containers/ArrayViewStl.h>
#include <Magnum/Math/Color.h>
#include <Magnum/GL/Renderer.h>

#include <iostream>
#include <fstream>

using planeta::active::SysPlanetA;
using planeta::universe::UCompPlanet;

using osp::active::ActiveScene;
using osp::active::ActiveEnt;

using osp::active::ACompPhysBody;

using osp::active::SysHierarchy;
using osp::active::SysAreaAssociate;
using osp::active::SysPhysics;
using osp::active::SysNewton;

using osp::universe::Universe;
using osp::universe::Satellite;

using osp::Matrix4;
using osp::Vector2;
using osp::Vector3;

using osp::Vector3d;

using osp::Vector3l;

using osp::universe::Satellite;

struct PlanetVertex
{
    osp::Vector3 m_position;
    osp::Vector3 m_normal;
};

using Vector3ui = Magnum::Math::Vector3<Magnum::UnsignedInt>;

ActiveEnt SysPlanetA::activate(
            ActiveScene &rScene, Universe &rUni,
            Satellite areaSat, Satellite tgtSat)
{
    using namespace osp::active;

    OSP_LOG_INFO("Activating a planet");

    //SysPlanetA& self = scene.get_system<SysPlanetA>();
    auto &loadMePlanet = rUni.get_reg().get<universe::UCompPlanet>(tgtSat);

    // Convert position of the satellite to position in scene
    Vector3 positionInScene = rUni.sat_calc_pos_meters(areaSat, tgtSat).value();

    // Create planet entity and add components to it

    ActiveEnt root = rScene.hier_get_root();
    ActiveEnt planetEnt = SysHierarchy::create_child(rScene, root, "Planet");

    auto &rPlanetTransform = rScene.get_registry()
                             .emplace<ACompTransform>(planetEnt);
    rPlanetTransform.m_transform = Matrix4::translation(positionInScene);
    rScene.reg_emplace<ACompFloatingOrigin>(planetEnt);
    rScene.reg_emplace<ACompActivatedSat>(planetEnt, tgtSat);

    auto &rPlanetPlanet = rScene.reg_emplace<ACompPlanet>(planetEnt);
    rPlanetPlanet.m_radius = loadMePlanet.m_radius;

    auto &rPlanetForceField = rScene.reg_emplace<ACompFFGravity>(planetEnt);

    // gravitational constant
    static const float sc_GravConst = 6.67408E-11f;

    rPlanetForceField.m_Gmass = loadMePlanet.m_mass * sc_GravConst;



    std::array<planeta::SkVrtxId, 12> icoVrtx;
    std::array<planeta::SkTriId, 20> icoTri;
    std::vector<Vector3l> positions;
    std::vector<Vector3> normals;
    int const scale = 10;
    SubdivTriangleSkeleton skeleton = create_skeleton_icosahedron(loadMePlanet.m_radius, scale, icoVrtx, icoTri, positions, normals);

    SkeletonTriangle const &tri = skeleton.tri_at(icoTri[0]);

    std::array<SkVrtxId, 3> const middles = skeleton.vrtx_create_middles(tri.m_vertices);

    SkTriGroupId triChildren = skeleton.tri_subdiv(icoTri[0], middles);

    constexpr int const c_level = 6;
    constexpr int const c_edgeCount = (1u << c_level) - 1;

    for (planeta::SkTriId tri : icoTri)
    {
        auto fish = skeleton.tri_at(tri);

        std::array<SkVrtxId, c_edgeCount> chunkEdgeA;
        std::array<SkVrtxId, c_edgeCount> chunkEdgeB;
        std::array<SkVrtxId, c_edgeCount> chunkEdgeC;
        skeleton.vrtx_create_chunk_edge_recurse(c_level, fish.m_vertices[0], fish.m_vertices[1], chunkEdgeA);
        skeleton.vrtx_create_chunk_edge_recurse(c_level, fish.m_vertices[1], fish.m_vertices[2], chunkEdgeB);
        skeleton.vrtx_create_chunk_edge_recurse(c_level, fish.m_vertices[2], fish.m_vertices[0], chunkEdgeC);

        positions.resize(skeleton.vrtx_ids().size_required());
        normals.resize(skeleton.vrtx_ids().size_required());

        ico_calc_chunk_edge_recurse(loadMePlanet.m_radius, scale, c_level, fish.m_vertices[0], fish.m_vertices[1], chunkEdgeA, positions, normals);
        ico_calc_chunk_edge_recurse(loadMePlanet.m_radius, scale, c_level, fish.m_vertices[1], fish.m_vertices[2], chunkEdgeB, positions, normals);
        ico_calc_chunk_edge_recurse(loadMePlanet.m_radius, scale, c_level, fish.m_vertices[2], fish.m_vertices[0], chunkEdgeC, positions, normals);

    }

    std::array<SkVrtxId, c_edgeCount> chunkEdgeA;
    std::array<SkVrtxId, c_edgeCount> chunkEdgeB;
    std::array<SkVrtxId, c_edgeCount> chunkEdgeC;
    skeleton.vrtx_create_chunk_edge_recurse(c_level, middles[1], middles[2], chunkEdgeA);
    skeleton.vrtx_create_chunk_edge_recurse(c_level, middles[2], middles[0], chunkEdgeB);
    skeleton.vrtx_create_chunk_edge_recurse(c_level, middles[0], middles[1], chunkEdgeC);

    positions.resize(skeleton.vrtx_ids().size_required());
    normals.resize(skeleton.vrtx_ids().size_required());

    ico_calc_chunk_edge_recurse(loadMePlanet.m_radius, scale, c_level, middles[1], middles[2], chunkEdgeA, positions, normals);
    ico_calc_chunk_edge_recurse(loadMePlanet.m_radius, scale, c_level, middles[2], middles[0], chunkEdgeB, positions, normals);
    ico_calc_chunk_edge_recurse(loadMePlanet.m_radius, scale, c_level, middles[0], middles[1], chunkEdgeC, positions, normals);

    ico_calc_middles(loadMePlanet.m_radius, scale, tri.m_vertices, middles, positions, normals);

    // output can be pasted into an obj file for viewing
    float scalepow = std::pow(2.0f, -scale);
//    for (Vector3l v : positions)
//    {
//        std::cout << "v " << (v.x() * scalepow) << " "
//                          << (v.y() * scalepow) << " "
//                          << (v.z() * scalepow) << "\n";
//    }

    ChunkedTriangleMeshInfo a = make_subdivtrimesh_general(10, c_level, scale);

    std::vector<PlanetVertex> vrtxBuf(a.vertex_count_max());
    auto const get_vrtx = [&vrtxBuf] (VertexId vrtxId) -> PlanetVertex& { return vrtxBuf[size_t(vrtxId)]; };

    std::vector<Vector3ui> indxBuf(a.index_count_max());

    ChunkVrtxSubdivLUT chunkVrtxLut(c_level);

    ChunkId chunk = a.chunk_create(skeleton, tri_id(triChildren, 3), chunkEdgeA, chunkEdgeB, chunkEdgeC);

    using Corrade::Containers::arrayCast;

    ArrayView_t<PlanetVertex> const vrtxBufShared{
        &vrtxBuf[a.vertex_offset_shared()],
        a.shared_count_max()
    };

    ArrayView_t<PlanetVertex> const vrtxBufChunkFill(
        &vrtxBuf[a.vertex_offset_fill(chunk)],
        a.chunk_vrtx_fill_count()
    );

    // Set positions of shared vertices

    a.shared_update( [&positions, &scalepow, &vrtxBufShared] (
            ArrayView_t<SharedVrtxId const> newlyAdded,
            ArrayView_t<SkVrtxId const> sharedToSkel)
    {

        for (SharedVrtxId const sharedId : newlyAdded)
        {
            SkVrtxId const skelId = sharedToSkel[size_t(sharedId)];

            //Vector3l const translated = positions[size_t(skelId)] + translaton;
            Vector3d const scaled
                    = Vector3d(positions[size_t(skelId)]) * scalepow;

            vrtxBufShared[size_t(sharedId)].m_position = Vector3(scaled);
        }
    });

    // Set Positions of fill vertices using LUT

    ArrayView_t<SharedVrtxId const> chunkShared = a.chunk_shared(chunk);

    for (ChunkVrtxSubdivLUT::ToSubdiv const& toSubdiv : chunkVrtxLut.data())
    {
        PlanetVertex const &vrtxA = chunkVrtxLut.get(
                    toSubdiv.m_vrtxA, chunkShared,
                    vrtxBufChunkFill, vrtxBufShared);

        PlanetVertex const &vrtxB = chunkVrtxLut.get(
                    toSubdiv.m_vrtxB, chunkShared,
                    vrtxBufChunkFill, vrtxBufShared);



        PlanetVertex &rVrtxC = vrtxBufChunkFill[size_t(toSubdiv.m_fillOut)];
        // Heightmap goes here

        osp::Vector3 const avg = (vrtxA.m_position + vrtxB.m_position) / 2.0f;
        float const avgLen = avg.length();
        float const roundness = loadMePlanet.m_radius - avgLen;

        rVrtxC.m_position =  avg + (avg / avgLen) * roundness;
    }

    // calculate faces and normals

    // future optimization: LUT some of these too

    using Vector2us = ChunkVrtxSubdivLUT::Vector2us;

    uint32_t indexOffset = a.index_chunk_offset(chunk);

    int trisAdded = 0;
    for (unsigned int y = 0; y < a.chunk_width(); y ++)
    {
        for (unsigned int x = 0; x < y * 2 + 1; x ++)
        {
            // alternate between up-pointing and down-pointing triangles
            bool const upPointing = (x % 2 == 1);
            auto const coords = upPointing
                    ? std::array<Vector2us, 3>({
                        Vector2us(x / 2 + 1, y + 1),
                        Vector2us(x / 2 + 1, y),
                        Vector2us(x / 2, y) })
                    : std::array<Vector2us, 3>({
                        Vector2us(x / 2, y),
                        Vector2us(x / 2, y + 1),
                        Vector2us(x / 2 + 1, y + 1) });

            bool const onEdge = (x == 0) || (x == y * 2)
                                || (!upPointing && y == a.chunk_width() - 1);

            std::array<VertexId, 3> const triVrtxIds{
                a.chunk_coord_to_vrtx(chunk, coords[0].x(), coords[0].y()),
                a.chunk_coord_to_vrtx(chunk, coords[1].x(), coords[1].y()),
                a.chunk_coord_to_vrtx(chunk, coords[2].x(), coords[2].y())};

            std::array<PlanetVertex*, 3> const triVrtx
            {
                &get_vrtx(triVrtxIds[0]),
                &get_vrtx(triVrtxIds[1]),
                &get_vrtx(triVrtxIds[2])
            };

            // Calculate face normal
            Vector3 const u = triVrtx[1]->m_position - triVrtx[0]->m_position;
            Vector3 const v = triVrtx[2]->m_position - triVrtx[0]->m_position;
            Vector3 const faceNorm = Magnum::Math::cross(u, v).normalized();

            for (int i = 0; i < 3; i ++)
            {
                Vector3 &rVrtxNorm = triVrtx[i]->m_normal;

                if (a.vertex_is_shared(triVrtxIds[i]))
                {
                    if ( ! onEdge)
                    {
                        // Shared vertices can have a variable number of
                        // connected faces

                        SharedVrtxId const shared = a.vertex_to_shared(triVrtxIds[i]);
                        uint8_t &rFaceCount = a.shared_face_count(shared);

                        // Add new face normal to the average
                        rVrtxNorm = (rVrtxNorm * rFaceCount + faceNorm) / (rFaceCount + 1);
                        rFaceCount ++;

                    }
                    // edge triangles handled by fans, and are left empty
                }
                else
                {
                    // All fill vertices have 6 connected faces
                    // (just look at a picture of a triangular tiling)

                    // Fans with multiple triangles may be connected to a fill
                    // vertex, but the normals are calculated as if there was
                    // only one triangle to (potentially) improve blending

                    rVrtxNorm += faceNorm / 6.0f;
                }
            }

            if ( ! onEdge)
            {
                // Add to the index buffer

                indxBuf[indexOffset + trisAdded]
                        = {uint32_t(triVrtxIds[0]), uint32_t(triVrtxIds[1]), uint32_t(triVrtxIds[2])};
                trisAdded ++;
            }


        }
    }


    // debugging: export obj file

    std::ofstream objfile;
    objfile.open("planetdebug.obj");

    objfile << "o Planet\n";

    for (PlanetVertex v : vrtxBuf)
    {
        objfile << "v " << (v.m_position.x()) << " "
                        << (v.m_position.y()) << " "
                        << (v.m_position.z()) << "\n";
    }

    for (PlanetVertex v : vrtxBuf)
    {
        objfile << "vn " << (v.m_normal.x()) << " "
                         << (v.m_normal.y()) << " "
                         << (v.m_normal.z()) << "\n";
    }

    for (Vector3ui i : indxBuf)
    {
        Vector3ui iB = i + Vector3ui(1, 1, 1); // obj indices start at 1
        objfile << "f " << iB.x() << "//" << iB.x() << " "
                        << iB.y() << "//" << iB.y() << " "
                        << iB.z() << "//" << iB.z() << "\n";
    }

    objfile.close();

    return planetEnt;
}

void SysPlanetA::update_activate(ActiveScene &rScene)
{
    osp::active::ACompAreaLink *pLink
            = SysAreaAssociate::try_get_area_link(rScene);

    if (pLink == nullptr)
    {
        return;
    }

    Universe &rUni = pLink->get_universe();
    auto &rSync = rScene.get_registry().ctx<SyncPlanets>();

    // Delete planets that have exited the ActiveArea
    for (Satellite sat : pLink->m_leave)
    {
        if (!rUni.get_reg().all_of<UCompPlanet>(sat))
        {
            continue;
        }

        auto foundEnt = rSync.m_inArea.find(sat);
        SysHierarchy::mark_delete_cut(rScene, foundEnt->second);
        rSync.m_inArea.erase(foundEnt);
    }

    // Activate planets that have just entered the ActiveArea
    for (Satellite sat : pLink->m_enter)
    {
        if (!rUni.get_reg().all_of<UCompPlanet>(sat))
        {
            continue;
        }

        ActiveEnt ent = activate(rScene, rUni, pLink->m_areaSat, sat);
        rSync.m_inArea[sat] = ent;
    }
}

void SysPlanetA::update_geometry(ActiveScene& rScene)
{
    using namespace osp::active;

    auto view = rScene.get_registry().view<ACompPlanet, ACompTransform>();

    for (osp::active::ActiveEnt ent : view)
    {
        auto &planet = view.get<ACompPlanet>(ent);
        auto &tf = view.get<ACompTransform>(ent);


        planet_update_geometry(ent, rScene);
    }
}

void SysPlanetA::planet_update_geometry(osp::active::ActiveEnt planetEnt,
                                        osp::active::ActiveScene& rScene)
{
    using namespace osp::active;

    auto &rPlanetPlanet = rScene.reg_get<ACompPlanet>(planetEnt);
    auto const &planetTf = rScene.reg_get<ACompTransform>(planetEnt);
    auto const &planetActivated = rScene.reg_get<ACompActivatedSat>(planetEnt);

    osp::universe::Universe const &uni = rScene.reg_get<ACompAreaLink>(rScene.hier_get_root()).m_rUniverse;

    Satellite planetSat = planetActivated.m_sat;
    auto &planetUComp = uni.get_reg().get<universe::UCompPlanet>(planetSat);


}

void SysPlanetA::update_physics(ActiveScene& rScene)
{

}

