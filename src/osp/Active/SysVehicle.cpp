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

#include "../Resource/PrototypePart.h"
#include "../Resource/AssetImporter.h"

#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Texture.h>

#include <iostream>


using namespace osp;
using namespace osp::active;

// Traverses the hierarchy and sums the volume of all ACompShapes it finds
float SysVehicle::compute_hier_volume(ActiveScene& rScene, ActiveEnt part)
{
    float volume = 0.0f;
    auto checkVol = [&rScene, &volume](ActiveEnt ent)
    {
        auto const* shape = rScene.get_registry().try_get<ACompShape>(ent);
        if (shape)
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
    // TODO: make some changes to ActiveScene for creating multiple entities easier

    std::vector<ActiveEnt> newEntities(part.m_entityCount);
    ActiveEnt& rootEntity = newEntities[0];

    // reserve space for new entities and ACompTransforms to be created
    //rScene.get_registry().reserve(
    //            rScene.get_registry().capacity() + part.m_entityCount);
    rScene.get_registry().reserve<ACompTransform>(
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

    // Create drawables
    for (PCompDrawable const& pcompDrawable : part.m_partDrawable)
    {
        using Magnum::GL::Mesh;
        using Magnum::Trade::MeshData;
        using Magnum::GL::Texture2D;
        using Magnum::Trade::ImageData2D;

        Package& package = rScene.get_application().debug_find_package("lzdb");

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
        for (DependRes<ImageData2D> imageData : pcompDrawable.m_textures)
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
        rScene.reg_emplace<ACompShader>(currentEnt, SysRender::get_default_shader());
        rScene.reg_emplace<ACompMesh>(currentEnt, meshRes);
        rScene.reg_emplace<ACompDiffuseTex>(currentEnt, std::move(textureResources[0]));
    }

    rScene.get_registry().reserve<PCompPrimativeCollider>(
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
        rScene.reg_emplace<ACompMachineType>(machEnt, pcompMachine.m_type);
    }

    return rootEntity;
}

void debug_wire_vehicles(ActiveScene &rScene)
{
    auto view = rScene.get_registry()
            .view<osp::active::ACompVehicle,
                  osp::active::ACompVehicleInConstruction>();


}

void SysVehicle::update_vehicle_modification(ActiveScene& rScene)
{
    // Clear In-construction queue
    debug_wire_vehicles(rScene); // but wire the machines first because why not
    rScene.get_registry().clear<ACompVehicleInConstruction>();

    auto view = rScene.get_registry().view<ACompVehicle>();
    auto viewParts = rScene.get_registry().view<ACompPart>();

    // this part is sort of temporary and unoptimized. deal with it when it
    // becomes a problem. TODO: use more views

    for (ActiveEnt vehicleEnt : view)
    {
        auto &vehicleVehicle = view.get<ACompVehicle>(vehicleEnt);
        //std::vector<ActiveEnt> &parts = vehicleVehicle.m_parts;

        if (vehicleVehicle.m_separationCount > 0)
        {
            // Separation requested

            // mark collider as dirty
            auto &rVehicleBody = rScene.reg_get<ACompRigidBody>(vehicleEnt);
            rVehicleBody.m_colliderDirty = true;

            // Invalidate all ACompRigidbodyAncestors
            auto invalidateAncestors = [&rScene](ActiveEnt e)
            {
                auto* pRBA = rScene.get_registry().try_get<ACompRigidbodyAncestor>(e);
                if (pRBA) { pRBA->m_ancestor = entt::null; }
                return EHierarchyTraverseStatus::Continue;
            };
            SysHierarchy::traverse(rScene, vehicleEnt, invalidateAncestors);

            // Create the islands vector
            // [0]: current vehicle
            // [1+]: new vehicles
            std::vector<ActiveEnt> islands(vehicleVehicle.m_separationCount);
            vehicleVehicle.m_separationCount = 0;

            islands[0] = vehicleEnt;

            // NOTE: vehicleVehicle and vehicleTransform become invalid
            //       when emplacing new ones.
            for (size_t i = 1; i < islands.size(); i ++)
            {
                ActiveEnt islandEnt = SysHierarchy::create_child(
                            rScene, rScene.hier_get_root());
                rScene.reg_emplace<ACompVehicle>(islandEnt);
                auto &islandTransform
                        = rScene.reg_emplace<ACompTransform>(islandEnt);
                rScene.reg_emplace<ACompDrawTransform>(islandEnt);
                auto &islandBody
                        = rScene.reg_emplace<ACompRigidBody>(islandEnt);
                auto &islandShape
                        = rScene.reg_emplace<ACompShape>(islandEnt);
                rScene.reg_emplace<ACompSolidCollider>(islandEnt);
                islandShape.m_shape = phys::ECollisionShape::COMBINED;

                auto &vehicleTransform = rScene.reg_get<ACompTransform>(vehicleEnt);
                islandTransform.m_transform = vehicleTransform.m_transform;
                rScene.reg_emplace<ACompFloatingOrigin>(islandEnt);

                islands[i] = islandEnt;
            }


            // iterate through parts
            // * remove parts that are destroyed, destroy the part entity too
            // * remove parts different islands, and move them to the new
            //   vehicle


            entt::basic_registry<ActiveEnt> &reg = rScene.get_registry();
            auto removeDestroyed = [&viewParts, &rScene, &islands]
                    (ActiveEnt partEnt) -> bool
            {
                auto &partPart = viewParts.get<ACompPart>(partEnt);
                if (partPart.m_destroy)
                {
                    // destroy this part
                    SysHierarchy::mark_delete_cut(rScene, partEnt);
                    return true;
                }

                if (partPart.m_separationIsland)
                {
                    // separate into a new vehicle

                    ActiveEnt islandEnt = islands[partPart.m_separationIsland];
                    auto &islandVehicle = rScene.reg_get<ACompVehicle>(
                                islandEnt);
                    islandVehicle.m_parts.push_back(partEnt);

                    SysHierarchy::set_parent_child(rScene, islandEnt, partEnt);

                    return true;
                }

                return false;

            };

            std::vector<ActiveEnt> &parts
                    = view.get<ACompVehicle>(vehicleEnt).m_parts;

            parts.erase(std::remove_if(parts.begin(), parts.end(),
                                       removeDestroyed), parts.end());

            // update or create rigid bodies. also set center of masses

            for (ActiveEnt islandEnt : islands)
            {
                auto &islandVehicle = view.get<ACompVehicle>(islandEnt);
                auto &islandTransform
                        = rScene.reg_get<ACompTransform>(islandEnt);

                Vector3 comOffset;
                //float totalMass;

                for (ActiveEnt partEnt : islandVehicle.m_parts)
                {
                    // TODO: deal with part mass
                    auto const &partTransform
                            = rScene.reg_get<ACompTransform>(partEnt);

                    comOffset += partTransform.m_transform.translation();

                }

                comOffset /= islandVehicle.m_parts.size();



                for (ActiveEnt partEnt : islandVehicle.m_parts)
                {
                    ActiveEnt child = rScene.reg_get<ACompHierarchy>(partEnt)
                                        .m_childFirst;
                    auto &partTransform
                            = rScene.reg_get<ACompTransform>(partEnt);

                    partTransform.m_transform.translation() -= comOffset;
                }

//                ActiveEnt child = m_scene.reg_get<ACompHierarchy>(islandEnt)
//                                    .m_childFirst;
//                while (m_scene.get_registry().valid(child))
//                {
//                    auto &childTransform
//                            = m_scene.reg_get<ACompTransform>(child);

//                    childTransform.m_transform.translation() -= comOffset;

//                    child = m_scene.reg_get<ACompHierarchy>(child)
//                            .m_siblingNext;
//                }

                islandTransform.m_transform.translation() += comOffset;

                //m_scene.dynamic_system_find<SysPhysics>().create_body(islandEnt);
            }

        }
    }
}

