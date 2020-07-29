#pragma once

#include "../PlanetGeometryA.h"

#include <osp/Satellite.h>
#include <osp/Satellites/SatActiveArea.h>

#include <osp/Active/activetypes.h>

#include <Magnum/Shaders/MeshVisualizer.h>
#include <Magnum/GL/Mesh.h>


namespace osp

{

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

    static int area_activate_planet(SatActiveArea& area,
                                    SatelliteObject& loadMe);

    SysPlanetA(ActiveScene &scene);
    ~SysPlanetA() = default;

    void draw(CompCamera const& camera);

    void update_geometry();

    void update_physics();

private:

    ActiveScene &m_scene;

    UpdateOrderHandle m_updateGeometry;
    UpdateOrderHandle m_updatePhysics;

    RenderOrderHandle m_renderPlanetDraw;
};

}
