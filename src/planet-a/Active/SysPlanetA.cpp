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

    osp::universe::Universe &uni = area.get_universe();
    //SysPlanetA& self = scene.get_system<SysPlanetA>();
    auto &loadMePlanet = uni.get_reg().get<universe::UCompPlanet>(tgtSat);

    // Convert position of the satellite to position in scene
    Vector3 positionInScene = uni.sat_calc_pos_meters(areaSat, tgtSat);

    // Create planet entity and add components to it

    ActiveEnt root = scene.hier_get_root();
    ActiveEnt planetEnt = scene.hier_create_child(root, "Planet");

    auto &planetTransform = scene.get_registry()
                            .emplace<osp::active::ACompTransform>(planetEnt);
    planetTransform.m_transform = Matrix4::translation(positionInScene);
    scene.reg_emplace<ACompFloatingOrigin>(planetEnt);

    auto &planetPlanet = scene.reg_emplace<ACompPlanet>(planetEnt);
    planetPlanet.m_radius = loadMePlanet.m_radius;

    auto &planetForceField = scene.reg_emplace<ACompFFGravity>(planetEnt);

    // arbitrarily picked nice working number
    planetForceField.m_Gmass = 100000.0f;

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
    return 0;
}

void SysPlanetA::draw(osp::active::ACompCamera const& camera)
{
    auto drawGroup = m_scene.get_registry().group<ACompPlanet>(
                            entt::get<ACompTransform>);

    Matrix4 entRelative;

    for(auto entity: drawGroup)
    {
        auto& planet = drawGroup.get<ACompPlanet>(entity);
        auto& transform = drawGroup.get<ACompTransform>(entity);

        if (!planet.m_planet.is_initialized())
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
    auto itsChunk = planet.m_planet.iterate_chunk(chunk);

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

        if (!planet.m_planet.is_initialized())
        {
            // initialize planet if not done so yet
            std::cout << "Initializing planet\n";
            planet.m_planet.initialize(planet.m_radius);
            std::cout << "Planet initialized, now making colliders\n";

            // temporary: make colliders for all the chunks
            for (chindex_t i = 0; i < planet.m_planet.chunk_count(); i ++)
            {
                debug_create_chunk_collider(ent, planet, i);
                std::cout << "* completed chunk collider: " << i << "\n";
            }

            std::cout << "Planet colliders done\n";

            planet.m_vrtxBufGL.setData(planet.m_planet.get_vertex_buffer());
            planet.m_indxBufGL.setData(planet.m_planet.get_index_buffer());

            planet.m_planet.get_index_buffer_updates().clear();

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
                .setCount(planet.m_planet.calc_index_count());


        }

        if (m_debugUpdate.triggered())
        {
            ActiveEnt cam = m_scene.get_registry().view<ACompCamera>().front();
            auto &camTf = m_scene.reg_get<ACompTransform>(cam);

            // camera position relative to planet
            Vector3 camRelative = camTf.m_transform.translation()
                                - tf.m_transform.translation();

            using Magnum::Math::pow;
            using Magnum::Math::sqrt;

            // edge length of largest possible triangle
            float edgeLengthA =  planet.m_planet.get_ico_tree()->get_radius()
                                / sqrt(10.0f + 2.0f * sqrt(5.0f)) * 4.0;

            float threshold = 1.0f;

            planet.m_planet.chunk_geometry_update_all(
                    [edgeLengthA, threshold, tf, camRelative, ent] (
                            SubTriangle const& tri,
                            SubTriangleChunk const& chunk,
                            int index) -> EChunkUpdateAction
            {
                // approximation of triangle edge length
                float edgeLength = edgeLengthA / float(pow(2, int(tri.m_depth)));

                // distance between viewer and triangle
                float dist = (tri.m_center - camRelative).length();

                float screenLength = edgeLength / dist;

                bool tooFar = screenLength < threshold;

                if (tooFar)
                {
                    return EChunkUpdateAction::Chunk;
                }
                else
                {
                    return EChunkUpdateAction::Subdivide;
                }

                /*if (tri.m_depth == 2)
                {
                    if (chunk.m_chunk == gc_invalidChunk)
                    {
                        return EChunkUpdateAction::Chunk;
                    }
                    else
                    {
                        return EChunkUpdateAction::Unchunk;
                    }
                }
                else
                {
                    return EChunkUpdateAction::Nothing;
                }*/
            });

            using Corrade::Containers::ArrayView;

            std::vector<unsigned> const &indxData = planet.m_planet.get_index_buffer();

            // update GPU buffers
            for (UpdateRangeSub updRange : planet.m_planet.get_index_buffer_updates())
            {
                buindex_t size = updRange.m_end - updRange.m_start;
                auto arrView = ArrayView(indxData.data() + updRange.m_start, size);
                //planet.m_indxBufGL.setSubData(updRange.m_start, arrView);
            }

            planet.m_vrtxBufGL.setData(planet.m_planet.get_vertex_buffer());
            planet.m_indxBufGL.setData(planet.m_planet.get_index_buffer());

            planet.m_mesh.setCount(planet.m_planet.calc_index_count());

            planet.m_planet.get_index_buffer_updates().clear();
        }
    }
}

void SysPlanetA::update_physics()
{

}

