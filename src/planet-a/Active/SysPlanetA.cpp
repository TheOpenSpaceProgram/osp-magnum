#include "../Satellites/SatPlanet.h"
#include "SysPlanetA.h"

#include <osp/Active/ActiveScene.h>

#include <Corrade/Containers/ArrayViewStl.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/GL/DefaultFramebuffer.h>

#include <iostream>


// for _1, _2, _3, ... std::bind arguments
using namespace std::placeholders;

// for the 0xrrggbb_rgbf and _deg literals
using namespace Magnum::Math::Literals;

using namespace osp;

int SysPlanetA::area_activate_planet(SatActiveArea& area,
                                     SatelliteObject& loadMe)
{
    std::cout << "activatin a planet!!!!!!!!!!!!!!!!11\n";


    // Get the needed variables
    SatPlanet &planet = static_cast<SatPlanet&>(loadMe);
    ActiveScene &scene = *(area.get_scene());
    ActiveEnt root = scene.hier_get_root();

    ActiveEnt planetEnt = scene.hier_create_child(root, "Planet");

    // Convert position of the satellite to position in scene
    Vector3 positionInScene = planet.get_satellite()
            ->position_relative_to(*(area.get_satellite())).to_meters();

    CompTransform& planetTransform = scene.get_registry()
                                        .emplace<CompTransform>(planetEnt);
    planetTransform.m_transform = Matrix4::translation(positionInScene);
    planetTransform.m_enableFloatingOrigin = true;

    CompPlanet& planetComp = scene.reg_emplace<CompPlanet>(planetEnt);

    planetComp.m_radius = planet.m_radius;

    return 0;
}

SysPlanetA::SysPlanetA(ActiveScene &scene) :
    m_scene(scene),
    m_updateGeometry(scene.get_update_order(), "planet_geo", "physics", "",
                     std::bind(&SysPlanetA::update_geometry, this)),
    m_updatePhysics(scene.get_update_order(), "planet_phys", "planet_geo", "",
                    std::bind(&SysPlanetA::update_geometry, this)),
    m_renderPlanetDraw(scene.get_render_order(), "", "", "",
                       std::bind(&SysPlanetA::draw, this, _1))
{

}


void SysPlanetA::update_geometry()
{

    auto view = m_scene.get_registry().view<CompPlanet>();

    for (ActiveEnt ent : view)
    {
        CompPlanet &planet = view.get<CompPlanet>(ent);

        if (!planet.m_planet.is_initialized())
        {
            // initialize planet if not done so yet
            planet.m_planet.initialize(planet.m_radius);
            std::cout << "planet init\n";

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

void SysPlanetA::draw(CompCamera const& camera)
{
    using Magnum::GL::Renderer;

    auto drawGroup = m_scene.get_registry().group<CompPlanet>(
                            entt::get<CompTransform>);

    Matrix4 entRelative;

    for(auto entity: drawGroup)
    {
        CompPlanet& planet = drawGroup.get<CompPlanet>(entity);
        CompTransform& transform = drawGroup.get<CompTransform>(entity);

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
