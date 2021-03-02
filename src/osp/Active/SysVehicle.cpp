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
#include "SysVehicle.h"
#include "SysDebugRender.h"
#include "physics.h"

#include "../Satellites/SatActiveArea.h"
#include "../Satellites/SatVehicle.h"
#include "../Resource/PrototypePart.h"
#include "../Resource/AssetImporter.h"
#include "adera/Shaders/Phong.h"

#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Texture.h>

#include <iostream>


using namespace osp;
using namespace osp::active;

using osp::universe::Universe;
using osp::universe::Satellite;
using osp::universe::TypeSatIndex;

// for the 0xrrggbb_rgbf literalsm
using namespace Magnum::Math::Literals;

const std::string SysVehicle::smc_name = "Vehicle";

SysVehicle::SysVehicle(ActiveScene &scene)
 : m_updateActivation(
       scene.get_update_order(), "vehicle_activate", "", "vehicle_modification",
       &SysVehicle::update_activate)
 , m_updateVehicleModification(
       scene.get_update_order(), "vehicle_modification", "", "physics",
       &SysVehicle::update_vehicle_modification)
{ }


ActiveEnt SysVehicle::activate(ActiveScene &rScene, universe::Universe &rUni,
                          universe::Satellite areaSat,
                          universe::Satellite tgtSat)
{

    std::cout << "loadin a vehicle!\n";

    auto &loadMeVehicle = rUni.get_reg().get<universe::UCompVehicle>(tgtSat);
    auto &tgtPosTraj = rUni.get_reg().get<universe::UCompTransformTraj>(tgtSat);

    // make sure there is vehicle data to load
    if (loadMeVehicle.m_blueprint.empty())
    {
        // no vehicle data to load
        return entt::null;
    }

    BlueprintVehicle &vehicleData = *(loadMeVehicle.m_blueprint);
    ActiveEnt root = rScene.hier_get_root();

    // Create the root entity to add parts to

    ActiveEnt vehicleEnt = rScene.hier_create_child(root, "Vehicle");

    rScene.reg_emplace<ACompActivatedSat>(vehicleEnt, tgtSat);

    ACompVehicle& vehicleComp = rScene.reg_emplace<ACompVehicle>(vehicleEnt);

    // Convert position of the satellite to position in scene
    Vector3 positionInScene = rUni.sat_calc_pos_meters(areaSat, tgtSat);

    ACompTransform& vehicleTransform = rScene.get_registry()
                                        .emplace<ACompTransform>(vehicleEnt);
    vehicleTransform.m_transform
            = Matrix4::from(tgtPosTraj.m_rotation.toMatrix(), positionInScene);
    rScene.reg_emplace<ACompFloatingOrigin>(vehicleEnt);
    //vehicleTransform.m_enableFloatingOrigin = true;

    // Create the parts

    // Unique part prototypes used in the vehicle
    // Access with [blueprintParts.m_partIndex]
    std::vector<DependRes<PrototypePart> >& partsUsed =
            vehicleData.get_prototypes();

    // All the parts in the vehicle
    std::vector<BlueprintPart> &blueprintParts = vehicleData.get_blueprints();

    // Keep track of parts
    //std::vector<ActiveEnt> newEntities;
    vehicleComp.m_parts.reserve(blueprintParts.size());

    // Loop through list of blueprint parts
    for (BlueprintPart& partBp : blueprintParts)
    {
        DependRes<PrototypePart>& partDepends =
                partsUsed[partBp.m_partIndex];

        // Check if the part prototype this depends on still exists
        if (partDepends.empty())
        {
            return entt::null;
        }

        PrototypePart &proto = *partDepends;

        auto const& [partEntity, machineMapping] = part_instantiate(rScene, proto,
                        partBp, vehicleEnt);

        vehicleComp.m_parts.push_back(partEntity);

        auto& partPart = rScene.reg_emplace<ACompPart>(partEntity);
        partPart.m_vehicle = vehicleEnt;

        // Part now exists

        /* Deferred machine instantiation
        * 
        * Since machines may depend on the existence of other objects in the
        * part's hierarchy, machine instantiation must be deferred until the
        * entire hierarchy exists. For instance, a rocket engine's root
        * "MachineRocket" machine may depend on the existence of a child node
        * to receive an "ACompExhaustPlume" component that handles graphical
        * effects related to the rocket machine, but that child node would not
        * exist in time for the MachineRocket to be instantiated alongside
        * the root part entity.
        */
        part_instantiate_machines(rScene, partEntity, machineMapping, proto, partBp);

        //std::cout << "empty? " << partMachines.m_machines.isEmpty() << "\n";

        //std::cout << "entity: " << int(partEntity) << "\n";

        ACompTransform& partTransform = rScene.get_registry()
                                            .get<ACompTransform>(partEntity);

        // set the transformation
        partTransform.m_transform
                = Matrix4::from(partBp.m_rotation.toMatrix(),
                              partBp.m_translation)
                * Matrix4::scaling(partBp.m_scale);

        // temporary: initialize the rigid body
        //area.get_scene()->debug_get_newton().create_body(partEntity);
    }

    // Wire the thing up

    SysWire& sysWire = rScene.dynamic_system_find<SysWire>();

    // Loop through wire connections
    for (BlueprintWire& blueprintWire : vehicleData.get_wires())
    {
        // TODO: check if the connections are valid

        // get wire from

        ACompMachines& fromMachines = rScene.reg_get<ACompMachines>(
                *(vehicleComp.m_parts.begin() + blueprintWire.m_fromPart));

        auto fromMachineEntry = fromMachines
                .m_machines[blueprintWire.m_fromMachine];
        Machine &fromMachine = fromMachineEntry.m_system->second
                ->get(fromMachineEntry.m_partEnt);
        WireOutput* fromWire =
                fromMachine.request_output(blueprintWire.m_fromPort);

        // get wire to

        ACompMachines& toMachines = rScene.reg_get<ACompMachines>(
                *(vehicleComp.m_parts.begin() + blueprintWire.m_toPart));

        auto toMachineEntry = toMachines
                .m_machines[blueprintWire.m_toMachine];
        Machine &toMachine = toMachineEntry.m_system->second
                ->get(toMachineEntry.m_partEnt);
        WireInput* toWire =
                toMachine.request_input(blueprintWire.m_toPort);

        // make the connection

        sysWire.connect(*fromWire, *toWire);
    }

    // temporary: make the whole thing a single rigid body
    auto& vehicleBody = rScene.reg_emplace<ACompRigidBody_t>(vehicleEnt);
    rScene.reg_emplace<ACompCollisionShape>(vehicleEnt,
        nullptr, phys::ECollisionShape::COMBINED);
    //scene.dynamic_system_find<SysPhysics>().create_body(vehicleEnt);

    return vehicleEnt;
}


void SysVehicle::add_machines_to_object(ActiveScene& rScene,
    ActiveEnt partEnt, ActiveEnt objEnt,
    std::vector<PrototypeMachine> const& protoMachines,
    std::vector<BlueprintMachine> const& blueprintMachines,
    std::vector<unsigned> const& machineIndices)
{
    if (machineIndices.empty()) { return; }

    auto &compMachines =
        rScene.get_registry().get_or_emplace<ACompMachines>(partEnt);

    for (unsigned i : machineIndices)
    {
        BlueprintMachine const& bpMachine = blueprintMachines[i];
        PrototypeMachine const& protoMachine = protoMachines[i];

        MapSysMachine_t::iterator sysMachine
            = rScene.system_machine_find(protoMachine.m_type);

        if (!(rScene.system_machine_it_valid(sysMachine)))
        {
            std::cout << "Machine type: " << protoMachine.m_type << " Not found\n";
            continue;
        }

        // Instantiate the machine for the object
        sysMachine->second->instantiate(objEnt, protoMachine, bpMachine);

        // Add a PartMachine definition to the root part's ACompMachines
        compMachines.m_machines.emplace_back(objEnt, sysMachine);
    }
}

void SysVehicle::part_instantiate_machines(ActiveScene& rScene, ActiveEnt partEnt,
    std::vector<MachineDef> const& machineMapping,
    PrototypePart const& part, BlueprintPart const& partBP)
{
    std::vector<PrototypeMachine> const& protoMachines = part.get_machines();
    std::vector<BlueprintMachine> const& bpMachines = partBP.m_machines;
    for (auto const& obj : machineMapping)
    {
        add_machines_to_object(rScene, partEnt, obj.m_machineOwner,
            protoMachines, bpMachines, obj.m_machineIndices);
    }
}

std::pair<ActiveEnt, std::vector<SysVehicle::MachineDef>> SysVehicle::part_instantiate(
    ActiveScene& rScene, PrototypePart& part, BlueprintPart& blueprint, ActiveEnt rootParent)
{
    std::vector<PrototypeObject> const& prototypes = part.get_objects();
    std::vector<ActiveEnt> newEntities(prototypes.size());

    /* A list of MachineDefs, which catalog all part machines and the ActiveEnts
     * that are created to represent the objects that own them
     */
    std::vector<MachineDef> machineMapping;
    machineMapping.reserve(prototypes.size());

    //std::cout << "size: " << newEntities.size() << "\n";

    for (size_t i = 0; i < prototypes.size(); i++)
    {
        PrototypeObject const& currentPrototype = prototypes[i];
        ActiveEnt parentEnt;

        // Get parent
        if (currentPrototype.m_parentIndex == i)
        {
            // if parented to self,
            parentEnt = rootParent;//m_scene->hier_get_root();
        }
        else
        {
            // since objects were loaded recursively, the parents always load
            // before their children
            parentEnt = newEntities[currentPrototype.m_parentIndex];
        }

        // Create the new entity
        ActiveEnt currentEnt = rScene.hier_create_child(parentEnt,
                                                currentPrototype.m_name);
        newEntities[i] = currentEnt;

        // Add and set transform component
        ACompTransform& currentTransform
                = rScene.reg_emplace<ACompTransform>(currentEnt);
        currentTransform.m_transform
                = Matrix4::from(currentPrototype.m_rotation.toMatrix(),
                                currentPrototype.m_translation)
                * Matrix4::scaling(currentPrototype.m_scale);

        if (currentPrototype.m_type == ObjectType::MESH)
        {
            using Magnum::GL::Mesh;
            using Magnum::Trade::MeshData;
            using Magnum::GL::Texture2D;
            using Magnum::Trade::ImageData2D;

            // Current prototype is a mesh, get the mesh and add it

            Package& package = rScene.get_application().debug_find_package("lzdb");
            const DrawableData& drawable =
                std::get<DrawableData>(currentPrototype.m_objectData);

            //Mesh* mesh = nullptr;
            Package& glResources = rScene.get_context_resources();
            DependRes<Mesh> meshRes = glResources.get<Mesh>(
                                            part.get_strings()[drawable.m_mesh]);

            if (meshRes.empty())
            {
                // Mesh isn't compiled yet, now check if mesh data exists
                std::string const& meshName = part.get_strings()[drawable.m_mesh];
                DependRes<MeshData> meshData = package.get<MeshData>(meshName);
                meshRes = AssetImporter::compile_mesh(meshData, glResources);
            }

            std::vector<DependRes<Texture2D>> textureResources;
            textureResources.reserve(drawable.m_textures.size());
            for (unsigned i = 0; i < drawable.m_textures.size(); i++)
            {
                unsigned texID = drawable.m_textures[i];
                std::string const& texName = part.get_strings()[texID];
                DependRes<Texture2D> texRes = glResources.get<Texture2D>(texName);

                if (texRes.empty())
                {
                    DependRes<ImageData2D> imageData = package.get<ImageData2D>(texName);
                    texRes = AssetImporter::compile_tex(imageData, glResources);
                }
                textureResources.push_back(texRes);
            }

            // by now, the mesh and texture should both exist
            using adera::shader::Phong;
            Phong::ACompPhongInstance shader;
            shader.m_shaderProgram = glResources.get<Phong>("phong_shader");
            shader.m_textures = std::move(textureResources);
            shader.m_lightPosition = Vector3{10.0f, 15.0f, 5.0f};
            shader.m_ambientColor = 0x111111_rgbf;
            shader.m_specularColor = 0x330000_rgbf;
            rScene.reg_emplace<Phong::ACompPhongInstance>(currentEnt, std::move(shader));

            CompDrawableDebug& bBocks = rScene.reg_emplace<CompDrawableDebug>(
                        currentEnt, meshRes, &Phong::draw_entity, 0x0202EE_rgbf);

            //new DrawablePhongColored(*obj, *m_shader, *mesh, 0xff0000_rgbf, m_drawables);
        }
        else if (currentPrototype.m_type == ObjectType::COLLIDER)
        {
            ACompCollisionShape& collision = rScene.reg_emplace<ACompCollisionShape>(currentEnt);
            const ColliderData& cd = std::get<ColliderData>(currentPrototype.m_objectData);
            collision.m_shape = cd.m_type;

        }

        // Save the list of machines this object owns
        machineMapping.emplace_back(currentEnt, currentPrototype.m_machineIndices);
    }

    // Create mass
    rScene.reg_emplace<ACompMass>(newEntities[0], part.get_mass());

    // return root object
    return {newEntities[0], machineMapping};
}

void SysVehicle::update_activate(ActiveScene &rScene)
{
    ACompAreaLink *pArea = SysAreaAssociate::try_get_area_link(rScene);

    if (pArea == nullptr)
    {
        return;
    }

    Universe &rUni = pArea->get_universe();
    TypeSatIndex type = rUni.sat_type_find_index<universe::SatVehicle>();
    ActivationTracker& activations = pArea->get_tracker(type);

    // Delete vehicles that have gone too far from the ActiveArea range
    for (auto const &[sat, ent] : activations.m_leave)
    {
        rScene.hier_destroy(ent);
    }

    // Update transforms of already-activated vehicle satellites
    auto view = rScene.get_registry().view<ACompVehicle, ACompTransform,
                                           ACompActivatedSat>();
    for (ActiveEnt vehicleEnt : view)
    {
        auto &vehicleTf = view.get<ACompTransform>(vehicleEnt);
        auto &vehicleSat = view.get<ACompActivatedSat>(vehicleEnt);
        SysAreaAssociate::sat_transform_set_relative(
            rUni, pArea->m_areaSat, vehicleSat.m_sat, vehicleTf.m_transform);
    }

    // Activate nearby vehicle satellites that have just entered the ActiveArea
    for (auto &entered : activations.m_enter)
    {
        universe::Satellite sat = entered->first;
        ActiveEnt &rEnt = entered->second;

        rEnt = activate(rScene, rUni, pArea->m_areaSat, sat);
    }
}

void SysVehicle::update_vehicle_modification(ActiveScene& rScene)
{
    auto view = rScene.get_registry().view<ACompVehicle>();
    auto viewParts = rScene.get_registry().view<ACompPart>();

    // this part is sort of temporary and unoptimized. deal with it when it
    // becomes a problem. TODO: use more views

    for (ActiveEnt vehicleEnt : view)
    {
        ACompVehicle &vehicleVehicle = view.get(vehicleEnt);
        //std::vector<ActiveEnt> &parts = vehicleVehicle.m_parts;

        if (vehicleVehicle.m_separationCount > 0)
        {
            // Separation requested

            // mark collider as dirty
            auto &rVehicleBody = rScene.reg_get<ACompRigidBody_t>(vehicleEnt);
            rVehicleBody.m_colliderDirty = true;

            // Invalidate all ACompRigidbodyAncestors
            auto invalidateAncestors = [&rScene](ActiveEnt e)
            {
                auto* pRBA = rScene.get_registry().try_get<ACompRigidbodyAncestor>(e);
                if (pRBA) { pRBA->m_ancestor = entt::null; }
                return EHierarchyTraverseStatus::Continue;
            };
            rScene.hierarchy_traverse(vehicleEnt, invalidateAncestors);

            // Create the islands vector
            // [0]: current vehicle
            // [1+]: new vehicles
            std::vector<ActiveEnt> islands(vehicleVehicle.m_separationCount);
            vehicleVehicle.m_separationCount = 0;

            islands[0] = vehicleEnt;

            // NOTE: vehicleVehicle and vehicleTransform become invalid
            //       when emplacing new ones.
            for (unsigned i = 1; i < islands.size(); i ++)
            {
                ActiveEnt islandEnt = rScene.hier_create_child(
                            rScene.hier_get_root());
                rScene.reg_emplace<ACompVehicle>(islandEnt);
                auto &islandTransform
                        = rScene.reg_emplace<ACompTransform>(islandEnt);
                auto &islandBody
                        = rScene.reg_emplace<ACompRigidBody_t>(islandEnt);
                auto &islandShape
                        = rScene.reg_emplace<ACompCollisionShape>(islandEnt);
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
                ACompPart &partPart = viewParts.get(partEnt);
                if (partPart.m_destroy)
                {
                    // destroy this part
                    rScene.hier_destroy(partEnt);
                    return true;
                }

                if (partPart.m_separationIsland)
                {
                    // separate into a new vehicle

                    ActiveEnt islandEnt = islands[partPart.m_separationIsland];
                    auto &islandVehicle = rScene.reg_get<ACompVehicle>(
                                islandEnt);
                    islandVehicle.m_parts.push_back(partEnt);

                    rScene.hier_set_parent_child(islandEnt, partEnt);

                    return true;
                }

                return false;

            };

            std::vector<ActiveEnt> &parts = view.get(vehicleEnt).m_parts;

            parts.erase(std::remove_if(parts.begin(), parts.end(),
                                       removeDestroyed), parts.end());

            // update or create rigid bodies. also set center of masses

            for (ActiveEnt islandEnt : islands)
            {
                ACompVehicle &islandVehicle = view.get(islandEnt);
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

