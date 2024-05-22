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
#include "drawing_fn.h"
#include "own_restypes.h"

#include "../core/Resources.h"

using namespace osp;
using namespace osp::active;
using namespace osp::draw;

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

void SysRender::clear_owners(ACtxSceneRender& rCtxScnRdr, ACtxDrawing& rCtxDrawing)
{
    for (TexIdOwner_t &rOwner : std::exchange(rCtxScnRdr.m_diffuseTex, {}))
    {
        if (rOwner.has_value())
        {
            rCtxDrawing.m_texRefCounts.ref_release(std::move(rOwner));
        }
    }

    for (MeshIdOwner_t &rOwner : std::exchange(rCtxScnRdr.m_mesh, {}))
    {
        if (rOwner.has_value())
        {
            rCtxDrawing.m_meshRefCounts.ref_release(std::move(rOwner));
        }
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

MeshIdOwner_t SysRender::add_drawable_mesh(ACtxDrawing& rDrawing, ACtxDrawingRes& rDrawingRes, Resources& rResources, PkgId const pkg, std::string_view const name)
{
    ResId const res = rResources.find(restypes::gc_mesh, pkg, name);
    assert(res != lgrn::id_null<ResId>());
    MeshId const meshId = SysRender::own_mesh_resource(rDrawing, rDrawingRes, rResources, res);
    return rDrawing.m_meshRefCounts.ref_add(meshId);
}


