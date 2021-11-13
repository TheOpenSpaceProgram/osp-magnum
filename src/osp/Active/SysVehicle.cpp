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
#include "ActiveScene.h"
#include "SysHierarchy.h"
#include "SysPhysics.h"
#include "SysRender.h"
#include "SysVehicle.h"
#include "physics.h"

#include "../Resource/PackageRegistry.h"
#include "../Resource/PrototypePart.h"
#include "../Resource/AssetImporter.h"

#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Texture.h>

#include <Corrade/Containers/ArrayViewStl.h>

#include <iostream>


using namespace osp;
using namespace osp::active;

#if 0

// Traverses the hierarchy and sums the volume of all ACompShapes it finds
float SysVehicle::compute_hier_volume(ActiveScene& rScene, ActiveEnt part)
{
    float volume = 0.0f;
    auto checkVol = [&rScene, &volume](ActiveEnt ent)
    {
        auto const* shape = rScene.get_registry().try_get<ACompShape>(ent);
        if(nullptr != shape)
        {
            auto const& xform = rScene.reg_get<ACompTransform>(ent);
            volume += shape_volume(shape->m_shape, xform.m_transform.scaling());
        }
        return EHierarchyTraverseStatus::Continue;
    };

    SysHierarchy::traverse(rScene, part, std::move(checkVol));

    return volume;
}

ActiveEnt SysVehicle::part_instantiate(
        ActiveScene& rScene, PrototypePart const& part,
        BlueprintPart const& blueprint, ActiveEnt rootParent)
{
    ActiveReg_t &rReg = rScene.get_registry();

    // TODO: make some changes to ActiveScene for creating multiple entities easier
    std::vector<ActiveEnt> newEntities(part.m_entityCount);
    ActiveEnt& rootEntity = newEntities[0];

    // reserve space for new entities and ACompTransforms to be created
    //rScene.get_registry().reserve(
    //            rScene.get_registry().capacity() + part.m_entityCount);
    rReg.reserve<ACompTransform>(
                rScene.get_registry().capacity<ACompTransform>() + part.m_entityCount);

    // Create entities and hierarchy
    for (size_t i = 0; i < part.m_entityCount; i++)
    {
        PCompHierarchy const &pcompHier = part.m_partHier[i];
        ActiveEnt parentEnt = entt::null;

        // Get parent
        if (pcompHier.m_parent == i)
        {
            // if parented to self (the root)
            parentEnt = rootParent;//m_scene->hier_get_root();
        }
        else
        {
            // since objects were loaded recursively, the parents always load
            // before their children
            parentEnt = newEntities[pcompHier.m_parent];
        }

        // Create the new entity
        ActiveEnt currentEnt = SysHierarchy::create_child(rScene, parentEnt);
        newEntities[i] = currentEnt;

        // Add and set transform component
        PCompTransform const &pcompTransform = part.m_partTransform[i];
        auto &rCurrentTransform = rScene.reg_emplace<ACompTransform>(currentEnt);
        rCurrentTransform.m_transform
                = Matrix4::from(pcompTransform.m_rotation.toMatrix(),
                                pcompTransform.m_translation)
                * Matrix4::scaling(pcompTransform.m_scale);
        rScene.reg_emplace<ACompDrawTransform>(currentEnt);
    }

    for (PCompName const& pcompName : part.m_partName)
    {
        rScene.reg_emplace<ACompName>(newEntities[pcompName.m_entity], pcompName.m_name);
    }

    auto &rGroups = rReg.ctx<ACtxRenderGroups>();

    // Create drawables
    for (PCompDrawable const& pcompDrawable : part.m_partDrawable)
    {
        using Magnum::GL::Mesh;
        using Magnum::Trade::MeshData;
        using Magnum::GL::Texture2D;
        using Magnum::Trade::ImageData2D;

        Package& glResources = rScene.get_context_resources();
        DependRes<MeshData> meshData = pcompDrawable.m_mesh;
        DependRes<Mesh> meshRes = glResources.get<Mesh>(meshData.name());

        if (meshRes.empty())
        {
            // Mesh isn't compiled yet, compile it
            meshRes = AssetImporter::compile_mesh(meshData, glResources);
        }

        std::vector<DependRes<Texture2D>> textureResources;
        textureResources.reserve(pcompDrawable.m_textures.size());
        for (DependRes<ImageData2D> const& imageData : pcompDrawable.m_textures)
        {
            DependRes<Texture2D> texRes = glResources.get<Texture2D>(imageData.name());

            if (texRes.empty())
            {
                // Texture isn't compiled yet, compile it
                texRes = AssetImporter::compile_tex(imageData, glResources);
            }
            textureResources.push_back(texRes);
        }

        // by now, the mesh and texture should both exist

        ActiveEnt currentEnt = newEntities[pcompDrawable.m_entity];

        // by now, the mesh and texture should both exist so we emplace them
        // as components to be consumed by the Phong shader
        rScene.reg_emplace<ACompVisible>(currentEnt);
        rScene.reg_emplace<ACompOpaque>(currentEnt);
        rScene.reg_emplace<ACompMesh>(currentEnt, meshRes);
        rScene.reg_emplace<ACompDiffuseTex>(currentEnt, std::move(textureResources[0]));
        rGroups.add<MaterialCommon>(currentEnt);
    }

    rReg.reserve<PCompPrimativeCollider>(
                rScene.get_registry().capacity<PCompPrimativeCollider>()
                + part.m_partCollider.size());

    // Create primative colliders
    for (PCompPrimativeCollider const& pcompCollider : part.m_partCollider)
    {
        ActiveEnt currentEnt = newEntities[pcompCollider.m_entity];
        ACompShape& collision = rScene.reg_emplace<ACompShape>(currentEnt);
        rScene.reg_emplace<ACompSolidCollider>(currentEnt);
        collision.m_shape = pcompCollider.m_shape;
    }

    // TODO: individual glTF nodes can now have masses, but there's no
    //       implementation for it yet. This is a workaround to keep the old
    //       system

    // Create masses
    float totalMass = 0;
    for (PCompMass const& pcompMass : part.m_partMass)
    {
        totalMass += pcompMass.m_mass;
    }

    float partVolume = compute_hier_volume(rScene, rootEntity);
    float partDensity = totalMass / partVolume;

    auto applyMasses = [&rScene, partDensity](ActiveEnt ent)
    {
        if (auto const* shape = rScene.reg_try_get<ACompShape>(ent);
            shape != nullptr)
        {
            if (!rScene.get_registry().all_of<ACompSolidCollider>(ent))
            {
                return EHierarchyTraverseStatus::Continue;
            }
            Matrix4 const& transform = rScene.reg_get<ACompTransform>(ent).m_transform;
            float volume = phys::shape_volume(shape->m_shape, transform.scaling());
            float mass = volume * partDensity;

            rScene.reg_emplace<ACompMass>(ent, mass);
        }
        return EHierarchyTraverseStatus::Continue;
    };

    SysHierarchy::traverse(rScene, rootEntity, applyMasses);

    // Initialize entities for individual machines. This is done now in one
    // place, as creating new entities can be problematic for concurrency

    auto &machines = rScene.reg_emplace<ACompMachines>(rootEntity);
    machines.m_machines.reserve(part.m_protoMachines.size());

    for (PCompMachine const& pcompMachine : part.m_protoMachines)
    {
        ActiveEnt parent = newEntities[pcompMachine.m_entity];
        ActiveEnt machEnt = machines.m_machines.emplace_back(
                    SysHierarchy::create_child(rScene, parent, "Machine"));
        //rScene.reg_emplace<ACompMachineType>(machEnt, pcompMachine.m_type);
    }

    return rootEntity;
}

void SysVehicle::update_vehicle_modification(
        ACtxVehicle& rCtx,
        ACtxPhysics& rPhys,
        acomp_view_t<ACompTransform> viewTf,
        acomp_view_t<ACompHierarchy> viewHier,
        acomp_view_t<ACompRigidbodyAncestor> viewRBA)
{
    // Clear In-construction queue
    rCtx.m_vehicleInConstruction.clear();

    auto viewVehicles   = entt::basic_view{rCtx.m_vehicle};
    auto viewParts      = entt::basic_view{rCtx.m_part};

    // this part is sort of temporary and unoptimized. deal with it when it
    // becomes a problem. TODO: use more views

    for (ActiveEnt vehicleEnt : viewVehicles)
    {
        auto &vehicleVehicle = viewVehicles.get<ACompVehicle>(vehicleEnt);

        if (vehicleVehicle.m_separationCount > 0)
        {
            // Separation requested

            // mark collider as dirty
            rPhys.m_colliderDirty.push_back(vehicleEnt);

            // Invalidate all ACompRigidbodyAncestors
            auto invalidateAncestors = [&viewRBA](ActiveEnt ent)
            {
                if (viewRBA.contains(ent))
                {
                    viewRBA.get<ACompRigidbodyAncestor>(ent).m_ancestor = entt::null;
                }

                return EHierarchyTraverseStatus::Continue;
            };
            SysHierarchy::traverse(viewHier, vehicleEnt, invalidateAncestors);

            // Create the islands vector
            // [0]: current vehicle
            // [1+]: new vehicles
            std::vector<ActiveEnt> islands(vehicleVehicle.m_separationCount);
            vehicleVehicle.m_separationCount = 0;

            islands[0] = vehicleEnt;

            Vector3 const velocity = rPhys.m_physLinearVel.get(vehicleEnt);

            // Loop but skip first element, already set to vehicleEnt
            for (ActiveEnt& rIslands
                 : Corrade::Containers::arrayView(islands).suffix(1))
            {
                auto const &vehicleTransform = viewTf.get<ACompTransform>(vehicleEnt);

                ActiveEnt const islandEnt = SysHierarchy::create_child(
                            rScene, rScene.hier_get_root());

                // NOTE: vehicleVehicle and vehicleTransform can become invalid
                //       when emplacing new components of the same type
                rScene.reg_emplace<ACompVehicle>(islandEnt);
                rScene.reg_emplace<ACompTransform>(
                        islandEnt, ACompTransform{vehicleTransform.m_transform});

                // temporary:
                // Make the whole thing a single rigid body. Eventually have a
                // 'vehicle structure system' do this
                // Note that there may be future 'structure' implementations
                // that have multiple rigid bodies per vehicle.
                rScene.reg_emplace<ACompDrawTransform>(islandEnt);
                rScene.reg_emplace<ACompPhysBody>(islandEnt);
                rScene.reg_emplace<ACompPhysDynamic>(islandEnt);
                rScene.reg_emplace<ACompPhysLinearVel>(islandEnt);
                rScene.reg_emplace<ACompPhysAngularVel>(islandEnt);
                rScene.reg_emplace<ACompShape>(islandEnt, ACompShape{phys::EShape::Combined});
                rScene.reg_emplace<ACompSolidCollider>(islandEnt);
                rPhys.m_setVelocity.emplace_back(islandEnt, velocity);

                rIslands = islandEnt;
            }


            // iterate through parts
            // * remove parts that are destroyed, destroy the part entity too
            // * remove parts different islands, and move them to the new
            //   vehicle

            auto removeDestroyed = [&viewParts, &rScene, &islands]
                    (ActiveEnt partEnt) -> bool
            {
                auto &rPart = viewParts.get<ACompPart>(partEnt);
                if (rPart.m_destroy)
                {
                    // destroy this part
                    SysHierarchy::mark_delete_cut(rScene, partEnt);
                    return true;
                }

                if (0 != rPart.m_separationIsland)
                {
                    // Separate into an island vehicle created previously

                    ActiveEnt const islandEnt = islands[rPart.m_separationIsland];
                    auto &rIslandVehicle = rScene.reg_get<ACompVehicle>(
                                islandEnt);
                    rIslandVehicle.m_parts.push_back(partEnt);

                    SysHierarchy::set_parent_child(rScene, islandEnt, partEnt);

                    return true;
                }

                return false;

            };

            std::vector<ActiveEnt> &rParts
                    = view.get<ACompVehicle>(vehicleEnt).m_parts;

            rParts.erase(std::remove_if(rParts.begin(), rParts.end(),
                                        removeDestroyed), rParts.end());

        }
    }
}

#endif

