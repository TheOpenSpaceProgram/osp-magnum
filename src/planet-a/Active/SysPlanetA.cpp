#include "../Satellites/SatPlanet.h"
#include "SysPlanetA.h"

#include <osp/Active/ActiveScene.h>
#include <osp/Universe.h>

#include <Corrade/Containers/ArrayViewStl.h>
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

StatusActivated SysPlanetA::activate_sat(
        osp::active::ActiveScene &scene,
        osp::active::SysAreaAssociate &area,
        osp::universe::Satellite areaSat,
        osp::universe::Satellite tgtSat)
{

    std::cout << "activatin a planet!!!!!!!!!!!!!!!!11\n";

    osp::universe::Universe &uni = area.get_universe();
    //SysPlanetA& self = scene.get_system<SysPlanetA>();
    universe::UCompPlanet &loadMePlanet = uni.get_reg().get<universe::UCompPlanet>(tgtSat);

    ActiveEnt root = scene.hier_get_root();

    ActiveEnt planetEnt = scene.hier_create_child(root, "Planet");

    // Convert position of the satellite to position in scene
    Vector3 positionInScene = uni.sat_calc_pos_meters(areaSat, tgtSat);

    auto &planetTransform = scene.get_registry()
                            .emplace<osp::active::ACompTransform>(planetEnt);
    planetTransform.m_transform = Matrix4::translation(positionInScene);
    planetTransform.m_enableFloatingOrigin = true;

    CompPlanet &planetComp = scene.reg_emplace<CompPlanet>(planetEnt);
    planetComp.m_radius = loadMePlanet.m_radius;

    return {0, planetEnt, false};
}

SysPlanetA::SysPlanetA(osp::active::ActiveScene &scene) :
    m_scene(scene),
    m_updateGeometry(scene.get_update_order(), "planet_geo", "", "physics",
                     std::bind(&SysPlanetA::update_geometry, this)),
    m_updatePhysics(scene.get_update_order(), "planet_phys", "planet_geo", "",
                    std::bind(&SysPlanetA::update_geometry, this)),
    m_renderPlanetDraw(scene.get_render_order(), "", "", "",
                       std::bind(&SysPlanetA::draw, this, _1))
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
    auto drawGroup = m_scene.get_registry().group<CompPlanet>(
                            entt::get<ACompTransform>);

    Matrix4 entRelative;

    for(auto entity: drawGroup)
    {
        CompPlanet& planet = drawGroup.get<CompPlanet>(entity);
        ACompTransform& transform = drawGroup.get<ACompTransform>(entity);

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
                                             CompPlanet &planet,
                                             chindex_t chunk)
{

    using osp::ECollisionShape;

    SysPhysics &physics = m_scene.get_system<SysPhysics>();

    // Create entity and required components
    ActiveEnt fish = m_scene.hier_create_child(m_scene.hier_get_root());
    ACompTransform &fishTransform
            = m_scene.reg_emplace<ACompTransform>(fish);
    ACompCollisionShape &fishShape
            = m_scene.reg_emplace<ACompCollisionShape>(fish);
    ACompRigidBody &fishBody
            = m_scene.reg_emplace<ACompRigidBody>(fish);

    // Set some stuff
    fishShape.m_shape = ECollisionShape::TERRAIN;
    fishTransform.m_transform = m_scene.reg_get<ACompTransform>(ent).m_transform;
    fishTransform.m_enableFloatingOrigin = true;

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

    auto view = m_scene.get_registry().view<CompPlanet>();

    for (osp::active::ActiveEnt ent : view)
    {
        CompPlanet &planet = view.get<CompPlanet>(ent);

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
    }
}

void SysPlanetA::update_physics()
{

}

