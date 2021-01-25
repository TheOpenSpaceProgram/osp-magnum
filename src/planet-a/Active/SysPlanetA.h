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
    std::shared_ptr<IcoSphereTree> m_icoTree;
    std::shared_ptr<PlanetGeometryA> m_planet;
    Magnum::GL::Mesh m_mesh{};
    Magnum::Shaders::MeshVisualizer3D m_shader{
            Magnum::Shaders::MeshVisualizer3D::Flag::Wireframe
            | Magnum::Shaders::MeshVisualizer3D::Flag::NormalDirection};
    Magnum::GL::Buffer m_vrtxBufGL{};
    Magnum::GL::Buffer m_indxBufGL{};
    double m_radius;
};


class SysPlanetA : public osp::active::IDynamicSystem
{
public:

    static const std::string smc_name;

    SysPlanetA(osp::active::ActiveScene &scene,
               osp::UserInputHandler &userInput);
    ~SysPlanetA() = default;

    static osp::active::ActiveEnt activate(
            osp::active::ActiveScene &rScene, osp::universe::Universe &rUni,
            osp::universe::Satellite areaSat, osp::universe::Satellite tgtSat);

    void draw(osp::active::ACompCamera const& camera);

    void debug_create_chunk_collider(osp::active::ActiveEnt ent,
                                     ACompPlanet &planet,
                                     chindex_t chunk);

    static void planet_update_geometry(osp::active::ActiveEnt planetEnt,
                                osp::active::ActiveScene& rScene);

    static void update_activate(osp::active::ActiveScene& rScene);

    void update_geometry(osp::active::ActiveScene& rScene);

    void update_physics(osp::active::ActiveScene& rScene);

private:

    osp::active::ActiveScene &m_scene;

    osp::active::UpdateOrderHandle_t m_updateActivate;
    osp::active::UpdateOrderHandle_t m_updateGeometry;
    osp::active::UpdateOrderHandle_t m_updatePhysics;

    osp::active::RenderOrderHandle_t m_renderPlanetDraw;

    osp::ButtonControlHandle m_debugUpdate;
};

}
