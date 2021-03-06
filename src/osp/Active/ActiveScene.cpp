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
#include <iostream>

#include "ActiveScene.h"
#include "osp/Active/SysRender.h"
#include "osp/Active/SysHierarchy.h"

using namespace osp;
using namespace osp::active;

void ACompCamera::calculate_projection()
{

    m_projection = Matrix4::perspectiveProjection(
            m_fov, m_viewport.x() / m_viewport.y(),
            m_near, m_far);
}


ActiveScene::ActiveScene(OSPApplication& rApp, Package& rContext)
 : m_app(rApp)
 , m_context(rContext)
{
    // Create the root entity
    m_root = m_registry.create();
}

void ActiveScene::draw()
{
    SysHierarchy::sort(*this);
    SysRender::update_hierarchy_transforms(*this);

    auto renderView = m_registry.view<ACompRenderingAgent,
                                      ACompPerspective3DView, ACompRenderer>();
    for (auto const& [ent, agent, perspective, renderer] : renderView.each())
    {
        DependRes<RenderPipeline> render =
                get_context_resources().get<RenderPipeline>(renderer.m_name);
        auto &rCamera = reg_get<ACompCamera>(perspective.m_camera);
        auto const &cameraDrawTf = reg_get<ACompDrawTransform>(perspective.m_camera);
        auto &rTarget = reg_get<ACompRenderTarget>(agent.m_target);

        rTarget.m_fbo->bind();
        using Magnum::GL::FramebufferClear;
        rTarget.m_fbo->clear(
                FramebufferClear::Color
                | FramebufferClear::Depth
                | FramebufferClear::Stencil);

        rCamera.m_inverse = cameraDrawTf.m_transformWorld.inverted();

        for (RenderStep_t step : render->m_order)
        {
            step(*this, rCamera);
        }
    }

    SysRender::display_default_rendertarget(*this);
}
