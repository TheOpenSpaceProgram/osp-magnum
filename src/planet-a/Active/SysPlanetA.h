#pragma once

#include <Magnum/Shaders/Phong.h>

#include <Magnum/GL/Mesh.h>

#include <osp/Satellite.h>
#include <osp/Satellites/SatActiveArea.h>

#include <osp/Active/activetypes.h>

#include "../PlanetGeometryA.h"

namespace osp

{

struct CompPlanet
{
    PlanetGeometryA m_planet;
    //Magnum::GL::Mesh m_mesh;
    std::unique_ptr<Magnum::Shaders::Phong> m_shader;
    Magnum::GL::Buffer m_vrtxBufGL;
    Magnum::GL::Buffer m_indxBufGL;
};


class SysPlanetA
{
public:

    static int area_activate_planet(SatActiveArea& area,
                                    SatelliteObject& loadMe);

    void update_render();

    void update_geometry();

    void update_physics();

    SysPlanetA();
};

}
