// This file is a bit spaghetti-style, but should be easy to understand.
// All parts of OSP can be configured through just C++, and understanding what
// this file is doing is a good start to getting familar with the codebase.
// Replace this entire file eventually.

#include <iostream>
//#include <format>
#include <memory>
#include <thread>
#include <stack>
#include <random>

#include "OSPMagnum.h"
#include "DebugObject.h"

#include "osp/types.h"
#include "osp/Universe.h"
#include "osp/Trajectories/Stationary.h"
#include "osp/Satellites/SatActiveArea.h"
#include "osp/Satellites/SatVehicle.h"
#include "osp/Resource/SturdyImporter.h"
#include "osp/Resource/Package.h"

#include "osp/Active/ActiveScene.h"
#include "osp/Active/SysVehicle.h"
#include "osp/Active/SysAreaAssociate.h"


#include "adera/Machines/UserControl.h"
#include "adera/Machines/Rocket.h"

#include "planet-a/Satellites/SatPlanet.h"
#include "planet-a/Active/SysPlanetA.h"


namespace universe
{
    using namespace osp::universe;
    using namespace planeta::universe;
}

namespace active
{
    using namespace osp::active;
    using namespace adera::active;
    using namespace planeta::active;
}

namespace machines
{
    using namespace adera::active::machines;
}

using osp::Vector2;
using osp::Vector3;
using osp::Matrix4;
using osp::Quaternion;

// for the 0xrrggbb_rgbf and angle literals
using namespace Magnum::Math::Literals;


/**
 * Starts a magnum application, an active area, and links them together
 */
void magnum_application();

void config_controls();

/**
 * As the name implies. This should only be called once for the entire lifetime
 * of the program.
 *
 * prefer not to use names like this anywhere else but main.cpp
 */
void load_a_bunch_of_stuff();

/**
 * Adds stuff to the universe
 */
void create_solar_system();

/**
 * Creates a BlueprintVehicle and adds a random mess of 10 part_spamcans to it
 *
 * Also creates a
 *
 * Call load_a_bunch_of_stuff before this function to make sure part_spamcan
 * is loaded
 *
 * @param name
 * @return
 */
osp::universe::Satellite debug_add_random_vehicle(std::string const& name);

/**
 * The spaghetti command line interface that gets inputs from stdin. This
 * function will only return once the user exits.
 * @return An error code maybe
 */
int debug_cli_loop();

// called only from commands to display information
void debug_print_help();
void debug_print_sats();
void debug_print_hier();
void debug_print_update_order();

// Deals with the underlying OSP universe, with the satellites and stuff. A
// Magnum application or OpenGL context is not required for the universe to
// exist. This also stores loaded resources in packages.
osp::OSPApplication g_osp;

// Deals with the window, OpenGL context, and other game engine stuff that
// often have "Active" written all over them
std::unique_ptr<osp::OSPMagnum> g_ospMagnum;
std::thread g_magnumThread;

// lazily save the arguments to pass to Magnum
int g_argc;
char** g_argv;

int main(int argc, char** argv)
{   
    // eventually do more important things here.
    // just lazily save the arguments
    g_argc = argc;
    g_argv = argv;

    // start doing debug cli loop
    return debug_cli_loop();
}

int debug_cli_loop()
{

    debug_print_help();

    std::string command;

    while(true)
    {
        std::cout << "> ";
        std::cin >> command;

        if (command == "help")
        {
            debug_print_help();
        }
        if (command == "list_uni")
        {
            debug_print_sats();
        }
        if (command == "list_ent")
        {
            debug_print_hier();
        }
        if (command == "list_upd")
        {
            debug_print_update_order();
        }
        else if (command == "start")
        {
            if (g_magnumThread.joinable())
            {
                g_magnumThread.join();
            }
            std::thread t(magnum_application);
            g_magnumThread.swap(t);
        }
        else if (command == "exit")
        {
            if (g_ospMagnum)
            {
                // request exit if application exists
                g_ospMagnum->exit();
            }
            break;
        }
        else
        {
            std::cout << "that doesn't do anything ._.\n";
        }
    }

    // wait for magnum thread to exit if it exists
    if (g_magnumThread.joinable())
    {
        g_magnumThread.join();
    }

    // destory the universe
    //g_osp.get_universe().get_sats().clear();
    return 0;
}

void magnum_application()
{
    // Create the application
    g_ospMagnum = std::make_unique<osp::OSPMagnum>(
                        osp::OSPMagnum::Arguments{g_argc, g_argv}, g_osp);

    config_controls(); // as the name implies

    // Load if not loaded yet. This only calls once during entire runtime
    if (!g_osp.debug_get_packges().size())
    {
        load_a_bunch_of_stuff();
        create_solar_system();
    }

    // Create an ActiveArea, an ActiveScene, then connect them together

    // Get needed variables
    universe::Universe &uni = g_osp.get_universe();
    universe::SatActiveArea *satAA = static_cast<universe::SatActiveArea*>(
            uni.sat_type_find("Vehicle")->second.get());
    universe::SatPlanet *satPlanet = static_cast<universe::SatPlanet*>(
            uni.sat_type_find("Planet")->second.get());

    // Create an ActiveScene
    active::ActiveScene& scene = g_ospMagnum->scene_add("Area 1");

    // Register dynamic systems for that scene
    auto &sysArea = scene.dynamic_system_add<active::SysAreaAssociate>(
                "AreaAssociate", uni);
    auto &sysVehicle = scene.dynamic_system_add<active::SysVehicle>(
                "Vehicle");
    auto &sysPlanet = scene.dynamic_system_add<active::SysPlanetA>("Planet");

    // Register machines for that scene
    scene.system_machine_add<machines::SysMachineUserControl>("UserControl",
            g_ospMagnum->get_input_handler());
    scene.system_machine_add<machines::SysMachineRocket>("Rocket");

    // Make active areas load vehicles and planets
    sysArea.activator_add(satAA, sysVehicle);
    sysArea.activator_add(satPlanet, sysPlanet);

    // create a Satellite with an ActiveArea
    universe::Satellite sat = g_osp.get_universe().sat_create();

    // assign sat as an ActiveArea
    universe::UCompActiveArea &area = satAA->add_get_ucomp(sat);

    // Link ActiveArea to scene using the AreaAssociate
    sysArea.connect(sat);

    // Add a camera to the scene

    // Create the camera entity
    active::ActiveEnt camera = scene.hier_create_child(scene.hier_get_root(),
                                                       "Camera");
    auto &cameraTransform = scene.reg_emplace<active::ACompTransform>(camera);
    auto &cameraComp = scene.get_registry().emplace<active::ACompCamera>(camera);

    cameraTransform.m_transform = Matrix4::translation(Vector3(0, 0, 25));
    cameraTransform.m_enableFloatingOrigin = true;

    cameraComp.m_viewport
            = Vector2(Magnum::GL::defaultFramebuffer.viewport().size());
    cameraComp.m_far = 4096.0f;
    cameraComp.m_near = 0.125f;
    cameraComp.m_fov = 45.0_degf;

    cameraComp.calculate_projection();

    // Add the debug camera controller to the scene. This adds controls
    auto camObj = std::make_unique<osp::DebugCameraController>(scene, camera);

    // Add a CompDebugObject to camera to manage camObj's lifetime
    scene.reg_emplace<osp::CompDebugObject>(camera, std::move(camObj));

    // Starts the game loop. This function is blocking, and will only return
    // when the window is  closed. See OSPMagnum::drawEvent
    g_ospMagnum->exec();

    // Close button has been pressed

    std::cout << "Magnum Application closed\n";

    // Disconnect ActiveArea
    sysArea.disconnect();

    // workaround: wipe mesh resources because they're specific to the
    // opengl context
    g_osp.debug_get_packges()[0].clear<Magnum::GL::Mesh>();

    // destruct the application, this closes the window
    g_ospMagnum.reset();
}

void config_controls()
{
    // Configure Controls

    // It should be pretty easy to write a config file parser that calls these
    // functions.

    using namespace osp;

    using Key = OSPMagnum::KeyEvent::Key;
    using VarOp = ButtonVarConfig::VarOperator;
    using VarTrig = ButtonVarConfig::VarTrigger;
    UserInputHandler& userInput = g_ospMagnum->get_input_handler();

    // vehicle control, used by MachineUserControl

    // would help to get an axis for yaw, pitch, and roll, but use individual
    // axis buttons for now
    userInput.config_register_control("vehicle_pitch_up", true,
            {{0, (int) Key::S, VarTrig::PRESSED, false, VarOp::AND}});
    userInput.config_register_control("vehicle_pitch_dn", true,
            {{0, (int) Key::W, VarTrig::PRESSED, false, VarOp::AND}});
    userInput.config_register_control("vehicle_yaw_lf", true,
            {{0, (int) Key::A, VarTrig::PRESSED, false, VarOp::AND}});
    userInput.config_register_control("vehicle_yaw_rt", true,
            {{0, (int) Key::D, VarTrig::PRESSED, false, VarOp::AND}});
    userInput.config_register_control("vehicle_roll_lf", true,
            {{0, (int) Key::Q, VarTrig::PRESSED, false, VarOp::AND}});
    userInput.config_register_control("vehicle_roll_rt", true,
            {{0, (int) Key::E, VarTrig::PRESSED, false, VarOp::AND}});

    // Set throttle max to Z
    userInput.config_register_control("vehicle_thr_max", false,
            {{0, (int) Key::Z, VarTrig::PRESSED, false, VarOp::OR}});
    // Set throttle min to X
    userInput.config_register_control("vehicle_thr_min", false,
            {{0, (int) Key::X, VarTrig::PRESSED, false, VarOp::OR}});
    // Set self destruct to LeftCtrl+C or LeftShift+A. this just prints
    // a silly message.
    userInput.config_register_control("vehicle_self_destruct", false,
            {{0, (int) Key::LeftCtrl, VarTrig::HOLD, false, VarOp::AND},
             {0, (int) Key::C, VarTrig::PRESSED, false, VarOp::OR},
             {0, (int) Key::LeftShift, VarTrig::HOLD, false, VarOp::AND},
             {0, (int) Key::A, VarTrig::PRESSED, false, VarOp::OR}});

    // Camera and Game controls, handled in DebugCameraController

    // Switch to next vehicle
    userInput.config_register_control("game_switch", false,
            {{0, (int) Key::V, VarTrig::PRESSED, false, VarOp::OR}});

    // Set UI Up/down/left/right to arrow keys. this is used to rotate the view
    // for now
    userInput.config_register_control("ui_up", true,
            {{0, (int) Key::Up, VarTrig::PRESSED, false, VarOp::AND}});
    userInput.config_register_control("ui_dn", true,
            {{0, (int) Key::Down, VarTrig::PRESSED, false, VarOp::AND}});
    userInput.config_register_control("ui_lf", true,
            {{0, (int) Key::Left, VarTrig::PRESSED, false, VarOp::AND}});
    userInput.config_register_control("ui_rt", true,
            {{0, (int) Key::Right, VarTrig::PRESSED, false, VarOp::AND}});
}

void load_a_bunch_of_stuff()
{
    // Create a new package
    osp::Package lazyDebugPack("lzdb", "lazy-debug");

    // Create a sturdy
    osp::SturdyImporter importer;
    importer.open_filepath("OSPData/adera/spamcan.sturdy.gltf");

    // load the sturdy into the package
    importer.load_config(lazyDebugPack);

    // Add package to the univere
    g_osp.debug_get_packges().push_back(std::move(lazyDebugPack));

    // Add 50 vehicles so there's something to load
    //g_osp.get_universe().get_sats().reserve(64);

    //s_partsLoaded = true;
}

void create_solar_system()
{
    using osp::universe::Universe;

    Universe& uni = g_osp.get_universe();

    // Register satellite types used
    uni.type_register<universe::SatActiveArea>(uni);
    uni.type_register<universe::SatVehicle>(uni);
    auto &typePlanet = uni.type_register<universe::SatPlanet>(uni);

    // Create trajectory that will make things added to the universe stationary
    auto &stationary = uni.trajectory_create<universe::TrajStationary>(
                                        uni, uni.sat_root());

    for (int i = 0; i < 20; i ++)
    {
        // Creates a random mess of spamcans
        universe::Satellite sat = debug_add_random_vehicle("TestyMcTestFace Mk"
                                                 + std::to_string(i));

        auto &posTraj = uni.get_reg().get<universe::UCompTransformTraj>(sat);

        posTraj.m_position = osp::Vector3s(i * 1024l * 5l, 0l, 0l);
        posTraj.m_dirty = true;

        stationary.add(sat);

    }


    // Add Grid of planets too
    // for now, planets are hard-coded to 128 meters in radius

    for (int x = -1; x < 2; x ++)
    {
        for (int z = -1; z < 2; z ++)
        {
            universe::Satellite sat = g_osp.get_universe().sat_create();

            // assign sat as a planet
            universe::UCompPlanet &planet = typePlanet.add_get_ucomp(sat);

            // set radius
            planet.m_radius = 128;

            auto &posTraj = uni.get_reg().get<universe::UCompTransformTraj>(sat);

            // space planets 400m apart from each other
            // 1024 units = 1 meter
            posTraj.m_position = {x * 1024l * 400l,
                                  1024l * -140l,
                                  z * 1024l * 400l};
        }
    }

}

osp::universe::Satellite debug_add_random_vehicle(std::string const& name)
{

    using osp::BlueprintVehicle;
    using osp::PrototypePart;
    using osp::DependRes;

    // Start making the blueprint

    BlueprintVehicle blueprint;

    // Part to add, very likely a spamcan
    DependRes<PrototypePart> victim =
            g_osp.debug_get_packges()[0]
            .get<PrototypePart>("part_spamcan");

    // Add 6 parts
    for (int i = 0; i < 6; i ++)
    {
        // Generate random vector
        Vector3 randomvec(std::rand() % 128 - 64,
                          std::rand() % 128 - 64,
                          std::rand() % 128 - 64);

        randomvec /= 64.0f;

        // Add a new [victim] part
        blueprint.add_part(victim, randomvec,
                           Quaternion(), Vector3(1, 1, 1));
        //std::cout << "random: " <<  << "\n";
    }

    // Wire throttle control
    // from (output): a MachineUserControl m_woThrottle
    // to    (input): a MachineRocket m_wiThrottle
    blueprint.add_wire(0, 0, 1,
                       0, 1, 2);

    // Wire attitude control to gimbal
    // from (output): a MachineUserControl m_woAttitude
    // to    (input): a MachineRocket m_wiGimbal
    blueprint.add_wire(0, 0, 0,
                       0, 1, 0);

    // put blueprint in package
    DependRes<BlueprintVehicle> depend = g_osp.debug_get_packges()[0]
            .add<BlueprintVehicle>(name, std::move(blueprint));

    // Create the Satellite containing a SatVehicle

    // TODO: make this more safe

    universe::Universe &uni = g_osp.get_universe();

    // Create blank satellite
    universe::Satellite sat = uni.sat_create();

    // Set the name
    auto &posTraj = uni.get_reg().get<universe::UCompTransformTraj>(sat);
    posTraj.m_name = name;

    // Make it into a vehicle
    auto &typeVehicle = *static_cast<universe::SatVehicle*>(
            uni.sat_type_find("Vehicle")->second.get());
    universe::UCompVehicle &ucompVehicle = typeVehicle.add_get_ucomp(sat);

    // set the SatVehicle's blueprint to the one just made
    ucompVehicle.m_blueprint = std::move(depend);

    return sat;

}

void debug_print_help()
{
    std::cout
        << "OSP-Magnum Temporary Debug CLI\n"
        << "Things to type:\n"
        << "* start     - Create an ActiveArea and start Magnum\n"
        << "* list_uni  - List Satellites in the universe\n"
        << "* list_ent  - List Entities in active scene\n"
        << "* list_upd  - List Update order from active scene\n"
        << "* help      - Show this again\n"
        << "* exit      - Deallocate everything and return memory to OS\n";
}

void debug_print_update_order()
{
    if (!g_ospMagnum)
    {
        std::cout << "Can't do that yet, start the magnum application first!\n";
        return;
    }

    osp::active::UpdateOrder &order = g_ospMagnum->get_scenes().begin()
                                            ->second.get_update_order();

    std::cout << "Update order:\n";
    for (auto call : order.get_call_list())
    {
        std::cout << "* " << call.m_name << "\n";
    }
}

void debug_print_hier()
{

    if (!g_ospMagnum)
    {
        std::cout << "Can't do that yet, start the magnum application first!\n";
        return;
    }

    std::cout << "ActiveScene Entity Hierarchy:\n";

    std::vector<active::ActiveEnt> parentNextSibling;
    active::ActiveScene &scene = g_ospMagnum->get_scenes().begin()->second;
    active::ActiveEnt currentEnt = scene.hier_get_root();

    parentNextSibling.reserve(16);

    while (true)
    {
        // print some info about the entity
        active::ACompHierarchy &hier = scene.reg_get<active::ACompHierarchy>(currentEnt);
        for (unsigned i = 0; i < hier.m_level; i ++)
        {
            // print arrows to indicate level
            std::cout << "  ->";
        }
        std::cout << "[" << int(currentEnt) << "]: " << hier.m_name << "\n";

        if (hier.m_childCount)
        {
            // entity has some children
            currentEnt = hier.m_childFirst;


            // save next sibling for later if it exists
            if (hier.m_siblingNext != entt::null)
            {
                parentNextSibling.push_back(hier.m_siblingNext);
            }
        }
        else if (hier.m_siblingNext != entt::null)
        {
            // no children, move to next sibling
            currentEnt = hier.m_siblingNext;
        }
        else if (parentNextSibling.size())
        {
            // last sibling, and not done yet
            // is last sibling, move to parent's (or ancestor's) next sibling
            currentEnt = parentNextSibling.back();
            parentNextSibling.pop_back();
        }
        else
        {
            break;
        }
    }
}

void debug_print_sats()
{

    universe::Universe &universe = g_osp.get_universe();

    auto view = universe.get_reg().view<universe::UCompTransformTraj,
                                        universe::UCompType>();

    for (universe::Satellite sat : view)
    {
        auto &posTraj = view.get<universe::UCompTransformTraj>(sat);
        auto &type = view.get<universe::UCompType>(sat);

        auto &pos = posTraj.m_position;

        std::cout << "SATELLITE: \"" << posTraj.m_name << "\" \n";
        if (type.m_type)
        {
            std::cout << " * Type: " << type.m_type->get_name() << "\n";
        }

        if (posTraj.m_trajectory)
        {
            std::cout << " * Trajectory: "
                      << posTraj.m_trajectory->get_type_name() << "\n";
        }

        std::cout << " * Position: ["
                  << pos.x() << ", " << pos.y() << ", " << pos.z() << "]\n";
    }


}
