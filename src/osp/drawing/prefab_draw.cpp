/**
 * Open Space Program
 * Copyright © 2019-2023 Open Space Program Project
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
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHEfR DEALINGS IN THE
 * SOFTWARE.
 */

#include "prefab_draw.h"
#include "drawing_fn.h"

#include <osp/core/Resources.h>
#include <osp/vehicles/ImporterData.h>

#include <Magnum/Trade/Trade.h>
#include <Magnum/Trade/PbrMetallicRoughnessMaterialData.h>

using namespace osp;
using namespace osp::active;
using namespace osp::draw;

using osp::restypes::gc_importer;

void SysPrefabDraw::init_drawents(
        ACtxPrefabs&                rPrefabs,
        Resources&                  rResources,
        ACtxBasic const&            rBasic,
        ACtxDrawing&                rDrawing,
        ACtxSceneRender&            rScnRender)
{
    auto itPfEnts = rPrefabs.spawnedEntsOffset.begin();

    for (TmpPrefabRequest const& request : rPrefabs.spawnRequest)
    {
        auto const &rImportData = rResources.data_get<osp::ImporterData const>(gc_importer, request.m_importerRes);
        auto const &rPrefabData = rResources.data_get<osp::Prefabs const>     (gc_importer, request.m_importerRes);
        auto const objects      = rPrefabData.m_prefabs[request.m_prefabId];
        auto const ents         = ArrayView<ActiveEnt const>{*itPfEnts};

        for (std::size_t i = 0; i < objects.size(); ++i)
        {
            int const meshImportId = rImportData.m_objMeshes[objects[i]];
            if (meshImportId == -1)
            {
                continue;
            }

            ActiveEnt const ent = ents[i];
            rScnRender.m_activeToDraw[ent] = rScnRender.m_drawIds.create();
        }

        ++itPfEnts;
    }
}

void SysPrefabDraw::resync_drawents(
        ACtxPrefabs&                rPrefabs,
        Resources&                  rResources,
        ACtxBasic const&            rBasic,
        ACtxDrawing&                rDrawing,
        ACtxSceneRender&            rScnRender)
{
    for (ActiveEnt const root : rPrefabs.roots)
    {
        PrefabInstanceInfo const &rRootInfo = rPrefabs.instanceInfo[root];

        LGRN_ASSERT(rRootInfo.prefab   != lgrn::id_null<PrefabId>());
        LGRN_ASSERT(rRootInfo.importer != lgrn::id_null<ResId>());

        auto const &rImportData = rResources.data_get<osp::ImporterData const>(gc_importer, rRootInfo.importer);
        auto const &rPrefabData = rResources.data_get<osp::Prefabs const>     (gc_importer, rRootInfo.importer);
        auto const objects      = rPrefabData.m_prefabs[rRootInfo.prefab];

        for (ActiveEnt const ent : SysSceneGraph::descendants(rBasic.m_scnGraph, root))
        {
            PrefabInstanceInfo const &rInfo = rPrefabs.instanceInfo[ent];

            int const meshImportId = rImportData.m_objMeshes[objects[rInfo.obj]];
            if (meshImportId == -1)
            {
                continue;
            }

            LGRN_ASSERT(rScnRender.m_activeToDraw[ent] == lgrn::id_null<DrawEnt>());
            rScnRender.m_activeToDraw[ent] = rScnRender.m_drawIds.create();
        }
    }
}

void SysPrefabDraw::init_mesh_texture_material(
        ACtxPrefabs&                rPrefabs,
        Resources&                  rResources,
        ACtxBasic const&            rBasic,
        ACtxDrawing&                rDrawing,
        ACtxDrawingRes&             rDrawingRes,
        ACtxSceneRender&            rScnRender,
        MaterialId                  material)
{
    auto itPfEnts = rPrefabs.spawnedEntsOffset.begin();

    for (TmpPrefabRequest const& request : rPrefabs.spawnRequest)
    {
        auto const &rImportData = rResources.data_get<osp::ImporterData const>(gc_importer, request.m_importerRes);
        auto const &rPrefabData = rResources.data_get<osp::Prefabs const>     (gc_importer, request.m_importerRes);
        auto const objects  = lgrn::Span<int const>{rPrefabData.m_prefabs[request.m_prefabId]};
        auto const parents  = lgrn::Span<int const>{rPrefabData.m_prefabParents[request.m_prefabId]};
        auto const ents     = ArrayView<ActiveEnt const>{*itPfEnts};

        // All ancestors of each entity that has a mesh
        auto const needs_draw_transform
            = [&parents, &ents, &rDrawing, &needDrawTf = rScnRender.m_needDrawTf]
              (auto&& self, int const object, ActiveEnt const ent) noexcept -> void
        {
            needDrawTf.insert(ent);

            int const parentObj = parents[object];

            if (parentObj != -1)
            {
                self(self, parentObj, ents[parentObj]);
            }
        };

        for (std::size_t i = 0; i < objects.size(); ++i)
        {
            ActiveEnt const ent = ents[i];

            // Check if object has mesh
            int const meshImportId = rImportData.m_objMeshes[objects[i]];
            if (meshImportId == -1)
            {
                continue;
            }

            needs_draw_transform(needs_draw_transform, objects[i], ent);

            DrawEnt const drawEnt = rScnRender.m_activeToDraw[ent];

            osp::ResId const meshRes = rImportData.m_meshes[meshImportId];
            MeshId const meshId = SysRender::own_mesh_resource(rDrawing, rDrawingRes, rResources, meshRes);
            rScnRender.m_mesh[drawEnt] = rDrawing.m_meshRefCounts.ref_add(meshId);
            rScnRender.m_meshDirty.push_back(drawEnt);

            int const matImportId = rImportData.m_objMaterials[objects[i]];

            if (Magnum::Trade::MaterialData const &mat = *rImportData.m_materials.at(matImportId);
                mat.types() & Magnum::Trade::MaterialType::PbrMetallicRoughness)
            {
                auto const& matPbr = mat.as<Magnum::Trade::PbrMetallicRoughnessMaterialData>();
                if (auto const baseColor = matPbr.baseColorTexture();
                    baseColor != -1)
                {
                    osp::ResId const texRes = rImportData.m_textures[baseColor];
                    TexId const texId = SysRender::own_texture_resource(rDrawing, rDrawingRes, rResources, texRes);
                    rScnRender.m_diffuseTex[drawEnt] = rDrawing.m_texRefCounts.ref_add(texId);
                    rScnRender.m_diffuseDirty.push_back(drawEnt);
                }
            }

            rScnRender.m_opaque.insert(drawEnt);
            rScnRender.m_visible.insert(drawEnt);

            if (material != lgrn::id_null<MaterialId>())
            {
                rScnRender.m_materials[material].m_dirty.push_back(drawEnt);
                rScnRender.m_materials[material].m_ents.insert(drawEnt);
            }
        }

        ++itPfEnts;
    }
}


void SysPrefabDraw::resync_mesh_texture_material(
        ACtxPrefabs&                rPrefabs,
        Resources&                  rResources,
        ACtxBasic const&            rBasic,
        ACtxDrawing&                rDrawing,
        ACtxDrawingRes&             rDrawingRes,
        ACtxSceneRender&            rScnRender,
        MaterialId                  material)
{
    for (ActiveEnt const root : rPrefabs.roots)
    {
        PrefabInstanceInfo const &rRootInfo = rPrefabs.instanceInfo[root];

        LGRN_ASSERT(rRootInfo.prefab   != lgrn::id_null<PrefabId>());
        LGRN_ASSERT(rRootInfo.importer != lgrn::id_null<ResId>());

        auto const &rImportData = rResources.data_get<osp::ImporterData const>(gc_importer, rRootInfo.importer);
        auto const &rPrefabData = rResources.data_get<osp::Prefabs const>     (gc_importer, rRootInfo.importer);
        auto const objects  = lgrn::Span<int const>{rPrefabData.m_prefabs[rRootInfo.prefab]};

        for (ActiveEnt const ent : SysSceneGraph::descendants(rBasic.m_scnGraph, root))
        {
            PrefabInstanceInfo const &rInfo = rPrefabs.instanceInfo[ent];

            int const meshImportId = rImportData.m_objMeshes[objects[rInfo.obj]];
            if (meshImportId == -1)
            {
                continue;
            }

            SysRender::needs_draw_transforms(rBasic.m_scnGraph, rScnRender.m_needDrawTf, ent);

            DrawEnt const drawEnt = rScnRender.m_activeToDraw[ent];

            osp::ResId const meshRes = rImportData.m_meshes[meshImportId];
            MeshId const meshId = SysRender::own_mesh_resource(rDrawing, rDrawingRes, rResources, meshRes);
            rScnRender.m_mesh[drawEnt] = rDrawing.m_meshRefCounts.ref_add(meshId);
            rScnRender.m_meshDirty.push_back(drawEnt);

            int const matImportId = rImportData.m_objMaterials[objects[rPrefabs.instanceInfo[ent].obj]];

            if (Magnum::Trade::MaterialData const &mat = *rImportData.m_materials.at(matImportId);
                mat.types() & Magnum::Trade::MaterialType::PbrMetallicRoughness)
            {
                auto const& matPbr = mat.as<Magnum::Trade::PbrMetallicRoughnessMaterialData>();
                if (auto const baseColor = matPbr.baseColorTexture();
                    baseColor != -1)
                {
                    osp::ResId const texRes = rImportData.m_textures[baseColor];
                    TexId const texId = SysRender::own_texture_resource(rDrawing, rDrawingRes, rResources, texRes);
                    rScnRender.m_diffuseTex[drawEnt] = rDrawing.m_texRefCounts.ref_add(texId);
                    rScnRender.m_diffuseDirty.push_back(drawEnt);
                }
            }

            rScnRender.m_opaque.insert(drawEnt);
            rScnRender.m_visible.insert(drawEnt);

            if (material != lgrn::id_null<MaterialId>())
            {
                rScnRender.m_materials[material].m_dirty.push_back(drawEnt);
                rScnRender.m_materials[material].m_ents.insert(drawEnt);
            }
        }
    }
}
