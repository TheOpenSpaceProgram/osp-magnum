#include "SysVehicle.h"
#include "ActiveScene.h"
#include "../Satellites/SatActiveArea.h"
#include "../Satellites/SatVehicle.h"
#include "../Resource/PrototypePart.h"

#include <Magnum/MeshTools/Compile.h>
#include <Magnum/Trade/MeshData.h>

#include <iostream>


using namespace osp;
using namespace osp::active;

// for the 0xrrggbb_rgbf literalsm
using namespace Magnum::Math::Literals;

SysVehicle::SysVehicle(ActiveScene &scene) :
        m_scene(scene),
        m_updateVehicleModification(
            scene.get_update_order(), "vehicle_modification", "", "physics",
            std::bind(&SysVehicle::update_vehicle_modification, this))
{
    m_shader = std::make_unique<Magnum::Shaders::Phong>(Magnum::Shaders::Phong{});
}

StatusActivated SysVehicle::activate_sat(ActiveScene &scene,
                                         SysAreaAssociate &area,
                                         universe::Satellite areaSat,
                                         universe::Satellite tgtSat)
{

    std::cout << "loadin a vehicle!\n";

    universe::Universe &uni = area.get_universe();
    //SysVehicle& self = scene.get_system<SysVehicle>();
    auto &loadMeVehicle
            = uni.get_reg().get<universe::UCompVehicle>(tgtSat);
    auto &tgtPosTraj
            = uni.get_reg().get<universe::UCompTransformTraj>(tgtSat);

    // make sure there is vehicle data to load
    if (loadMeVehicle.m_blueprint.empty())
    {
        // no vehicle data to load
        return {1, entt::null, false};
    }

    BlueprintVehicle &vehicleData = *(loadMeVehicle.m_blueprint);
    ActiveEnt root = scene.hier_get_root();

    // Create the root entity to add parts to

    ActiveEnt vehicleEnt = scene.hier_create_child(root, "Vehicle");

    ACompVehicle& vehicleComp = scene.reg_emplace<ACompVehicle>(vehicleEnt);

    // Convert position of the satellite to position in scene
    Vector3 positionInScene
            = area.get_universe().sat_calc_pos_meters(areaSat, tgtSat);

    ACompTransform& vehicleTransform = scene.get_registry()
                                        .emplace<ACompTransform>(vehicleEnt);
    vehicleTransform.m_transform
            = Matrix4::from(tgtPosTraj.m_rotation.toMatrix(), positionInScene);
    vehicleTransform.m_enableFloatingOrigin = true;

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
            return {1, entt::null, false};
        }

        PrototypePart &proto = *partDepends;

        ActiveEnt partEntity = this->part_instantiate(proto, vehicleEnt);
        vehicleComp.m_parts.push_back(partEntity);

        auto& partPart = scene.reg_emplace<ACompPart>(partEntity);
        partPart.m_vehicle = vehicleEnt;

        // Part now exists

        // TODO: Deal with blueprint machines instead of prototypes directly

        auto &partMachines = scene.reg_emplace<ACompMachines>(partEntity);

        for (PrototypeMachine& protoMachine : proto.get_machines())
        {
            MapSysMachine::iterator sysMachine
                    = scene.system_machine_find(protoMachine.m_type);

            if (!(scene.system_machine_it_valid(sysMachine)))
            {
                std::cout << "Machine: " << protoMachine.m_type << " Not found\n";
                continue;
            }

            // TODO: pass the blueprint configs into this function
            Machine& machine = sysMachine->second->instantiate(partEntity);

            // Add the machine to the part
            partMachines.m_machines.emplace_back(partEntity, sysMachine);

        }

        //std::cout << "empty? " << partMachines.m_machines.isEmpty() << "\n";

        //std::cout << "entity: " << int(partEntity) << "\n";

        ACompTransform& partTransform = scene.get_registry()
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

    SysWire& sysWire = scene.get_system<SysWire>();

    // Loop through wire connections
    for (BlueprintWire& blueprintWire : vehicleData.get_wires())
    {
        // TODO: check if the connections are valid

        // get wire from

        ACompMachines& fromMachines = scene.reg_get<ACompMachines>(
                *(vehicleComp.m_parts.begin() + blueprintWire.m_fromPart));

        auto fromMachineEntry = fromMachines
                .m_machines[blueprintWire.m_fromMachine];
        Machine &fromMachine = fromMachineEntry.m_system->second
                ->get(fromMachineEntry.m_partEnt);
        WireOutput* fromWire =
                fromMachine.request_output(blueprintWire.m_fromPort);

        // get wire to

        ACompMachines& toMachines = scene.reg_get<ACompMachines>(
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
    ACompRigidBody& vehicleBody = scene.reg_emplace<ACompRigidBody>(vehicleEnt);
    ACompCollisionShape& vehicleShape = scene.reg_emplace<ACompCollisionShape>(vehicleEnt);
    vehicleShape.m_shape = ECollisionShape::COMBINED;
    scene.get_system<SysPhysics>().create_body(vehicleEnt);

    return {0, vehicleEnt, true};
}

int SysVehicle::deactivate_sat(ActiveScene &scene, SysAreaAssociate &area,
        universe::Satellite areaSat, universe::Satellite tgtSat,
        ActiveEnt tgtEnt)
{
    area.sat_transform_update(tgtEnt);
    return 0;
}


ActiveEnt SysVehicle::part_instantiate(PrototypePart& part,
                                       ActiveEnt rootParent)
{

    std::vector<PrototypeObject> const& prototypes = part.get_objects();
    std::vector<ActiveEnt> newEntities(prototypes.size());

    //std::cout << "size: " << newEntities.size() << "\n";

    for (unsigned i = 0; i < prototypes.size(); i ++)
    {
        const PrototypeObject& currentPrototype = prototypes[i];
        ActiveEnt currentEnt, parentEnt;

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
        currentEnt = m_scene.hier_create_child(parentEnt,
                                                currentPrototype.m_name);
        newEntities[i] = currentEnt;

        // Add and set transform component
        ACompTransform& currentTransform
                = m_scene.reg_emplace<ACompTransform>(currentEnt);
        currentTransform.m_transform
                = Matrix4::from(currentPrototype.m_rotation.toMatrix(),
                                currentPrototype.m_translation)
                * Matrix4::scaling(currentPrototype.m_scale);
        currentTransform.m_enableFloatingOrigin = true;

        if (currentPrototype.m_type == ObjectType::MESH)
        {
            using Magnum::Trade::MeshData;
            using Magnum::GL::Mesh;

            // Current prototype is a mesh, get the mesh and add it

            // TODO: put package prefixes in resource path
            //       for now just get the first package
            Package& package = m_scene.get_application().debug_get_packges()[0];

            //Mesh* mesh = nullptr;
            DependRes<Mesh> meshRes = package.get<Mesh>(
                                            part.get_strings()[currentPrototype.m_drawable.m_mesh]);

            if (meshRes.empty())
            {
                // Mesh isn't compiled yet, now check if mesh data exists
                std::string const& meshName
                        = part.get_strings()
                                    [currentPrototype.m_drawable.m_mesh];

                DependRes<MeshData> meshDataRes
                        = package.get<MeshData>(meshName);

                if (!meshDataRes.empty())
                {
                    // Compile the mesh data into a mesh

                    std::cout << "Compiling mesh \"" << meshName << "\"\n";

                    MeshData &meshData = *meshDataRes;

                    //Resource<Mesh> compiledMesh;
                    //compiledMesh.m_data = Magnum::MeshTools::compile(meshData);
                    //mesh = &(package.debug_add_resource<Mesh>(
                    //            std::move(compiledMesh))->m_data);

                    meshRes = package.add<Mesh>(meshName, Magnum::MeshTools::compile(meshData));

                }
                else
                {
                    std::cout << "Mesh \"" << meshName << "\" doesn't exist!\n";
                    return entt::null;
                }

            }
            else
            {
                // mesh already loaded
            }

            // by now, the mesh should exist

            CompDrawableDebug& bBocks
                    = m_scene.reg_emplace<CompDrawableDebug>(
                        currentEnt, &(*meshRes), m_shader.get(), 0x0202EE_rgbf);

            //new DrawablePhongColored(*obj, *m_shader, *mesh, 0xff0000_rgbf, m_drawables);
        }
        else if (currentPrototype.m_type == ObjectType::COLLIDER)
        {
            ACompCollisionShape collision = m_scene.reg_emplace<ACompCollisionShape>(currentEnt);
            collision.m_shape = currentPrototype.m_collider.m_type;

        }
    }


    // first element is 100% going to be the root object

    // Temporary: add a rigid body root
    //CompNewtonBody& nwtBody
    //        = m_scene->get_registry().emplace<CompNewtonBody>(newEntities[0]);


    // return root object
    return newEntities[0];
}

void SysVehicle::update_vehicle_modification()
{
    auto view = m_scene.get_registry().view<ACompVehicle>();
    auto viewParts = m_scene.get_registry().view<ACompPart>();

    // this part is sort of temporary and unoptimized. deal with it when it
    // becomes a problem. TODO: use more views

    for (ActiveEnt vehicleEnt : view)
    {
        ACompVehicle &vehicleVehicle = view.get(vehicleEnt);
        auto &vehicleTransform = m_scene.reg_get<ACompTransform>(vehicleEnt);
        //std::vector<ActiveEnt> &parts = vehicleVehicle.m_parts;

        if (vehicleVehicle.m_separationCount)
        {
            // Separation requestedts

            // Create the islands vector
            // [0]: current vehicle
            // [1+]: new vehicles
            std::vector<ActiveEnt> islands(vehicleVehicle.m_separationCount);
            vehicleVehicle.m_separationCount = 0;

            islands[0] = vehicleEnt;

            for (unsigned i = 1; i < islands.size(); i ++)
            {
                ActiveEnt islandEnt = m_scene.hier_create_child(
                            m_scene.hier_get_root());
                m_scene.reg_emplace<ACompVehicle>(islandEnt);
                auto &islandTransform
                        = m_scene.reg_emplace<ACompTransform>(islandEnt);
                auto &islandBody
                        = m_scene.reg_emplace<ACompRigidBody>(islandEnt);
                auto &islandShape
                        = m_scene.reg_emplace<ACompCollisionShape>(islandEnt);
                islandShape.m_shape = ECollisionShape::COMBINED;

                islandTransform.m_transform = vehicleTransform.m_transform;
                islandTransform.m_enableFloatingOrigin = true;

                islands[i] = islandEnt;

                // note: vehicleVehicle and vehicleTransform become invalid
                //       when emplacing new ones.
            }


            // iterate through parts
            // * remove parts that are destroyed, destroy the part entity too
            // * remove parts different islands, and move them to the new
            //   vehicle

            ActiveScene &scene = m_scene;

            entt::basic_registry<ActiveEnt> &reg = m_scene.get_registry();
            auto removeDestroyed = [&viewParts, &scene, &islands]
                    (ActiveEnt partEnt) -> bool
            {
                ACompPart &partPart = viewParts.get(partEnt);
                if (partPart.m_destroy)
                {
                    // destroy this part
                    scene.hier_destroy(partEnt);
                    return true;
                }

                if (partPart.m_separationIsland)
                {
                    // separate into a new vehicle

                    ActiveEnt islandEnt = islands[partPart.m_separationIsland];
                    auto &islandVehicle = scene.reg_get<ACompVehicle>(
                                islandEnt);
                    islandVehicle.m_parts.push_back(partEnt);

                    scene.hier_set_parent_child(islandEnt, partEnt);

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
                        = m_scene.reg_get<ACompTransform>(islandEnt);

                Vector3 comOffset;
                //float totalMass;

                for (ActiveEnt partEnt : islandVehicle.m_parts)
                {
                    // TODO: deal with part mass
                    auto const &partTransform
                            = m_scene.reg_get<ACompTransform>(partEnt);

                    comOffset += partTransform.m_transform.translation();

                }

                comOffset /= islandVehicle.m_parts.size();



                for (ActiveEnt partEnt : islandVehicle.m_parts)
                {
                    ActiveEnt child = m_scene.reg_get<ACompHierarchy>(partEnt)
                                        .m_childFirst;
                    auto &partTransform
                            = m_scene.reg_get<ACompTransform>(partEnt);

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

                m_scene.get_system<SysPhysics>().create_body(islandEnt);
            }

        }
    }
}

