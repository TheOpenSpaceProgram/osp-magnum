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

    auto const objs     = rPrefabData.m_prefabs[basic.m_prefabId];
    auto const parents  = rPrefabData.m_prefabParents[basic.m_prefabId];

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

void SysPrefabInit::init_drawing(
        ACtxPrefabInit const&               rPrefabInit,
        Resources&                          rResources,
        ACtxDrawing&                        rDrawing,
        ACtxDrawingRes&                     rDrawingRes,
        std::optional<EntSetPair>           material) noexcept
{
    auto itPfEnts = std::begin(rPrefabInit.m_ents);

    for (TmpPrefabInitBasic const& rPfBasic : rPrefabInit.m_basicIn)
    {
        auto const &rImportData = rResources.data_get<osp::ImporterData const>(
                gc_importer, rPfBasic.m_importerRes);
        auto const &rPrefabData = rResources.data_get<osp::Prefabs const>(
                gc_importer, rPfBasic.m_importerRes);

        auto const objects = rPrefabData.m_prefabs[rPfBasic.m_prefabId];

        for (std::size_t i = 0; i < objects.size(); ++i)
        {
            ActiveEnt const ent = (*itPfEnts)[i];

            // TODO: Don't actually do this. This marks every entity as drawable,
            //       which considers them for draw transformations.
            //       Only set drawable for entities that have a mesh or is an
            //       ancestor of an entity with a mesh.
            rDrawing.m_drawable.set(std::size_t(ent));

            // Check if object has mesh
            int const meshImportId = rImportData.m_objMeshes[objects[i]];
            if (meshImportId == -1)
            {
                continue;
            }

            osp::ResId const meshRes = rImportData.m_meshes[meshImportId];
            MeshId const meshId = SysRender::own_mesh_resource(rDrawing, rDrawingRes, rResources, meshRes);
            rDrawing.m_mesh.emplace(ent, rDrawing.m_meshRefCounts.ref_add(meshId));
            rDrawing.m_meshDirty.push_back(ent);

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
                    rDrawing.m_diffuseTex.emplace(ent, rDrawing.m_texRefCounts.ref_add(texId));
                    rDrawing.m_diffuseDirty.push_back(ent);
                }

            }

            rDrawing.m_opaque.emplace(ent);
            rDrawing.m_visible.emplace(ent);

            if (material.has_value())
            {
                material.value().m_rEnts.set(std::size_t(ent));
                material.value().m_rDirty.push_back(ent);
            }
        }

        std::advance(itPfEnts, 1);
    }
}

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

        auto const objects = rPrefabData.m_prefabs[rPfBasic.m_prefabId];

        for (std::size_t i = 0; i < objects.size(); ++i)
        {
            ActiveEnt const ent = (*itPfEnts)[i];

            // TODO: hasColliders should be set for each entity that has
            //       colliders, or descendents have colliders.
            //       For now, all entities are set.
            rCtxPhys.m_hasColliders.set(std::size_t(ent));

            EShape const shape = rPrefabData.m_objShape[objects[i]];

            rCtxPhys.m_shape[std::size_t(ent)] = shape;
            if (shape != EShape::None)
            {
                //rCtxPhys.m_solid.set(std::size_t(ent));
                if (float const mass = rPrefabData.m_objMass[objects[i]];
                   mass != 0.0f)
                {
                    Vector3 const scale = rImportData.m_objTransforms[objects[i]].scaling();
                    Vector3 const inertia = phys::collider_inertia_tensor(shape, scale, mass);
                    Vector3 const offset{0.0f, 0.0f, 0.0f};
                    rCtxPhys.m_ownDyn.emplace( ent, ACompSubBody{ inertia, offset, mass } );
                }
            }
        }

        std::advance(itPfEnts, 1);
    }
}

} // namespace osp::active
