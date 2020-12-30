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

#include <osp/Active/ActiveScene.h>
#include <osp/Universe.h>

#include <Corrade/Containers/ArrayViewStl.h>
#include <Magnum/Math/Color.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/GL/DefaultFramebuffer.h>

#include <iostream>

using namespace osp::active;
using namespace planeta::active;

using Magnum::GL::Renderer;
using osp::Matrix4;
using osp::Vector2;
using osp::Vector3;

// for _1, _2, _3, ... std::bind arguments
using namespace std::placeholders;

// for the 0xrrggbb_rgbf and _deg literals
using namespace Magnum::Math::Literals;

using osp::universe::Satellite;

const std::string SysPlanetA::smc_name = "PlanetA";

StatusActivated SysPlanetA::activate_sat(
        osp::active::ActiveScene &scene,
        osp::active::SysAreaAssociate &area,
        osp::universe::Satellite areaSat,
        osp::universe::Satellite tgtSat)
{

    std::cout << "activatin a planet!!!!!!!!!!!!!!!!11\n";

    osp::universe::Universe const &uni = area.get_universe();
    //SysPlanetA& self = scene.get_system<SysPlanetA>();
    auto &loadMePlanet = uni.get_reg().get<universe::UCompPlanet>(tgtSat);

    // Convert position of the satellite to position in scene
    Vector3 positionInScene = uni.sat_calc_pos_meters(areaSat, tgtSat);

    // Create planet entity and add components to it

    ActiveEnt root = scene.hier_get_root();
    ActiveEnt planetEnt = scene.hier_create_child(root, "Planet");

    auto &rPlanetTransform = scene.get_registry()
                             .emplace<osp::active::ACompTransform>(planetEnt);
    rPlanetTransform.m_transform = Matrix4::translation(positionInScene);
    scene.reg_emplace<ACompFloatingOrigin>(planetEnt);

    auto &rPlanetPlanet = scene.reg_emplace<ACompPlanet>(planetEnt);
    rPlanetPlanet.m_radius = loadMePlanet.m_radius;

    auto &rPlanetForceField = scene.reg_emplace<ACompFFGravity>(planetEnt);

    // gravitational constant
    static const float sc_GravConst = 6.67408E-11f;

    rPlanetForceField.m_Gmass = loadMePlanet.m_mass * sc_GravConst;

    return {0, planetEnt, false};
}

SysPlanetA::SysPlanetA(osp::active::ActiveScene &scene,
                       osp::UserInputHandler &userInput) :
    m_scene(scene),
    m_updateGeometry(scene.get_update_order(), "planet_geo", "", "physics",
                    [this] { this->update_geometry(); } ),
    m_updatePhysics(scene.get_update_order(), "planet_phys", "planet_geo", "",
                    [this] { this->update_physics(); }),
    m_renderPlanetDraw(scene.get_render_order(), "", "", "",
                       std::bind(&SysPlanetA::draw, this, _1)),
    m_debugUpdate(userInput.config_get("debug_planet_update"))
{

}


int SysPlanetA::deactivate_sat(osp::active::ActiveScene &scene,
                               osp::active::SysAreaAssociate &area,
                               osp::universe::Satellite areaSat,
                               osp::universe::Satellite tgtSat,
                               osp::active::ActiveEnt tgtEnt)
{
    // TODO
    return 0;
}

void SysPlanetA::draw(osp::active::ACompCamera const& camera)
{
    // TODO: move planet drawing to something more generic, like debug drawable
    //       SysPlanet should NOT be doing rendering

    auto drawGroup = m_scene.get_registry().group<ACompPlanet>(
                            entt::get<ACompTransform>);

    Matrix4 entRelative;

    for(auto entity: drawGroup)
    {
        auto& planet = drawGroup.get<ACompPlanet>(entity);
        auto& transform = drawGroup.get<ACompTransform>(entity);

        if (planet.m_planet.get() == nullptr)
        {
            continue;
        }

        entRelative = camera.m_inverse * transform.m_transformWorld;

        planet.m_shader
                //.setDiffuseColor(Magnum::Color4{0.2f, 1.0f, 0.2f, 1.0f})
                //.setLightPosition({10.0f, 15.0f, 5.0f})
                .setColor(0x2f83cc_rgbf)
                .setWireframeColor(0xdcdcdc_rgbf)
                .setViewportSize(Vector2{Magnum::GL::defaultFramebuffer.viewport().size()})
                .setTransformationMatrix(entRelative)
                .setNormalMatrix(entRelative.normalMatrix())
                .setProjectionMatrix(camera.m_projection)
                .draw(planet.m_mesh);
    }
}

void SysPlanetA::debug_create_chunk_collider(osp::active::ActiveEnt ent,
                                             ACompPlanet &planet,
                                             chindex_t chunk)
{

    using osp::ECollisionShape;

    auto &physics = m_scene.dynamic_system_find<SysPhysics>();

    // Create entity and required components
    ActiveEnt fish = m_scene.hier_create_child(m_scene.hier_get_root());
    auto &fishTransform = m_scene.reg_emplace<ACompTransform>(fish);
    auto &fishShape = m_scene.reg_emplace<ACompCollisionShape>(fish);
    auto &fishBody = m_scene.reg_emplace<ACompRigidBody>(fish);
    m_scene.reg_emplace<ACompFloatingOrigin>(fish);

    // Set some stuff
    fishShape.m_shape = ECollisionShape::TERRAIN;
    fishTransform.m_transform = m_scene.reg_get<ACompTransform>(ent).m_transform;

    // Get triangle iterators to start and end triangles of the specified chunk
    auto itsChunk = planet.m_planet->iterate_chunk(chunk);

    // Send them to the physics engine
    physics.shape_create_tri_mesh_static(fishShape,
                                         itsChunk.first, itsChunk.second);

    // create the rigid body
    physics.create_body(fish);
}

void SysPlanetA::update_geometry()
{

    auto view = m_scene.get_registry().view<ACompPlanet, ACompTransform>();

    for (osp::active::ActiveEnt ent : view)
    {
        auto &planet = view.get<ACompPlanet>(ent);
        auto &tf = view.get<ACompTransform>(ent);

        if (planet.m_planet.get() == nullptr)
        {
            planet.m_planet = std::make_shared<PlanetGeometryA>();
            PlanetGeometryA &rPlanetGeo = *(planet.m_planet);

            // initialize planet if not done so yet
            std::cout << "Initializing planet\n";
            planet.m_icoTree = std::make_shared<IcoSphereTree>();

            planet.m_icoTree->initialize(planet.m_radius);

            rPlanetGeo.initialize(planet.m_icoTree, 4, 1u << 9, 1u << 13);

            planet.m_icoTree->event_add(planet.m_planet);

            // Chunk all the initial triangles
            rPlanetGeo.chunk_geometry_update_all(
                    [] (SubTriangle const& tri, SubTriangleChunk const& chunk,
                        int index) -> EChunkUpdateAction
            {
                return EChunkUpdateAction::Chunk;
            });

            //planet_update_geometry(ent, planet);

            std::cout << "Planet initialized, now making colliders\n";

            // temporary: make colliders for all the chunks
            for (chindex_t i = 0; i < rPlanetGeo.chunk_count(); i ++)
            {
                debug_create_chunk_collider(ent, planet, i);
                std::cout << "* completed chunk collider: " << i << "\n";
            }

            std::cout << "Planet colliders done\n";

            planet.m_vrtxBufGL.setData(rPlanetGeo.get_vertex_buffer());
            planet.m_indxBufGL.setData(rPlanetGeo.get_index_buffer());

            rPlanetGeo.updates_clear();

            using Magnum::Shaders::MeshVisualizer3D;
            using Magnum::GL::MeshPrimitive;
            using Magnum::GL::MeshIndexType;

            planet.m_mesh
                .setPrimitive(MeshPrimitive::Triangles)
                .addVertexBuffer(planet.m_vrtxBufGL, 0,
                                 MeshVisualizer3D::Position{},
                                 MeshVisualizer3D::Normal{})
                .setIndexBuffer(planet.m_indxBufGL, 0,
                                MeshIndexType::UnsignedInt)
                .setCount(rPlanetGeo.calc_index_count());
        }

        if (m_debugUpdate.triggered() || true)
        {
            planet_update_geometry(ent);
        }
    }
}

void SysPlanetA::planet_update_geometry(osp::active::ActiveEnt planetEnt)
{
    auto &rPlanetPlanet = m_scene.reg_get<ACompPlanet>(planetEnt);
    auto const &planetTf = m_scene.reg_get<ACompTransform>(planetEnt);
    auto const &planetActivated = m_scene.reg_get<ACompActivatedSat>(planetEnt);

    // TODO: de-spaghettify systems. make systems entirely stateless
    osp::universe::Universe const &uni = m_scene.dynamic_system_find<SysAreaAssociate>().get_universe();

    Satellite planetSat = planetActivated.m_sat;
    auto &planetUComp = uni.get_reg().get<universe::UCompPlanet>(planetSat);

    PlanetGeometryA &rPlanetGeo = *(rPlanetPlanet.m_planet);

    // Temporary: use the first camera in the scene as the viewer
    ActiveEnt cam = m_scene.get_registry().view<ACompCamera>().front();
    auto const &camTf = m_scene.reg_get<ACompTransform>(cam);

    // Set this somewhere else. Radius at which detail falloff will start
    // This will essentially be the physics area size
    float viewerRadius = 64.0f;

    // camera position relative to planet
    Vector3 camRelative = camTf.m_transform.translation()
                        - planetTf.m_transform.translation();

    using Magnum::Math::max;
    using Magnum::Math::pow;
    using Magnum::Math::sqrt;

    // Ratio between an icosahedron's edge length and radius
    static const float sc_icoEdgeRatio = sqrt(10.0f + 2.0f * sqrt(5.0f)) / 4.0f;

    // edge length of largest possible triangle
    float edgeLengthA = planetUComp.m_radius / sc_icoEdgeRatio
                        / rPlanetGeo.get_chunk_vertex_width();

    rPlanetGeo.chunk_geometry_update_all(
            [edgeLengthA, planetUComp, camRelative, viewerRadius] (
                    SubTriangle const& tri,
                    SubTriangleChunk const& chunk,
                    trindex_t index) -> EChunkUpdateAction
    {
        // distance between viewer and triangle
        float dist = (tri.m_center - camRelative).length();
        dist = max(0.0001f, dist - viewerRadius);

        // approximation of current triangle edge length
        float edgeLength = edgeLengthA / float(pow(2, int(tri.m_depth)));
        float screenLength = edgeLength / dist;

        bool tooClose = screenLength > planetUComp.m_resolutionScreenMax;
        bool canDivideFurther = edgeLength > planetUComp.m_resolutionSurfaceMax;

        if (tooClose && canDivideFurther)
        {
            // too close, subdivide it
            return EChunkUpdateAction::Subdivide;
        }
        else
        {
            return EChunkUpdateAction::Chunk;
        }
    });

    // New triangles were added, notify
    rPlanetPlanet.m_icoTree->event_notify();

    // Remove unused
    rPlanetPlanet.m_icoTree->subdivide_remove_all_unused();
    rPlanetPlanet.m_icoTree->event_notify();

    //planet.m_planet->debug_raise_by_share_count();

    using Corrade::Containers::ArrayView;

    // update GPU index buffer

    std::vector<unsigned> const &indxData = rPlanetGeo.get_index_buffer();

    for (UpdateRangeSub updRange : rPlanetGeo.updates_get_index_changes())
    {
        buindex_t size = updRange.m_end - updRange.m_start;
        ArrayView arrView(indxData.data() + updRange.m_start, size);
        rPlanetPlanet.m_indxBufGL.setSubData(
                updRange.m_start * sizeof(*indxData.data()), arrView);
    }

    // update GPU vertex buffer

    std::vector<float> const &vrtxData = rPlanetGeo.get_vertex_buffer();

    for (UpdateRangeSub updRange : rPlanetGeo.updates_get_vertex_changes())
    {
        buindex_t size = updRange.m_end - updRange.m_start;
        ArrayView arrView(vrtxData.data() + updRange.m_start, size);
        rPlanetPlanet.m_vrtxBufGL.setSubData(
                updRange.m_start * sizeof(*vrtxData.data()), arrView);
    }

    //planet.m_vrtxBufGL.setData(planet.m_planet->get_vertex_buffer());
    //planet.m_indxBufGL.setData(planet.m_planet->get_index_buffer());
    rPlanetGeo.updates_clear();

    rPlanetPlanet.m_mesh.setCount(rPlanetGeo.calc_index_count());

    rPlanetGeo.get_ico_tree()->debug_verify_state();
}

void SysPlanetA::update_physics()
{

}

