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
#pragma once

#include "../PlanetGeometryA.h"

#include <osp/Universe.h>
#include <osp/Active/SysAreaAssociate.h>
#include <osp/Active/SysForceFields.h>
#include <osp/Active/activetypes.h>

#include <osp/UserInputHandler.h>

#include <Magnum/Shaders/MeshVisualizer.h>
#include <Magnum/GL/Mesh.h>


namespace planeta::active
{

struct ACompPlanet
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

    static const std::string smc_name;

    SysPlanetA(osp::active::ActiveScene &scene,
               osp::UserInputHandler &userInput);
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
                                     ACompPlanet &planet,
                                     chindex_t chunk);

    void update_geometry();

    void update_physics();

private:

    osp::active::ActiveScene &m_scene;

    osp::active::UpdateOrderHandle m_updateGeometry;
    osp::active::UpdateOrderHandle m_updatePhysics;

    osp::active::RenderOrderHandle m_renderPlanetDraw;

    osp::ButtonControlHandle m_debugUpdate;
};

}
