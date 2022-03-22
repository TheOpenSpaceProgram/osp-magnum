/**
 * Open Space Program
 * Copyright © 2019-2022 Open Space Program Project
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

#include "common_scene.h"

#include "../ActiveApplication.h"

#include <osp/Active/opengl/SysRenderGL.h>

#include <osp/Shaders/Phong.h>
#include <osp/Shaders/MeshVisualizer.h>

namespace testapp
{

struct CommonRendererGL
{
    osp::active::ACtxRenderGroups m_renderGroups;

    osp::active::ACtxSceneRenderGL m_renderGl;

    osp::shader::ACtxDrawPhong m_phong;
    osp::shader::ACtxDrawMeshVisualizer m_visualizer;

    osp::active::ActiveEnt m_camera;

    void setup(ActiveApplication& rApp, CommonTestScene& rScene);

    /**
     * @brief Render a CommonTestScene
     *
     * @param rApp      [ref] Application with GL context and resources
     * @param rScene    [ref] Test scene to render
     * @param rRenderer [ref] Renderer data for test scene
     */
    void render(ActiveApplication& rApp, CommonTestScene& rScene);
    void update_delete(std::vector<osp::active::ActiveEnt> const& toDelete);
};

} // namespace testapp
