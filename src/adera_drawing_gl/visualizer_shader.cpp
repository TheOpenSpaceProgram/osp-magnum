/**
 * Open Space Program
 * Copyright © 2019-2021 Open Space Program Project
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

#include "visualizer_shader.h"

#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Renderer.h>

// for the 0xrrggbb_rgbf and _deg literals
using namespace Magnum::Math::Literals;

using namespace osp;
using namespace osp::draw;

void adera::shader::draw_ent_visualizer(
        DrawEnt                     ent,
        ViewProjMatrix const&       viewProj,
        EntityToDraw::UserData_t    userData) noexcept
{
    using Magnum::Shaders::MeshVisualizerGL3D;

    void* const pData = std::get<0>(userData);
    assert(pData != nullptr);
    auto &rData = *reinterpret_cast<ACtxDrawMeshVisualizer*>(pData);

    Matrix4 const&  drawTf      = (*rData.m_pDrawTf)[ent];
    Matrix4 const   entRelative = viewProj.m_view * drawTf;

    MeshVisualizer &rShader = rData.m_shader;

    if (rShader.flags() & MeshVisualizer::Flag::NormalDirection)
    {
        rShader.setNormalMatrix(entRelative.normalMatrix());
    }

    if (rData.m_wireframeOnly)
    {
        rShader.setColor(0x00000000_rgbaf);
        Magnum::GL::Renderer::setDepthMask(GL_FALSE);
    }

    MeshGlId const      meshId = (*rData.m_pMeshId)[ent].m_glId;
    Magnum::GL::Mesh    &rMesh = rData.m_pMeshGl->get(meshId);

    rShader
        .setViewportSize(Vector2{Magnum::GL::defaultFramebuffer.viewport().size()})
        .setTransformationMatrix(entRelative)
        .setProjectionMatrix(viewProj.m_proj)
        .draw(rMesh);

    if (rData.m_wireframeOnly)
    {
        Magnum::GL::Renderer::setDepthMask(GL_TRUE);
    }
}
