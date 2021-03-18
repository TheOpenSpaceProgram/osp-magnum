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

#include <variant>
#include <Magnum/Math/Color.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Texture.h>

#include "osp/Resource/Resource.h"
#include "osp/Active/Shader.h"
#include "../types.h"
#include "activetypes.h"
#include "adera/Shaders/PlumeShader.h"

namespace osp::active
{

struct CompDrawableDebug
{
    DependRes<Magnum::GL::Mesh> m_mesh;
    ShaderDrawFnc_t m_shader_draw;
    Magnum::Color4 m_color;
};

struct CompTransparentDebug
{
    bool m_state = false;
};

struct CompVisibleDebug
{
    bool m_state = true;
};

class SysDebugRender
{
public:

    static void add_functions(ActiveScene& rScene);

    static void draw(ActiveScene& rScene, ACompCamera& camera);

    // TODO staticify
    void add_pass(std::function<void(ActiveScene&, ACompCamera&)> passDef)
    { m_renderPasses.push_back(std::move(passDef)); }

    template <typename T>
    static void draw_group(ActiveScene& rScene, T& rCollection, ACompCamera const& camera);
    static void render_framebuffer(ActiveScene& rScene, Magnum::GL::Texture2D& rTexture);
    // TODO
    std::vector<std::function<void(ActiveScene&, ACompCamera&)>> m_renderPasses;

};

template<typename T>
inline void SysDebugRender::draw_group(ActiveScene& rScene, T& rCollection, ACompCamera& camera)
{
    for (auto entity : rCollection)
    {
        auto& drawable = rCollection.template get<CompDrawableDebug>(entity);
        auto const& transform = rCollection.template get<ACompTransform>(entity);
        auto const* visible = rScene.get_registry().try_get<CompVisibleDebug>(entity);

        if (visible && !visible->m_state) { continue; }

        drawable.m_shader_draw(entity, rScene, *drawable.m_mesh, camera, transform);
    }
}

}
