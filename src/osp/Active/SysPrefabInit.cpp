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

void SysPrefabInit::init_subtrees(
        ACtxPrefabInit const&               rPrefabInit,
        Resources const&                    rResources,
        ACtxSceneGraph&                     rScnGraph) noexcept
{
    auto itPfEnts = std::begin(rPrefabInit.m_ents);

    /*
    for (TmpPrefabInitBasic const& rPfBasic : rPrefabInit.m_basic)
    {
        auto const &rPrefabData = rResources.data_get<osp::Prefabs const>(
                gc_importer, rPfBasic.m_importerRes);

        std::size_t const objCount = rPrefabData.m_prefabs[rPfBasic.m_prefabId].size();
        auto const parents = rPrefabData.m_prefabParents[rPfBasic.m_prefabId];

        SubtreeBuilder builder = SysSceneGraph::create_subtree(rScnGraph, rPfBasic.m_parent, );

        for (std::size_t i = 0; i < objCount; ++i)
        {
            ObjId const prefabParent = parents[i];
            ActiveEnt const parent = (prefabParent == -1)
                    ? rPfBasic.m_parent
                    : (*itPfEnts)[prefabParent];

            ActiveEnt const child = (*itPfEnts)[i];

            SysHierarchy::add_child(rHier, parent, child);
        }

        std::advance(itPfEnts, 1);
    }
    */
}

void SysPrefabInit::init_transforms(
        ACtxPrefabInit const&               rPrefabInit,
        Resources const&                    rResources,
        acomp_storage_t<ACompTransform>&    rTransform) noexcept
{
    auto itPfEnts = std::begin(rPrefabInit.m_ents);

    for (TmpPrefabInitBasic const& rPfBasic : rPrefabInit.m_basic)
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
            rTransform.emplace((*itPfEnts)[i], transform);
        }

        std::advance(itPfEnts, 1);
    }
}

void SysPrefabInit::init_drawing(
        ACtxPrefabInit const&               rPrefabInit,
        Resources&                          rResources,
        ACtxDrawing&                        rCtxDraw,
        ACtxDrawingRes&                     rCtxDrawRes,
        std::optional<EntSetPair>           material) noexcept
{
    auto itPfEnts = std::begin(rPrefabInit.m_ents);

    for (TmpPrefabInitBasic const& rPfBasic : rPrefabInit.m_basic)
    {
        auto const &rImportData = rResources.data_get<osp::ImporterData const>(
                gc_importer, rPfBasic.m_importerRes);
        auto const &rPrefabData = rResources.data_get<osp::Prefabs const>(
                gc_importer, rPfBasic.m_importerRes);

        auto const objects = rPrefabData.m_prefabs[rPfBasic.m_prefabId];

        for (std::size_t i = 0; i < objects.size(); ++i)
        {
            // Check if object has mesh
            int const meshImportId = rImportData.m_objMeshes[objects[i]];
            if (meshImportId == -1)
            {
                continue;
            }

            ActiveEnt const ent = (*itPfEnts)[i];

            osp::ResId const meshRes = rImportData.m_meshes[meshImportId];
            MeshId const meshId = SysRender::own_mesh_resource(rCtxDraw, rCtxDrawRes, rResources, meshRes);
            rCtxDraw.m_mesh.emplace(ent, rCtxDraw.m_meshRefCounts.ref_add(meshId));
            rCtxDraw.m_meshDirty.push_back(ent);

            int const matImportId = rImportData.m_objMaterials[objects[i]];

            if (Magnum::Trade::MaterialData const &mat = *rImportData.m_materials.at(matImportId);
                mat.types() & Magnum::Trade::MaterialType::PbrMetallicRoughness)
            {
                auto const& matPbr = mat.as<Magnum::Trade::PbrMetallicRoughnessMaterialData>();
                if (int const baseColor = matPbr.baseColorTexture();
                    baseColor != -1)
                {
                    osp::ResId const texRes = rImportData.m_textures[baseColor];
                    TexId const texId = SysRender::own_texture_resource(rCtxDraw, rCtxDrawRes, rResources, texRes);
                    rCtxDraw.m_diffuseTex.emplace(ent, rCtxDraw.m_texRefCounts.ref_add(texId));
                    rCtxDraw.m_diffuseDirty.push_back(ent);
                }

            }

            rCtxDraw.m_opaque.emplace(ent);
            rCtxDraw.m_visible.emplace(ent);

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
            ACtxPhysInputs&                     rPhysIn,
            ACtxPhysics&                        rCtxPhys,
            ACtxHierBody&                       rCtxHierBody) noexcept
{
    auto itPfEnts = std::begin(rPrefabInit.m_ents);

    for (TmpPrefabInitBasic const& rPfBasic : rPrefabInit.m_basic)
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
            rCtxPhys.m_hasColliders.emplace(ent);

            EShape const shape = rPrefabData.m_objShape[objects[i]];

            if (shape != EShape::None)
            {
                rCtxPhys.m_shape.emplace(ent, shape);
                rCtxPhys.m_solid.emplace(ent);
                rPhysIn.m_colliderDirty.push_back(ent);
            }

            if (float const mass = rPrefabData.m_objMass[objects[i]];
               mass != 0.0f)
            {
                osp::Vector3 const inertia = osp::phys::collider_inertia_tensor(
                        shape != EShape::None ? shape : EShape::Sphere,
                        rImportData.m_objTransforms[objects[i]].scaling(),
                        mass);
                rCtxHierBody.m_ownDyn.emplace( ent, ACompSubBody{ inertia, mass } );
            }
        }

        std::advance(itPfEnts, 1);
    }
}

} // namespace osp::active
