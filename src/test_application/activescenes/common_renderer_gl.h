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

/**
 * @brief Common data needed to render a scene
 *
 * Note: GPU resources and application-level rendering data can be found in
 *       osp::active::RenderGL
 *
 */
struct CommonSceneRendererGL : MultiAny
{
    using on_draw_t = void(*)(CommonSceneRendererGL&, CommonTestScene&,
                              ActiveApplication&, float) noexcept;

    on_draw_t m_onDraw{nullptr};

    osp::active::ACtxSceneRenderGL m_renderGl;

    osp::active::ACtxRenderGroups m_renderGroups;

    osp::shader::ACtxDrawPhong m_phong;
    osp::shader::ACtxDrawMeshVisualizer m_visualizer;

    osp::active::ActiveEnt m_camera;

    /**
     * @brief Setup default shaders and render groups
     *
     * @param rApp      [ref] Application with GL context
     */
    void setup(ActiveApplication& rApp);

    /**
     * @brief Sync GL resources with scene meshes, textures, and materials
     *
     * @param rApp      [ref] Application with GL context
     * @param rScene    [ref] Associated test scene to render
     */
    void sync(ActiveApplication& rApp, CommonTestScene const& rScene);

    /**
     * @brief Bind and clear off-screen framebuffer object
     *
     * @param rApp      [ref] Application with GL context
     */
    void prepare_fbo(ActiveApplication& rApp);

    /**
     * @brief Calls draw functions of entities in the fwd_opaque RenderGroup
     *
     * @param rApp      [ref] Application with GL context
     * @param rScene    [ref] Associated test scene to render
     */
    void draw_entities(ActiveApplication& rApp, CommonTestScene const& rScene);

    /**
     * @brief Display framebuffer texture to window
     *
     * @param rApp      [ref] Application with GL context
     */
    void display(ActiveApplication& rApp);

    /**
     * @brief Delete components of entities to delete
     *
     * @param toDelete  [in] Vector of deleted entities
     */
    void update_delete(std::vector<osp::active::ActiveEnt> const& toDelete);
};


using setup_renderer_t = void(*)(CommonSceneRendererGL&, CommonTestScene&, ActiveApplication&) noexcept;

/**
 * @brief Generate a draw function for drawing a single common scene
 *
 * @param rScene [ref] Common scene to draw. Must be in stable memory.
 * @param rApp   [ref] Application with GL context
 * @param setup  [in] Scene-specific setup function
 *
 * @return ActiveApplication draw function
 */
on_draw_t generate_common_draw(
        CommonTestScene& rScene,
        ActiveApplication& rApp,
        setup_renderer_t setup);


} // namespace testapp
