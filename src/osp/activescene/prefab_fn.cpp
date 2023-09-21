/**
 * Open Space Program
 * Copyright © 2019-2022 Open Space Program Project
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
#include "SysPrefabInit.h"
#include "SysRender.h"
#include "SysSceneGraph.h"
#include "SysSceneGraph.h"

#include <osp/Resource/resources.h>
#include <osp/Resource/ImporterData.h>

#include <Magnum/Trade/Trade.h>
#include <Magnum/Trade/PbrMetallicRoughnessMaterialData.h>

using Corrade::Containers::ArrayView;

using osp::restypes::gc_importer;

using osp::phys::EShape;

namespace osp::active
{

void SysPrefabInit::add_to_subtree(
        TmpPrefabInitBasic const&           basic,
        ArrayView<ActiveEnt const>          ents,
        Resources const&                    rResources,
        SubtreeBuilder&                     bldPrefab) noexcept
{
    auto const &rImportData = rResources.data_get<osp::ImporterData const>(
            gc_importer, basic.m_importerRes);
    auto const &rPrefabData = rResources.data_get<osp::Prefabs const>(
            gc_importer, basic.m_importerRes);

    auto const& objs    = rPrefabData.m_prefabs[basic.m_prefabId];
    auto const& parents = rPrefabData.m_prefabParents[basic.m_prefabId];

    auto itObj      = std::begin(objs);
    auto itEnt      = std::begin(ents);

    auto const add_child_recurse
            = [&rImportData, &itObj, &itEnt, &parents] (auto&& self, SubtreeBuilder& bldParent) -> void
    {
        std::size_t const descendants   = rImportData.m_objDescendants[*itObj];
        auto bldChildren = bldParent.add_child(*itEnt, descendants);
        auto children = rImportData.m_objChildren[*itObj];

        std::advance(itObj, 1);
        std::advance(itEnt, 1);

        for ([[maybe_unused]] ObjId const child : children)
        {
            self(self, bldChildren);
        }
    };

    add_child_recurse(add_child_recurse, bldPrefab);

    assert(itObj == std::end(objs));
    assert(itEnt == std::end(ents));
}

void SysPrefabInit::init_transforms(
        ACtxPrefabInit const&               rPrefabInit,
        Resources const&                    rResources,
        acomp_storage_t<ACompTransform>&    rTransform) noexcept
{
    auto itPfEnts = std::begin(rPrefabInit.m_ents);

    for (TmpPrefabInitBasic const& rPfBasic : rPrefabInit.m_basicIn)
    {
        auto const &rImportData = rResources.data_get<osp::ImporterData const>(
                gc_importer, rPfBasic.m_importerRes);
        auto const &rPrefabData = rResources.data_get<osp::Prefabs const>(
                gc_importer, rPfBasic.m_importerRes);

        auto const objects = rPrefabData.m_prefabs[rPfBasic.m_prefabId];
        auto const parents = rPrefabData.m_prefabParents[rPfBasic.m_prefabId];

        for (std::size_t i = 0; i < objects.size(); ++i)
        {
            Matrix4 const& transform = (parents[i] == -1)
                    ? *rPfBasic.m_pTransform
                    : rImportData.m_objTransforms[objects[i]];
            ActiveEnt const ent = (*itPfEnts)[i];
            rTransform.emplace(ent, transform);
        }

        std::advance(itPfEnts, 1);
    }
}

#if 0
void SysPrefabInit::init_drawing(
        ACtxPrefabInit const&               rPrefabInit,
        Resources&                          rResources,
        ACtxDrawing&                        rDrawing,
        ACtxDrawingRes&                     rDrawingRes,
        std::optional<Material>             material) noexcept
{
    auto itPfEnts = rPrefabInit.m_ents.begin();

    // stupidly make drawIDs first
    // TODO: separate this into another step.
    for (TmpPrefabInitBasic const& rPfBasic : rPrefabInit.m_basicIn)
    {
        auto const &rImportData = rResources.data_get<osp::ImporterData const>(
                gc_importer, rPfBasic.m_importerRes);
        auto const &rPrefabData = rResources.data_get<osp::Prefabs const>(
                gc_importer, rPfBasic.m_importerRes);

        auto const objects = rPrefabData.m_prefabs[rPfBasic.m_prefabId];

        for (std::size_t i = 0; i < objects.size(); ++i)
        {
            int const meshImportId = rImportData.m_objMeshes[objects[i]];
            if (meshImportId == -1)
            {
                continue;
            }

            ActiveEnt const ent = (*itPfEnts)[i];
            rDrawing.m_activeToDraw[ent] = rDrawing.m_drawIds.create();
        }

        ++itPfEnts;
    }

    // then resize containers
    rDrawing.resize_draw();

    itPfEnts = rPrefabInit.m_ents.begin();

    for (TmpPrefabInitBasic const& rPfBasic : rPrefabInit.m_basicIn)
    {
        auto const &rImportData = rResources.data_get<osp::ImporterData const>(
                gc_importer, rPfBasic.m_importerRes);
        auto const &rPrefabData = rResources.data_get<osp::Prefabs const>(
                gc_importer, rPfBasic.m_importerRes);

        auto const ents     = ArrayView<ActiveEnt const>{*itPfEnts};
        auto const objects  = lgrn::Span<int const>{rPrefabData.m_prefabs[rPfBasic.m_prefabId]};
        auto const parents  = lgrn::Span<int const>{rPrefabData.m_prefabParents[rPfBasic.m_prefabId]};

        // All ancestors of  each entity that has a mesh
        auto const needs_draw_transform
            = [&parents, &ents, &rDrawing, &needDrawTf = rDrawing.m_needDrawTf]
              (auto && self, int const object, ActiveEnt const ent) noexcept -> void
        {
            needDrawTf.set(std::size_t(ent));

            int const parentObj = parents[object];

            if (parentObj != -1)
            {
                self(self, parentObj, ents[parentObj]);
            }
        };

        for (std::size_t i = 0; i < objects.size(); ++i)
        {
            ActiveEnt const ent = (*itPfEnts)[i];

            // Check if object has mesh
            int const meshImportId = rImportData.m_objMeshes[objects[i]];
            if (meshImportId == -1)
            {
                continue;
            }

            needs_draw_transform(needs_draw_transform, objects[i], ent);

            DrawEnt const drawEnt = rDrawing.m_activeToDraw[ent];

            osp::ResId const meshRes = rImportData.m_meshes[meshImportId];
            MeshId const meshId = SysRender::own_mesh_resource(rDrawing, rDrawingRes, rResources, meshRes);
            rDrawing.m_mesh[drawEnt] = rDrawing.m_meshRefCounts.ref_add(meshId);
            rDrawing.m_meshDirty.push_back(drawEnt);

            int const matImportId = rImportData.m_objMaterials[objects[i]];

            if (Magnum::Trade::MaterialData const &mat = *rImportData.m_materials.at(matImportId);
                mat.types() & Magnum::Trade::MaterialType::PbrMetallicRoughness)
            {
                auto const& matPbr = mat.as<Magnum::Trade::PbrMetallicRoughnessMaterialData>();
                if (int const baseColor = matPbr.baseColorTexture();
                    baseColor != -1)
                {
                    osp::ResId const texRes = rImportData.m_textures[baseColor];
                    TexId const texId = SysRender::own_texture_resource(rDrawing, rDrawingRes, rResources, texRes);
                    rDrawing.m_diffuseTex[drawEnt] = rDrawing.m_texRefCounts.ref_add(texId);
                    rDrawing.m_diffuseDirty.push_back(drawEnt);
                }
            }

            rDrawing.m_drawBasic[drawEnt] = { .m_opaque = true, .m_transparent = false };
            rDrawing.m_visible.set(std::size_t(drawEnt));

            if (material.has_value())
            {
                material.value().m_ents.set(std::size_t(drawEnt));
                material.value().m_dirty.push_back(drawEnt);
            }
        }

        ++itPfEnts;
    }
}
#endif

void SysPrefabInit::init_physics(
            ACtxPrefabInit const&               rPrefabInit,
            Resources const&                    rResources,
            ACtxPhysics&                        rCtxPhys) noexcept
{

    auto itPfEnts = std::begin(rPrefabInit.m_ents);

    for (TmpPrefabInitBasic const& rPfBasic : rPrefabInit.m_basicIn)
    {
        auto const &rImportData = rResources.data_get<osp::ImporterData const>(
                gc_importer, rPfBasic.m_importerRes);
        auto const &rPrefabData = rResources.data_get<osp::Prefabs const>(
                gc_importer, rPfBasic.m_importerRes);

        auto const ents     = ArrayView<ActiveEnt const>{*itPfEnts};
        auto const objects  = lgrn::Span<int const>     {rPrefabData.m_prefabs[rPfBasic.m_prefabId]};
        auto const parents  = lgrn::Span<int const>     {rPrefabData.m_prefabParents[rPfBasic.m_prefabId]};

#if 0
        auto const assign_dyn_recurse = [&rCtxPhys, ents, objects, parents] (auto const& self, int objectId, ActiveEnt ent) -> void
        {
            int const parentId = parents[objectId];
            if (parentId == -1)
            {
                return;
            }

            ActiveEnt const parentEnt = ents[parentId];

            if ( ! rCtxPhys.m_totalDyn.contains(parentEnt) )
            {
                rCtxPhys.m_totalDyn.emplace(parentEnt);
            }

            self(self, parentId, parentEnt);
        };
#endif

        auto const assign_collider_recurse
                = [&rHasColliders = rCtxPhys.m_hasColliders, ents, objects, parents]
                  (auto const& self, int objectId, ActiveEnt ent) -> void
        {
            if (rHasColliders.test(std::size_t(ent)))
            {
                return; // HasColliders bit already set, this means all ancestors have it set too
            }
            rHasColliders.set(std::size_t(ent));

            int const parentId = parents[objectId];

            if (parentId != -1)
            {
                self(self, parentId, ents[parentId]);
            }
        };

        for (std::size_t i = 0; i < objects.size(); ++i)
        {
            ActiveEnt const ent         = ents[i];
            int const       objectId    = objects[i];
            float const     mass        = rPrefabData.m_objMass[objectId];
            EShape const    shape       = rPrefabData.m_objShape[objectId];


            rCtxPhys.m_shape[ent] = shape;

            if (mass != 0.0f)
            {
                Vector3 const scale = rImportData.m_objTransforms[objectId].scaling();
                Vector3 const inertia = phys::collider_inertia_tensor(shape, scale, mass);
                Vector3 const offset{0.0f, 0.0f, 0.0f};
                rCtxPhys.m_mass.emplace( ent, ACompMass{ offset, inertia, mass } );

                //assign_dyn_recurse(assign_dyn_recurse, objectId, ent);
            }

            if ( (mass != 0.0f) || (shape != EShape::None) )
            {
                assign_collider_recurse(assign_collider_recurse, objectId, ent);
            }
        }

        std::advance(itPfEnts, 1);
    }
}


} // namespace osp::active
