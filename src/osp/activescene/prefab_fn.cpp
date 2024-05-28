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
#include "prefab_fn.h"
#include "basic_fn.h"

#include "../core/Resources.h"
#include "../vehicles/ImporterData.h"

#include <Magnum/Trade/Trade.h>
#include <Magnum/Trade/PbrMetallicRoughnessMaterialData.h>

using Corrade::Containers::ArrayView;

using osp::restypes::gc_importer;

namespace osp::active
{

void SysPrefabInit::create_activeents(
        ACtxPrefabs&                        rPrefabs,
        ACtxBasic&                          rBasic,
        Resources&                          rResources)
{
    // Count number of entities needed to be created
    std::size_t totalEnts = 0;
    for (TmpPrefabRequest const& rPfBasic : rPrefabs.spawnRequest)
    {
        auto const& rPrefabData = rResources.data_get<Prefabs>(gc_importer, rPfBasic.m_importerRes);
        auto const& objects     = rPrefabData.m_prefabs[rPfBasic.m_prefabId];

        totalEnts += objects.size();
    }

    // Create entities
    rPrefabs.newEnts.resize(totalEnts);
    rBasic.m_activeIds.create(std::begin(rPrefabs.newEnts), std::end(rPrefabs.newEnts));

    // Assign new entities to each prefab to create
    rPrefabs.spawnedEntsOffset.resize(rPrefabs.spawnRequest.size());
    auto itEntAvailable = std::begin(rPrefabs.newEnts);
    auto itPfEntSpanOut = std::begin(rPrefabs.spawnedEntsOffset);
    for (TmpPrefabRequest& rPfBasic : rPrefabs.spawnRequest)
    {
        auto const& rPrefabData = rResources.data_get<Prefabs>(gc_importer, rPfBasic.m_importerRes);
        auto const& objects     = rPrefabData.m_prefabs[rPfBasic.m_prefabId];

        (*itPfEntSpanOut) = { &(*itEntAvailable), objects.size() };

        std::advance(itEntAvailable, objects.size());
        std::advance(itPfEntSpanOut, 1);
    }

    assert(itEntAvailable == std::end(rPrefabs.newEnts));
}

void SysPrefabInit::add_to_subtree(
        TmpPrefabRequest const&             basic,
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
        ACtxPrefabs const&                  rPrefabs,
        Resources const&                    rResources,
        ACompTransformStorage_t&            rTransform) noexcept
{
    auto itPfEnts = rPrefabs.spawnedEntsOffset.begin();

    for (TmpPrefabRequest const& rPfBasic : rPrefabs.spawnRequest)
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

        ++itPfEnts;
    }
}

void SysPrefabInit::init_info(
            ACtxPrefabs&                    rPrefabs,
            Resources const&                rResources) noexcept
{
    auto itPfEnts = rPrefabs.spawnedEntsOffset.begin();

    for (TmpPrefabRequest const& rPfBasic : rPrefabs.spawnRequest)
    {
        auto const &rImportData = rResources.data_get<osp::ImporterData const>(
                gc_importer, rPfBasic.m_importerRes);
        auto const &rPrefabData = rResources.data_get<osp::Prefabs const>(
                gc_importer, rPfBasic.m_importerRes);

        auto const ents     = ArrayView<ActiveEnt const>{*itPfEnts};
        auto const objects = rPrefabData.m_prefabs[rPfBasic.m_prefabId];
        auto const parents = rPrefabData.m_prefabParents[rPfBasic.m_prefabId];

        for (std::size_t i = 0; i < objects.size(); ++i)
        {
            ActiveEnt const ent = ents[i];
            PrefabInstanceInfo &rInfo = rPrefabs.instanceInfo[ent];
            rInfo.importer  = rPfBasic.m_importerRes;
            rInfo.prefab    = rPfBasic.m_prefabId;
            rInfo.obj       = static_cast<ObjId>(i);

            if (parents[i] == -1)
            {
                rPrefabs.roots.insert(ent);
            }
        }

        ++itPfEnts;
    }
}

void SysPrefabInit::init_physics(
            ACtxPrefabs const&              rPrefabs,
            Resources const&                rResources,
            ACtxPhysics&                    rCtxPhys) noexcept
{

    auto itPfEnts = rPrefabs.spawnedEntsOffset.begin();

    for (TmpPrefabRequest const& rPfBasic : rPrefabs.spawnRequest)
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
            if (rHasColliders.contains(ent))
            {
                return; // HasColliders bit already set, this means all ancestors have it set too
            }
            rHasColliders.insert(ent);

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
                Vector3 const inertia = collider_inertia_tensor(shape, scale, mass);
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
