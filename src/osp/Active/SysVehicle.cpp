#include <iostream>

#include "ActiveScene.h"

#include "DebugObject.h"

#include "SysVehicle.h"
#include "../Satellites/SatActiveArea.h"
#include "../Satellites/SatVehicle.h"



using namespace osp;

// for the 0xrrggbb_rgbf literalsm
using namespace Magnum::Math::Literals;

SysVehicle::SysVehicle(ActiveScene &scene) :
        m_scene(scene)
{
    m_shader = std::make_unique<Magnum::Shaders::Phong>(Magnum::Shaders::Phong{});
}

int SysVehicle::area_activate_vehicle(SatActiveArea& area, SatelliteObject& loadMe)
{

    std::cout << "loadin a vehicle!\n";

    SysVehicle& self = area.get_scene()->get_system<SysVehicle>();

    // everything that involves this variable is temporary
    bool debugFirstVehicle = !(area.get_scene()->get_registry().size<CompRigidBody>());

    //std::cout << "relative distance: " << positionInScene.length() << "\n";

    // Get the needed variables
    SatVehicle &vehicle = static_cast<SatVehicle&>(loadMe);
    BlueprintVehicle &vehicleData = vehicle.get_blueprint();
    ActiveScene &scene = *(area.get_scene());
    ActiveEnt root = scene.hier_get_root();

    // Create the root entity to add parts to

    ActiveEnt vehicleEnt = scene.hier_create_child(root, "Vehicle");

    // Convert position of the satellite to position in scene
    Vector3 positionInScene = loadMe.get_satellite()
            ->position_relative_to(*(area.get_satellite())).to_meters();

    CompTransform& vehicleTransform = scene.get_registry()
                                        .emplace<CompTransform>(vehicleEnt);
    vehicleTransform.m_transform = Matrix4::translation(positionInScene);
    vehicleTransform.m_enableFloatingOrigin = true;

    // Create the parts

    // Unique part prototypes used in the vehicle
    // Access with [blueprintParts.m_partIndex]
    std::vector<ResDependency<PrototypePart> >& partsUsed =
            vehicleData.get_prototypes();

    // All the parts in the vehicle
    std::vector<BlueprintPart> &blueprintParts = vehicleData.get_blueprints();

    // Keep track of newly created parts
    std::vector<ActiveEnt> newEntities;
    newEntities.reserve(blueprintParts.size());

    // Loop through list of blueprint parts
    for (BlueprintPart& partBp : blueprintParts)
    {
        ResDependency<PrototypePart>& partDepends =
                partsUsed[partBp.m_partIndex];

        PrototypePart *proto = partDepends.get_data();

        // Check if the part prototype this depends on still exists
        if (!proto)
        {
            return -1;
        }

        ActiveEnt partEntity = self.part_instantiate(*proto, vehicleEnt);
        newEntities.push_back(partEntity);

        // Part now exists

        // TODO: Deal with blueprint machines instead of prototypes directly

        ActiveScene &scene = *(area.get_scene());

        CompMachine& partMachines = scene.reg_emplace<CompMachine>(partEntity);

        for (PrototypeMachine& protoMachine : proto->get_machines())
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


            if (debugFirstVehicle)
            {
                machine.m_enable = true;
            }

            // Add the machine to the part
            partMachines.m_machines.emplace_back(partEntity, sysMachine);

        }

        //std::cout << "empty? " << partMachines.m_machines.isEmpty() << "\n";

        //std::cout << "entity: " << int(partEntity) << "\n";

        CompTransform& partTransform = scene.get_registry()
                                            .get<CompTransform>(partEntity);

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

        CompMachine& fromMachines = scene.reg_get<CompMachine>(
                                        newEntities[blueprintWire.m_fromPart]);

        auto fromMachineEntry = fromMachines
                .m_machines[blueprintWire.m_fromMachine];
        Machine &fromMachine = fromMachineEntry.m_system->second
                ->get(fromMachineEntry.m_partEnt);
        WireOutput* fromWire =
                fromMachine.request_output(blueprintWire.m_fromPort);

        // get wire to

        CompMachine& toMachines = scene.reg_get<CompMachine>(
                                        newEntities[blueprintWire.m_toPart]);

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
    CompRigidBody& nwtBody = scene.reg_emplace<CompRigidBody>(vehicleEnt);
    area.get_scene()->get_system<SysNewton>().create_body(vehicleEnt);

    if (debugFirstVehicle)
    {
        // if first loaded vehicle

        // point the camera
        DebugCameraController *cam = (DebugCameraController*)(
                    area.get_scene()->reg_get<CompDebugObject>(
                        area.get_camera()).m_obj.get());
        cam->view_orbit(vehicleEnt);


    }

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
        CompTransform& currentTransform
                = m_scene.reg_emplace<CompTransform>(currentEnt);
        currentTransform.m_transform
                = Matrix4::from(currentPrototype.m_rotation.toMatrix(),
                                currentPrototype.m_translation)
                * Matrix4::scaling(currentPrototype.m_scale);
        currentTransform.m_enableFloatingOrigin = true;

        if (currentPrototype.m_type == ObjectType::MESH)
        {
            using Magnum::Trade::MeshData3D;
            using Magnum::GL::Mesh;

            // Current prototype is a mesh, get the mesh and add it

            // TODO: put package prefixes in resource path
            //       for now just get the first package
            Package& package = m_scene.get_application().debug_get_packges()[0];

            Mesh* mesh = nullptr;
            Resource<Mesh>* meshRes = package.get_resource<Mesh>(
                                            currentPrototype.m_drawable.m_mesh);

            if (!meshRes)
            {
                // Mesh isn't compiled yet, now check if mesh data exists
                std::string const& meshName
                        = part.get_strings()
                                    [currentPrototype.m_drawable.m_mesh];

                Resource<MeshData3D>* meshDataRes
                        = package.get_resource<MeshData3D>(meshName);

                if (meshDataRes)
                {
                    // Compile the mesh data into a mesh

                    std::cout << "Compiling mesh \"" << meshName << "\"\n";

                    MeshData3D* meshData = &(meshDataRes->m_data);

                    Resource<Mesh> compiledMesh;
                    compiledMesh.m_data = Magnum::MeshTools::compile(*meshData);

                    mesh = &(package.debug_add_resource<Mesh>(
                                std::move(compiledMesh))->m_data);

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
                // TODO: this actually crashes, as all the opengl stuff were
                //       cleared.
                mesh = &(meshRes->m_data);
            }

            // by now, the mesh should exist

            CompDrawableDebug& bBocks
                    = m_scene.reg_emplace<CompDrawableDebug>(
                        currentEnt, mesh, m_shader.get(), 0x0202EE_rgbf);

            //new DrawablePhongColored(*obj, *m_shader, *mesh, 0xff0000_rgbf, m_drawables);
        }
        else if (currentPrototype.m_type == ObjectType::COLLIDER)
        {
            //new FtrNewtonBody(*obj, *this);
            // TODO collision shape!
        }
    }


    // first element is 100% going to be the root object

    // Temporary: add a rigid body root
    //CompNewtonBody& nwtBody
    //        = m_scene->get_registry().emplace<CompNewtonBody>(newEntities[0]);


    // return root object
    return newEntities[0];
}

