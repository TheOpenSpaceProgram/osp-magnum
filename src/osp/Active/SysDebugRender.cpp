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
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Renderer.h>

#include "SysDebugRender.h"
#include "ActiveScene.h"
#include "adera/Shaders/Phong.h"
#include "adera/Shaders/PlumeShader.h"


using namespace osp::active;

// for _1, _2, _3, ... std::bind arguments
using namespace std::placeholders;

// for the 0xrrggbb_rgbf and _deg literals
using namespace Magnum::Math::Literals;

SysDebugRender::SysDebugRender(ActiveScene &rScene) :
        m_scene(rScene),
        m_renderDebugDraw(rScene.get_render_order(), "debug", "", "",
                          std::bind(&SysDebugRender::draw, this, _1))
{
    Package& glResources = m_scene.get_context_resources();

    using namespace adera::shader;

    glResources.add<Phong>("phong_shader",
        Phong{Magnum::Shaders::Phong::Flag::DiffuseTexture});

    glResources.add<PlumeShader>("plume_shader");
}

void SysDebugRender::draw(ACompCamera const& camera)
{
    using Magnum::GL::Renderer;

    auto& reg = m_scene.get_registry();

    Renderer::enable(Renderer::Feature::DepthTest);
    Renderer::enable(Renderer::Feature::FaceCulling);



    // Get opaque objects
    auto opaqueObjects = reg.view<CompDrawableDebug, ACompTransform>(
        entt::exclude<CompTransparentDebug>);
    // Configure blend mode for opaque rendering
    Renderer::disable(Renderer::Feature::Blending);
    // Draw opaque objects
    draw_group(opaqueObjects, camera);

    // Get transparent objects
    auto transparentObjects = m_scene.get_registry()
        .view<CompDrawableDebug, CompVisibleDebug,
        CompTransparentDebug, ACompTransform>();

    // Configure blend mode for transparency
    Renderer::enable(Renderer::Feature::Blending);
    Renderer::setBlendFunction(
        Renderer::BlendFunction::SourceAlpha,
        Renderer::BlendFunction::OneMinusSourceAlpha);

    // Draw backfaces first
    Renderer::setFaceCullingMode(Renderer::PolygonFacing::Front);
    draw_group(transparentObjects, camera);

    // Then draw frontfaces
    Renderer::setFaceCullingMode(Renderer::PolygonFacing::Back);
    draw_group(transparentObjects, camera);
}
