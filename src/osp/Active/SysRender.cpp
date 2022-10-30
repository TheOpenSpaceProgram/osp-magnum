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
#include "SysRender.h"
#include "SysSceneGraph.h"


#include "../Resource/resources.h"

using namespace osp;
using namespace osp::active;

MeshId SysRender::own_mesh_resource(ACtxDrawing& rCtxDrawing, ACtxDrawingRes& rCtxDrawingRes, Resources &rResources, ResId const resId)
{
    auto const& [it, success] = rCtxDrawingRes.m_resToMesh.try_emplace(resId);
    if (success)
    {
        ResIdOwner_t owner = rResources.owner_create(restypes::gc_mesh, resId);
        MeshId const meshId = rCtxDrawing.m_meshIds.create();
        rCtxDrawingRes.m_meshToRes.emplace(meshId, std::move(owner));
        it->second = meshId;
        return meshId;
    }
    return it->second;
};

TexId SysRender::own_texture_resource(ACtxDrawing& rCtxDrawing, ACtxDrawingRes& rCtxDrawingRes, Resources &rResources, ResId const resId)
{
    auto const& [it, success] = rCtxDrawingRes.m_resToTex.try_emplace(resId);
    if (success)
    {
        ResIdOwner_t owner = rResources.owner_create(restypes::gc_mesh, resId);
        TexId const texId = rCtxDrawing.m_texIds.create();
        rCtxDrawingRes.m_texToRes.emplace(texId, std::move(owner));
        it->second = texId;
        return texId;
    }
    return it->second;
};

void SysRender::clear_owners(ACtxDrawing& rCtxDrawing)
{
    for (TexIdOwner_t &rOwner : std::exchange(rCtxDrawing.m_diffuseTex, {}))
    {
        rCtxDrawing.m_texRefCounts.ref_release(std::move(rOwner));
    }

    for (MeshIdOwner_t &rOwner : std::exchange(rCtxDrawing.m_mesh, {}))
    {
        rCtxDrawing.m_meshRefCounts.ref_release(std::move(rOwner));
    }
}

void SysRender::clear_resource_owners(ACtxDrawingRes& rCtxDrawingRes, Resources &rResources)
{
    for ([[maybe_unused]] auto && [_, rOwner] : std::exchange(rCtxDrawingRes.m_texToRes, {}))
    {
        rResources.owner_destroy(restypes::gc_texture, std::move(rOwner));
    }
    rCtxDrawingRes.m_resToTex.clear();

    for ([[maybe_unused]] auto && [_, rOwner] : std::exchange(rCtxDrawingRes.m_meshToRes, {}))
    {
        rResources.owner_destroy(restypes::gc_mesh, std::move(rOwner));
    }
    rCtxDrawingRes.m_resToMesh.clear();
}

void SysRender::set_dirty_all(ACtxDrawing &rCtxDrawing)
{
    using osp::active::active_sparse_set_t;

    // Set all meshs dirty
    auto &rMeshSet = static_cast<active_sparse_set_t&>(rCtxDrawing.m_mesh);
    rCtxDrawing.m_meshDirty.assign(std::begin(rMeshSet), std::end(rMeshSet));

    // Set all textures dirty
    auto &rDiffSet = static_cast<active_sparse_set_t&>(rCtxDrawing.m_diffuseTex);
    rCtxDrawing.m_diffuseDirty.assign(std::begin(rMeshSet), std::end(rMeshSet));
}

void SysRender::clear_dirty_all(ACtxDrawing& rCtxDrawing)
{
    rCtxDrawing.m_meshDirty.clear();
    rCtxDrawing.m_diffuseDirty.clear();
}


void SysRender::update_draw_transforms_recurse(
        ACtxSceneGraph const&                   rScnGraph,
        acomp_storage_t<ACompTransform> const&  rTf,
        acomp_storage_t<Matrix4>&               rDrawTf,
        EntSet_t const&                         rDrawable,
        ActiveEnt                               ent,
        Matrix4 const&                          parentTf,
        bool                                    root)
{
    Matrix4 const& entTf = rTf.get(ent).m_transform;
    Matrix4 const& entDrawTf = root ? (entTf) : (parentTf * entTf);

    if (rDrawTf.contains(ent))
    {
        rDrawTf.get(ent) = entDrawTf;
    }

    for (ActiveEnt entChild : SysSceneGraph::children(rScnGraph, ent))
    {
        if (rDrawable.test(std::size_t(entChild)))
        {
            update_draw_transforms_recurse(rScnGraph, rTf, rDrawTf, rDrawable, entChild, entDrawTf, false);
        }
    }

    //parentTf.
}

