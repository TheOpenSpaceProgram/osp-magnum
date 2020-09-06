#pragma once

#include "../PlanetGeometryA.h"

#include <osp/Universe.h>
#include <osp/Active/SysAreaAssociate.h>
#include <osp/Active/SysForceFields.h>
#include <osp/Active/activetypes.h>

#include <Magnum/Shaders/MeshVisualizer.h>
#include <Magnum/GL/Mesh.h>


namespace planeta::active
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


class SysPlanetA : public osp::active::IDynamicSystem,
                   public osp::active::IActivator
{
public:

    //static int area_activate_planet(
    //        osp::active::ActiveScene& scene, osp::active::SysAreaAssociate &area,
    //        osp::universe::Satellite areaSat, osp::universe::Satellite loadMe);

    SysPlanetA(osp::active::ActiveScene &scene);
    ~SysPlanetA() = default;

    osp::active::StatusActivated activate_sat(
            osp::active::ActiveScene &scene,
            osp::active::SysAreaAssociate &area,
            osp::universe::Satellite areaSat,
            osp::universe::Satellite tgtSat);

    int deactivate_sat(osp::active::ActiveScene &scene,
                       osp::active::SysAreaAssociate &area,
                       osp::universe::Satellite areaSat,
                       osp::universe::Satellite tgtSat,
                       osp::active::ActiveEnt tgtEnt);

    void draw(osp::active::ACompCamera const& camera);

    void debug_create_chunk_collider(osp::active::ActiveEnt ent,
                                     CompPlanet &planet,
                                     chindex_t chunk);

    void update_geometry();

    void update_physics();

private:

    osp::active::ActiveScene &m_scene;

    osp::active::UpdateOrderHandle m_updateGeometry;
    osp::active::UpdateOrderHandle m_updatePhysics;

    osp::active::RenderOrderHandle m_renderPlanetDraw;
};

}
