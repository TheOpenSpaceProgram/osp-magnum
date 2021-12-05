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


namespace osp::shader
{

using Phong = Magnum::Shaders::PhongGL;

/**
 * @brief Stores per-scene data needed for Phong shaders to draw
 */
struct ACtxDrawPhong
{

    DependRes<Phong> m_shaderUntextured;
    DependRes<Phong> m_shaderDiffuse;

    active::acomp_storage_t<active::ACompDrawTransform> *m_pDrawTf;
    active::acomp_storage_t<active::ACompTextureGL>     *m_pDiffuseTexGl;
    active::acomp_storage_t<active::ACompMeshGL>        *m_pMeshGl;
};

void draw_ent_phong(
        active::ActiveEnt ent,
        active::ACompCamera const& camera,
        active::EntityToDraw::UserData_t userData) noexcept;

/**
 * @brief Assign a Phong shader to a set of entities, and write results to
 *        a RenderGroup
 *
 * @param entities              [in] Entities to consider
 * @param pStorageOpaque        [out] Optional RenderGroup storage for opaque
 * @param pStorageTransparent   [out] Optional RenderGroup storage for transparent
 * @param viewOpaque            [in] View for opaque component
 * @param viewDiffuse           [in] View for diffuse texture component
 * @param rData                 [in] Phong shader data, stable memory required
 */
void assign_phong(
        active::RenderGroup::ArrayView_t entities,
        active::RenderGroup::Storage_t *pStorageOpaque,
        active::RenderGroup::Storage_t *pStorageTransparent,
        active::acomp_view_t<active::ACompOpaque const> viewOpaque,
        active::acomp_view_t<active::ACompTextureGL const> viewDiffuse,
        ACtxDrawPhong &rData);


} // namespace osp::shader
