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


    for (std::size_t const drawEntInt : rCtxDrawing.m_drawIds.bitview().zeros())
    {
        auto const drawEnt = DrawEnt(drawEntInt);

        // Set all meshs dirty
        if (rCtxDrawing.m_mesh[drawEnt] != lgrn::id_null<MeshId>())
        {
            rCtxDrawing.m_meshDirty.push_back(drawEnt);
        }

        // Set all textures dirty
        if (rCtxDrawing.m_diffuseTex[drawEnt] != lgrn::id_null<TexId>())
        {
            rCtxDrawing.m_diffuseDirty.push_back(drawEnt);
        }
    }

    for (std::size_t const materialInt : rCtxDrawing.m_materialIds.bitview().zeros())
    {
        Material &mat = rCtxDrawing.m_materials[MaterialId(materialInt)];
        for (std::size_t const entInt : mat.m_ents.ones())
        {
            mat.m_dirty.push_back(DrawEnt(entInt));
        }
    }
}


void SysRender::update_draw_transforms_recurse(
        ACtxSceneGraph const&                   rScnGraph,
        KeyedVec<ActiveEnt, DrawEnt> const&     activeToDraw,
        acomp_storage_t<ACompTransform> const&  rTf,
        DrawTransforms_t&                       rDrawTf,
        ActiveEntSet_t const&                   needDrawTf,
        ActiveEnt                               ent,
        Matrix4 const&                          parentTf,
        bool                                    root)
{
    Matrix4 const& entTf        = rTf.get(ent).m_transform;
    Matrix4 const& entDrawTf    = root ? (entTf) : (parentTf * entTf);

    if (DrawEnt const drawEnt = activeToDraw[ent];
        drawEnt != lgrn::id_null<DrawEnt>())
    {
        rDrawTf[drawEnt] = entDrawTf;
    }

    for (ActiveEnt entChild : SysSceneGraph::children(rScnGraph, ent))
    {
        if (needDrawTf.test(std::size_t(entChild)))
        {
            update_draw_transforms_recurse(rScnGraph, activeToDraw, rTf, rDrawTf, needDrawTf, entChild, entDrawTf, false);
        }
    }
}


MeshIdOwner_t SysRender::add_drawable_mesh(ACtxDrawing& rDrawing, ACtxDrawingRes& rDrawingRes, Resources& rResources, PkgId const pkg, std::string_view const name)
{
    ResId const res = rResources.find(restypes::gc_mesh, pkg, name);
    assert(res != lgrn::id_null<ResId>());
    MeshId const meshId = SysRender::own_mesh_resource(rDrawing, rDrawingRes, rResources, res);
    return rDrawing.m_meshRefCounts.ref_add(meshId);
}


