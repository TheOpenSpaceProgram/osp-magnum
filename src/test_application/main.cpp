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

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "OSPMagnum.h"
#include "flight.h"

#include "universes/simple.h"
#include "universes/planets.h"

#include <osp/Resource/AssetImporter.h>
#include <osp/string_concat.h>

#include <osp/Satellites/SatVehicle.h>

#include <adera/ShipResources.h>


#include <osp/Shaders/Phong.h>
#include <adera/Shaders/PlumeShader.h>

#include <adera/Machines/Container.h>
#include <adera/Machines/RCSController.h>
#include <adera/Machines/Rocket.h>
#include <adera/Machines/UserControl.h>

#include <planet-a/Satellites/SatPlanet.h>

#include <Corrade/Utility/Arguments.h>

#include <memory>
#include <thread>
#include <iostream>

using namespace testapp;

void config_controls();

/**
 * As the name implies. This should only be called once for the entire lifetime
 * of the program.
 *
 * prefer not to use names like this anywhere else but main.cpp
 */
void load_a_bunch_of_stuff();

/**
 * Try to everything in the universe
 */
bool destroy_universe();

/**
 * The spaghetti command line interface that gets inputs from stdin. This
 * function will only return once the user exits.
 * @return An error code maybe
 */
int debug_cli_loop();

// called only from commands to display information
void debug_print_help();
void debug_print_resources();
void debug_print_sats();
void debug_print_hier();
void debug_print_machines();

// Deals with the underlying OSP universe, with the satellites and stuff. A
// Magnum application or OpenGL context is not required for the universe to
// exist. This also stores loaded resources in packages.
osp::OSPApplication g_osp;

// Deals with the window, OpenGL context, and other game engine stuff that
// often have "Active" written all over them
std::unique_ptr<OSPMagnum> g_ospMagnum;
std::thread g_magnumThread;

// lazily save the arguments to pass to Magnum
int g_argc;
char** g_argv;

int main(int argc, char** argv)
{
    Corrade::Utility::Arguments args;
    args.addSkippedPrefix("magnum", "Magnum options")
        .addOption("scene", "simple").setHelp("scene", "Set the scene to launch")
        .addOption("config").setHelp("config", "path to configuration file to use")
        .addBooleanOption("norepl").setHelp("norepl", "don't enter read, evaluate, print, loop.")
        .addBooleanOption('v', "verbose").setHelp("verbose", "log verbosely")
        .setGlobalHelp("Helptext goes here.")
        .parse(argc, argv);

    // just lazily save the arguments
    g_argc = argc;
    g_argv = argv;

    load_a_bunch_of_stuff();

    if(args.value("scene") != "none")
    {
        if(args.value("scene") == "simple")
        {
            simplesolarsystem::create(g_osp);
        }
        else if(args.value("scene") == "moon")
        {
            moon::create(g_osp);
        }
        else
        {
            std::cerr << "unknown scene" << std::endl;
            exit(-1);
        }

        if (g_magnumThread.joinable())
        {
            g_magnumThread.join();
        }
        std::thread t(test_flight, std::ref(g_ospMagnum), std::ref(g_osp),
                      OSPMagnum::Arguments{g_argc, g_argv});
        g_magnumThread.swap(t);
    }

    if(!args.isSet("norepl"))
    {
        // start doing debug cli loop
        debug_cli_loop();
    }

    // wait for magnum thread to exit if it exists
    if (g_magnumThread.joinable())
    {
        g_magnumThread.join();
    }

    //Kill spdlog
    spdlog::shutdown();  //>_> -> X.X  *Stab 
    return 0;
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
        else if (command == "simple")
        {
            if (destroy_universe())
            {
                simplesolarsystem::create(g_osp);
            }
        }
        else if (command == "moon")
        {
            if (destroy_universe())
            {
                moon::create(g_osp);
            }
        }
        else if (command == "flight")
        {
            if (g_magnumThread.joinable())
            {
                g_magnumThread.join();
            }
            std::thread t(test_flight, std::ref(g_ospMagnum), std::ref(g_osp),
                          OSPMagnum::Arguments{g_argc, g_argv});
            g_magnumThread.swap(t);
        }
        else if (command == "list_pkg")
        {
            debug_print_resources();
        }
        else if (command == "list_uni")
        {
            debug_print_sats();
        }
        else if (command == "list_ent")
        {
            debug_print_hier();
        }
        else if (command == "list_mach")
        {
            debug_print_machines();
        }
        else if (command == "exit")
        {
            if (g_ospMagnum)
            {
                // request exit if application exists
                g_ospMagnum->exit();
            }
            destroy_universe();
            break;
        }
        else
        {
            std::cout << "that doesn't do anything ._.\n";
        }
    }

    return 0;
}

bool destroy_universe()
{
    // Make sure no application is open
    if (g_ospMagnum != nullptr)
    {
      SPDLOG_LOGGER_WARN(g_osp.get_logger(),
                         "Application must be closed to destroy universe.");
        return false;
    }

    // Destroy all satellites
    g_osp.get_universe().get_reg().clear();

    g_osp.get_universe().coordspace_clear();

    // Destroy blueprints as part of destroying all vehicles
    g_osp.debug_find_package("lzdb").clear<osp::BlueprintVehicle>();

    SPDLOG_LOGGER_INFO(g_osp.get_logger(), "explosion* Universe destroyed!");

    return true;
}

// TODO: move this somewhere else
template<typename MACH_T>
constexpr void register_machine(osp::Package &rPkg)
{
    rPkg.add<osp::RegisteredMachine>(std::string(MACH_T::smc_mach_name),
                                     osp::mach_id<MACH_T>());
}

// TODO: move this somewhere else
template<typename WIRETYPE_T>
constexpr void register_wiretype(osp::Package &rPkg)
{
    rPkg.add<osp::RegisteredWiretype>(std::string(WIRETYPE_T::smc_wire_name),
                                      osp::wiretype_id<WIRETYPE_T>());
}


void load_a_bunch_of_stuff()
{
    // Create a new package
    osp::Package lazyDebugPack("lzdb", "lazy-debug");

    using adera::active::machines::MachineContainer;
    using adera::active::machines::MachineRCSController;
    using adera::active::machines::MachineRocket;
    using adera::active::machines::MachineUserControl;

    // Register machines
    register_machine<MachineContainer>(lazyDebugPack);
    register_machine<MachineRCSController>(lazyDebugPack);
    register_machine<MachineRocket>(lazyDebugPack);
    register_machine<MachineUserControl>(lazyDebugPack);

    // Register wire types
    register_wiretype<adera::wire::AttitudeControl>(lazyDebugPack);
    register_wiretype<adera::wire::Percent>(lazyDebugPack);

    // Load sturdy glTF files
    const std::string_view datapath = {"OSPData/adera/"};
    const std::vector<std::string_view> meshes = 
    {
        "spamcan.sturdy.gltf",
        "stomper.sturdy.gltf",
        "ph_capsule.sturdy.gltf",
        "ph_fuselage.sturdy.gltf",
        "ph_engine.sturdy.gltf",
        "ph_plume.sturdy.gltf",
        "ph_rcs.sturdy.gltf",
        "ph_rcs_plume.sturdy.gltf"
    };
    for (auto meshName : meshes)
    {
        osp::AssetImporter::load_sturdy_file(
            osp::string_concat(datapath, meshName), lazyDebugPack, lazyDebugPack);
    }

    // Load noise textures
    const std::string noise256 = "noise256";
    const std::string noise1024 = "noise1024";
    const std::string n256path = osp::string_concat(datapath, noise256, ".png");
    const std::string n1024path = osp::string_concat(datapath, noise1024, ".png");

    osp::AssetImporter::load_image(n256path, lazyDebugPack);
    osp::AssetImporter::load_image(n1024path, lazyDebugPack);

    // Load placeholder fuel type
    using adera::active::machines::ShipResourceType;
    ShipResourceType fuel
    {
        "fuel",        // identifier
        "Rocket fuel", // display name
        1 << 16,       // quanta per unit
        1.0f,          // volume per unit (m^3)
        1000.0f,       // mass per unit (kg)
        1000.0f        // density (kg/m^3)
    };

    lazyDebugPack.add<ShipResourceType>("fuel", std::move(fuel));

    // Add package to the univere
    g_osp.debug_add_package(std::move(lazyDebugPack));

    // Add 50 vehicles so there's something to load
    //g_osp.get_universe().get_sats().reserve(64);

    //s_partsLoaded = true;

    SPDLOG_LOGGER_INFO(g_osp.get_logger(), "Resource loading complete");
}

void debug_print_help()
{
    std::cout
        << "OSP-Magnum Temporary Debug CLI\n"
        << "Choose a test universe:\n"
        << "* simple    - Simple test planet and vehicles (default)\n"
        << "* moon      - Simulate size and gravity of real world moon\n"
        << "\n"
        << "Start Application:\n"
        << "* flight    - Create an ActiveArea and start Magnum\n"
        << "\n"
        << "Other things to type:\n"
        << "* list_pkg  - List Packages and Resources\n"
        << "* list_uni  - List Satellites in the universe\n"
        << "* list_ent  - List Entities in active scene\n"
        << "* list_upd  - List Update order from active scene\n"
        << "* help      - Show this again\n"
        << "* exit      - Deallocate everything and return memory to OS\n";
}

template <typename RES_T>
void debug_print_resource_group(osp::Package& rPkg)
{
    osp::Package::group_t<RES_T> const &group = rPkg.group_get<RES_T>();

    if (group.empty())
    {
        return;
    }

    std::cout << "  * TYPE: " << entt::type_name<RES_T>().value() << "\n";

    for (auto const& [key, resource] : group)
    {
        std::cout << "    * " << (resource.m_data.has_value() ? "LOADED" : "RESERVED") << ": " << key << "\n";
    }
}

void debug_print_resources()
{
    std::vector<osp::Package*> packages = {
        &g_osp.debug_find_package("lzdb")};

    if (g_ospMagnum != nullptr)
    {
        packages.push_back(&g_ospMagnum->get_context_resources());
    }

    for (osp::Package* pPkg : packages)
    {
        osp::Package &rPkg = *pPkg;
        std::cout << "* PACKAGE: " << rPkg.get_prefix() << "\n";

        debug_print_resource_group<osp::PrototypePart>(rPkg);
        debug_print_resource_group<osp::BlueprintVehicle>(rPkg);

        debug_print_resource_group<Magnum::Trade::ImageData2D>(rPkg);
        debug_print_resource_group<Magnum::Trade::MeshData>(rPkg);
        debug_print_resource_group<Magnum::GL::Texture2D>(rPkg);
        debug_print_resource_group<Magnum::GL::Mesh>(rPkg);

        debug_print_resource_group<adera::active::machines::ShipResourceType>(rPkg);
        debug_print_resource_group<adera::shader::PlumeShader>(rPkg);
        debug_print_resource_group<osp::shader::Phong>(rPkg);
    }
}

void debug_print_machines()
{
    using osp::active::ACompHierarchy;
    using osp::active::ActiveScene;
    using osp::active::ActiveEnt;
    using osp::active::ACompMachines;
    using osp::active::ACompMachineType;
    using osp::RegisteredMachine;
    using regmachs_t = osp::Package::group_t<RegisteredMachine>;

    if (!g_ospMagnum)
    {
        std::cout << "Can't do that yet, start the magnum application first!\n";
        return;
    }

    // Get list of machine names
    regmachs_t const &group = g_osp.debug_find_package("lzdb").group_get<RegisteredMachine>();
    std::vector<std::string_view> machNames(group.size());
    for (auto const& [name, regMach] : group)
    {
        machNames[regMach.m_data->m_id] = name;
    }

    // Loop through every Vehicle
    ActiveScene const &scene = g_ospMagnum->get_scenes().begin()->second.first;
    auto view = scene.get_registry().view<const ACompMachines>();

    for (ActiveEnt ent : view)
    {
        auto const *name = scene.get_registry().try_get<osp::active::ACompName>(ent);
        std::string_view nameview = (name != nullptr) ? std::string_view(name->m_name) : "untitled";
        std::cout << "[" << int(ent) << "]: " << nameview << "\n";

        // Loop through each of that vehicle's Machines
        auto const &machines = scene.reg_get<ACompMachines>(ent);
        for (ActiveEnt machEnt : machines.m_machines)
        {
            auto const& type = scene.reg_get<ACompMachineType>(machEnt);

            std::cout << "  ->[" << int(machEnt) << "]: " << machNames[type.m_type] << "\n";
        }
    }
}

void debug_print_hier()
{
    using osp::active::ACompHierarchy;
    using osp::active::ActiveScene;
    using osp::active::ActiveEnt;

    if (!g_ospMagnum)
    {
        std::cout << "Can't do that yet, start the magnum application first!\n";
        return;
    }

    std::cout << "ActiveScene Entity Hierarchy:\n";

    std::vector<ActiveEnt> parentNextSibling;
    ActiveScene const &scene = g_ospMagnum->get_scenes().begin()->second.first;
    ActiveEnt currentEnt = scene.hier_get_root();

    parentNextSibling.reserve(16);

    while (true)
    {
        // print some info about the entitysize() != 0
        auto const &hier = scene.reg_get<ACompHierarchy>(currentEnt);
        for (uint8_t i = 0; i < hier.m_level; i ++)
        {
            // print arrows to indicate level
            std::cout << "  ->";
        }
        auto const *name = scene.get_registry().try_get<osp::active::ACompName>(currentEnt);
        std::string_view nameview = (name != nullptr) ? std::string_view(name->m_name) : "untitled";
        std::cout << "[" << uint32_t(scene.get_registry().entity(currentEnt))
                     << "]: " << nameview << "\n";

        if (hier.m_childCount != 0)
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
        else if (!parentNextSibling.empty())
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
    using osp::universe::Universe;
    using osp::universe::UCompTransformTraj;
    using osp::universe::UCompInCoordspace;
    using osp::universe::UCompCoordspaceIndex;
    using osp::universe::CoordinateSpace;

    Universe const &rUni = g_osp.get_universe();

    Universe::Reg_t const &rReg = rUni.get_reg();

    rReg.each([&rReg, &rUni] (osp::universe::Satellite sat)
    {
        auto const &posTraj = rReg.get<const UCompTransformTraj>(sat);
        auto const &inCoord = rReg.get<const UCompInCoordspace>(sat);
        auto const &coordIndex = rReg.get<const UCompCoordspaceIndex>(sat);

        CoordinateSpace const &rSpace
                = rUni.coordspace_get(inCoord.m_coordSpace);

        osp::universe::Vector3g const pos{
            rSpace.ccomp_view<osp::universe::CCompX>()[coordIndex.m_myIndex],
            rSpace.ccomp_view<osp::universe::CCompY>()[coordIndex.m_myIndex],
            rSpace.ccomp_view<osp::universe::CCompZ>()[coordIndex.m_myIndex]
        };

        std::cout << "* SATELLITE: \"" << posTraj.m_name << "\"\n";

        rReg.visit(sat, [&rReg] (entt::type_info info) {
            Universe::Reg_t::poly_storage storage = rReg.storage(info);
            std::cout << "  * UComp: " << storage->value_type().name() << "\n";
        });

        auto posM = osp::Vector3(pos) / 1024.0f;
        std::cout << "  * Position: ["
                  << pos.x() << ", " << pos.y() << ", " << pos.z() << "], ["
                  << posM.x() << ", " << posM.y() << ", " << posM.z()
                  << "] meters\n";
    });


}
