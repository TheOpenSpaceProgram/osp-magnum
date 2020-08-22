#pragma once

#include "../PlanetGeometryA.h"

#include <osp/Universe.h>
#include <osp/Satellites/SatActiveArea.h>

#include <osp/Active/activetypes.h>

#include <Magnum/Shaders/MeshVisualizer.h>
#include <Magnum/GL/Mesh.h>


namespace planeta
{

using namespace osp;
using namespace osp::universe;

struct CompPlanet
{


    PlanetGeometryA m_planet;
    Magnum::GL::Mesh m_mesh{};
    Magnum::Shaders::MeshVisualizer3D m_shader{
            Magnum::Shaders::MeshVisualizer3D::Flag::Wireframe
            | Magnum::Shaders::MeshVisualizer3D::Flag::NormalDirection};
    Magnum::GL::Buffer m_vrtxBufGL{};
    Magnum::GL::Buffer m_indxBufGL{};
    double m_radius;
};


class SysPlanetA : public IDynamicSystem
{
public:

    static int area_activate_planet(ActiveScene& scene, universe::SatActiveArea& area,
                        Satellite areaSat, Satellite loadMe);

    SysPlanetA(ActiveScene &scene);
    ~SysPlanetA() = default;

    void draw(CompCamera const& camera);

    void debug_create_chunk_collider(ActiveEnt ent, CompPlanet &planet,
                                     chindex_t chunk);

    void update_geometry();

    void update_physics();

private:

    ActiveScene &m_scene;

    UpdateOrderHandle m_updateGeometry;
    UpdateOrderHandle m_updatePhysics;

    RenderOrderHandle m_renderPlanetDraw;
};

}
