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


#include <osp/Active/opengl/SysRenderGL.h>
#include <osp/Resource/Resource.h>

#include <Magnum/Shaders/PhongGL.h>
#include <Magnum/Math/Color.h>

#include <string_view>
#include <vector>

namespace osp::shader
{

struct ACtxPhongData;

class Phong : protected Magnum::Shaders::PhongGL
{
    using RenderGroup = osp::active::RenderGroup;

public:

    using Magnum::Shaders::PhongGL::PhongGL;
    using Flag = Magnum::Shaders::PhongGL::Flag;

    static void draw_entity(
            osp::active::ActiveEnt ent,
            osp::active::ACompCamera const& camera,
            osp::active::EntityToDraw::UserData_t userData) noexcept;

    /**
     * @brief Assign a Phong shader to a set of entities, and write results to
     *        a RenderGroup
     *
     * @param entities      [in] Entities to consider
     * @param rStorage      [out] RenderGroup storage
     * @param viewOpaque    [in] View for opaque component
     * @param viewDiffuse   [in] View for diffuse texture component
     * @param rData         [in] Phong shader data, stable memory required
     */
    static void assign_phong_opaque(
            RenderGroup::ArrayView_t entities,
            RenderGroup::Storage_t& rStorage,
            osp::active::acomp_view_t<osp::active::ACompOpaque const> viewOpaque,
            osp::active::acomp_view_t<osp::active::ACompTextureGL const> viewDiffuse,
            ACtxPhongData &rData);
};

/**
 * @brief Stores per-scene data needed for Phong shaders to draw
 */
struct ACtxPhongData
{
    struct Views
    {
        active::acomp_view_t< osp::active::ACompDrawTransform > m_drawTf;
        active::acomp_view_t< osp::active::ACompTextureGL >     m_diffuseTexGl;
        active::acomp_view_t< osp::active::ACompMeshGL >        m_meshGl;
    };

    DependRes<Phong> m_shaderUntextured;
    DependRes<Phong> m_shaderDiffuse;

    std::optional<Views> m_views;
};

} // namespace osp::shader
