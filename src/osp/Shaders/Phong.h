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

#include <Magnum/Shaders/PhongGL.h>

namespace osp::shader
{

using Phong = Magnum::Shaders::PhongGL;

/**
 * @brief Stores per-scene data needed for Phong shaders to draw
 */
struct ACtxDrawPhong
{
    template <typename T>
    using acomp_storage_t = osp::active::acomp_storage_t<T>;

    Phong m_shaderUntextured    {Corrade::NoCreate};
    Phong m_shaderDiffuse       {Corrade::NoCreate};

    osp::active::DrawTransforms_t               *m_pDrawTf{nullptr};
    osp::KeyedVec<osp::active::DrawEnt, Magnum::Color4> *m_pColor{nullptr};
    osp::active::TexGlEntStorage_t              *m_pDiffuseTexId{nullptr};
    osp::active::MeshGlEntStorage_t             *m_pMeshId{nullptr};

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

void draw_ent_phong(
        active::DrawEnt ent,
        active::ViewProjMatrix const& viewProj,
        active::EntityToDraw::UserData_t userData) noexcept;

/**
 * @brief Assign a Phong shader to a set of entities, and write results to
 *        a RenderGroup
 *
 * @param dirtyFirst            [in] Iterator to first entity to sync
 * @param dirtyLast             [in] Iterator to last entity to sync
 * @param hasMaterial           [in] Which entities a phong material is assigned to
 * @param pStorageOpaque        [out] Optional RenderGroup storage for opaque
 * @param pStorageTransparent   [out] Optional RenderGroup storage for transparent
 * @param opaque                [in] Storage for opaque component
 * @param diffuse               [in] Storage for diffuse texture component
 * @param rData                 [in] Phong shader data, stable memory required
 */
template<typename ITA_T, typename ITB_T>
void sync_phong(
        ITA_T dirtyIt,
        ITB_T const& dirtyLast,
        active::EntSet_t const& hasMaterial,
        active::RenderGroup::Storage_t *const pStorageOpaque,
        active::RenderGroup::Storage_t *const pStorageTransparent,
        KeyedVec<active::DrawEnt, active:: BasicDrawProps> const& drawBasic,
        active::TexGlEntStorage_t const& diffuse,
        ACtxDrawPhong &rData)
{
    using namespace active;

    for (; dirtyIt != dirtyLast; std::advance(dirtyIt, 1))
    {
        DrawEnt const ent = *dirtyIt;

        // Erase from group if they exist
        if (pStorageOpaque != nullptr)
        {
            pStorageOpaque->remove(ent);
        }
        if (pStorageTransparent != nullptr)
        {
            pStorageTransparent->remove(ent);
        }

        if ( ! hasMaterial.test(std::size_t(ent)))
        {
            continue; // Phong material is not assigned to this entity
        }

        Phong *pShader = (diffuse[ent].m_glId != lgrn::id_null<TexGlId>())
                       ? &rData.m_shaderDiffuse
                       : &rData.m_shaderUntextured;

        if (drawBasic[ent].m_opaque)
        {
            if (pStorageOpaque == nullptr)
            {
                continue;
            }

            pStorageOpaque->emplace(ent, EntityToDraw{&draw_ent_phong, {&rData, pShader} });
        }
        else
        {
            if (pStorageTransparent == nullptr)
            {
                continue;
            }

            pStorageTransparent->emplace(ent, EntityToDraw{&draw_ent_phong, {&rData, pShader} });
        }
    }
}


} // namespace osp::shader
