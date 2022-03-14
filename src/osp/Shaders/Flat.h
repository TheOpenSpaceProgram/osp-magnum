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
#pragma once


#include <osp/Active/opengl/SysRenderGL.h>

#include <Magnum/Shaders/FlatGL.h>

namespace osp::shader
{

using Flat = Magnum::Shaders::FlatGL3D;


struct ACtxDrawFlat
{
    template <typename T>
    using acomp_storage_t = osp::active::acomp_storage_t<T>;

    Flat m_shaderUntextured     {Corrade::NoCreate};
    Flat m_shaderDiffuse        {Corrade::NoCreate};

    acomp_storage_t<active::ACompDrawTransform> *m_pDrawTf{nullptr};
    acomp_storage_t<active::ACompColor>         *m_pColor{nullptr};
    osp::active::ACompTexGlStorage_t            *m_pDiffuseTexId{nullptr};
    osp::active::ACompMeshGlStorage_t           *m_pMeshId{nullptr};

    osp::active::TexGlStorage_t                 *m_pTexGl{nullptr};
    osp::active::MeshGlStorage_t                *m_pMeshGl{nullptr};

    constexpr void assign_pointers(active::ACtxSceneRenderGL& rCtxScnGl,
                                   active::RenderGL& rRenderGl) noexcept
    {
        m_pDrawTf       = &rCtxScnGl.m_drawTransform;
        // TODO: ACompColor
        m_pDiffuseTexId = &rCtxScnGl.m_diffuseTexId;
        m_pMeshId       = &rCtxScnGl.m_meshId;

        m_pTexGl        = &rRenderGl.m_texGl;
        m_pMeshGl       = &rRenderGl.m_meshGl;
    }
};

void draw_ent_flat(
        active::ActiveEnt ent,
        active::ViewProjMatrix const& viewProj,
        active::EntityToDraw::UserData_t userData) noexcept;

/**
 * @brief Assign a Flat shader to a set of entities, and write results to
 *        a RenderGroup
 *
 * @param entities              [in] Entities to consider
 * @param pStorageOpaque        [out] Optional RenderGroup storage for opaque
 * @param pStorageTransparent   [out] Optional RenderGroup storage for transparent
 * @param opaque                [in] View for opaque component
 * @param diffuse               [in] View for diffuse texture component
 * @param rData                 [in] Phong shader data, stable memory required
 */
void assign_flat(
        active::RenderGroup::ArrayView_t entities,
        active::RenderGroup::Storage_t *pStorageOpaque,
        active::RenderGroup::Storage_t *pStorageTransparent,
        active::acomp_storage_t<active::ACompOpaque> const& opaque,
        active::acomp_storage_t<active::ACompTexGl> const& diffuse,
        ACtxDrawFlat &rData);


} // namespace osp::shader
