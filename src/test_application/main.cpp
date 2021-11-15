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

#include "ActiveApplication.h"
#include "activescenes/scenarios.h"

#include "universes/simple.h"
#include "universes/planets.h"

#include <osp/Resource/AssetImporter.h>
#include <osp/string_concat.h>
#include <osp/logging.h>

#include <adera/ShipResources.h>
#include <adera/Shaders/PlumeShader.h>

// TODO: make a forward declaration for these, and maybe an osp::mach_name<>
//       instead of smc_mach_name. same thing for smc_wire_name
#include <adera/Machines/Container.h>
#include <adera/Machines/RCSController.h>
#include <adera/Machines/Rocket.h>
#include <adera/Machines/UserControl.h>

#include <Magnum/Primitives/Cube.h>
#include <Corrade/Utility/Arguments.h>

#include <iostream>
#include <memory>
#include <thread>
#include <unordered_map>

using namespace testapp;

/**
 * The spaghetti command line interface that gets inputs from stdin. This
 * function will only return once the user exits.
 * @return An error code maybe
 */
int debug_cli_loop();

void start_magnum_async();

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

// called only from commands to display information
void debug_print_help();
void debug_print_resources();
void debug_print_machines();

// Stores loaded resources in packages.
osp::PackageRegistry g_packages;

// Test application supports 1 Active Scene
entt::any g_activeScene;
std::function<void(ActiveApplication&)> g_appSetup;

// Test application supports 1 Universe
std::optional<UniverseScene> g_universeScene;

// Magnum Application deals with window and OpenGL things
std::optional<ActiveApplication> g_activeApplication;
std::thread g_magnumThread;

// Loggers
std::shared_ptr<spdlog::logger> g_logTestApp;
std::shared_ptr<spdlog::logger> g_logMagnumApp;

// lazily save the arguments to pass to Magnum
int g_argc;
char** g_argv;

// Scene options
struct TestScene
{
    std::string_view m_desc;
    void(*m_run)();
};

std::unordered_map<std::string_view, TestScene> const g_scenes
{
    {"enginetest", {"Demonstrate basic game engine functionality", [] {

        using namespace enginetest;
        g_activeScene = setup_scene(g_packages.find("lzdb"));

        g_appSetup = [] (ActiveApplication& rApp)
        {
            EngineTestScene& rScene
                    = entt::any_cast<EngineTestScene&>(g_activeScene);
            rApp.set_on_draw(gen_draw(rScene, rApp));
            load_gl_resources(*g_activeApplication);
        };
    }}}
};

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

    // Setup loggers
    auto const sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    sink->set_pattern("[%T.%e] [%n] [%^%l%$] [%s:%#] %v");
    g_logTestApp = std::make_shared<spdlog::logger>("testapp", sink);
    g_logMagnumApp = std::make_shared<spdlog::logger>("flight", sink);

    osp::set_thread_logger(g_logTestApp); // Set logger for this thread

    load_a_bunch_of_stuff();

    if(args.value("scene") != "none")
    {
        auto const it = g_scenes.find(args.value("scene"));

        if(it != std::end(g_scenes))
        {
            std::cerr << "unknown scene" << std::endl;
            exit(-1);
        }

        it->second.m_run();

        start_magnum_async();
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

        if (auto const it = g_scenes.find(command);
            it != std::end(g_scenes))
        {
            std::cout << "Loading scene: " << it->first << "\n";
            it->second.m_run();
            start_magnum_async();
        }
        else if (command == "help")
        {
            debug_print_help();
        }
        else if (command == "list_pkg")
        {
            debug_print_resources();
        }
        else if (command == "exit")
        {
            if (g_activeApplication)
            {
                // request exit if application exists
                g_activeApplication->exit();
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

void start_magnum_async()
{
    if (g_magnumThread.joinable())
    {
        g_magnumThread.join();
    }
    std::thread t([] {
        osp::set_thread_logger(g_logMagnumApp);

        g_activeApplication.emplace(ActiveApplication::Arguments{g_argc, g_argv});

        // Configure the controls
        config_controls(*g_activeApplication);

        osp::active::SysRenderGL::setup_context(
                    g_activeApplication->get_gl_resources());

        g_appSetup(*g_activeApplication);

        // Starts the main loop. This function is blocking, and will only return
        // once the window is closed. See ActiveApplication::drawEvent
        g_activeApplication->exec();

        OSP_LOG_INFO("Closed Magnum Application");

        g_activeApplication.reset();

    });
    g_magnumThread.swap(t);
}

bool destroy_universe()
{
    // Make sure no application is open
    if (g_activeApplication.has_value())
    {
        OSP_LOG_WARN("Application must be closed to destroy universe.");
        return false;
    }

    // Destroy all satellites
    //g_universe.get_reg().clear();
    //g_universe.coordspace_clear();

    // Destroy blueprints as part of destroying all vehicles
    g_packages.find("lzdb").clear<osp::BlueprintVehicle>();

    OSP_LOG_INFO("explosion* Universe destroyed!");

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
    osp::Package &rDebugPack = g_packages.create("lzdb");

    using adera::active::machines::MCompContainer;
    using adera::active::machines::MCompRCSController;
    using adera::active::machines::MCompRocket;
    using adera::active::machines::MCompUserControl;

    // Register machines
    register_machine<MCompContainer>(rDebugPack);
    register_machine<MCompRCSController>(rDebugPack);
    register_machine<MCompRocket>(rDebugPack);
    register_machine<MCompUserControl>(rDebugPack);

    // Register wire types
    register_wiretype<adera::wire::AttitudeControl>(rDebugPack);
    register_wiretype<adera::wire::Percent>(rDebugPack);

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
            osp::string_concat(datapath, meshName), rDebugPack, rDebugPack);
    }

    // Load noise textures
    const std::string noise256 = "noise256";
    const std::string noise1024 = "noise1024";
    const std::string n256path = osp::string_concat(datapath, noise256, ".png");
    const std::string n1024path = osp::string_concat(datapath, noise1024, ".png");

    osp::AssetImporter::load_image(n256path, rDebugPack);
    osp::AssetImporter::load_image(n1024path, rDebugPack);

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

    rDebugPack.add<ShipResourceType>("fuel", std::move(fuel));

    // Add a default cube
    rDebugPack.add<Magnum::Trade::MeshData>("cube", Magnum::Primitives::cubeSolid());

    OSP_LOG_INFO("Resource loading complete");
}

//-----------------------------------------------------------------------------

void debug_print_help()
{
    std::cout
        << "OSP-Magnum Temporary Debug CLI\n"
        << "Load a scene:\n";

    for (auto const& [name, rTestScn] : g_scenes)
    {
        std::cout << "* " << name << " - " << rTestScn.m_desc << "\n";
    }

    std::cout
        << "Other commands:\n"
        << "* list_pkg  - List Packages and Resources\n"
        << "* help      - Show this again\n"
        << "* exit      - Deallocate everything and return memory to OS\n";
}

template <typename RES_T>
void debug_print_resource_group(osp::Package const& rPkg)
{
    auto const pGroup = rPkg.group_get<RES_T>();

    if (pGroup == nullptr)
    {
        return;
    }

    std::cout << "  * TYPE: " << entt::type_name<RES_T>().value() << "\n";

    for (auto const& [key, resource] : *pGroup)
    {
        std::cout << "    * " << (resource.m_data.has_value() ? "LOADED" : "RESERVED") << ": " << key << "\n";
    }
}

void debug_print_package(osp::Package const& rPkg, osp::ResPrefix_t const& prefix)
{
    std::cout << "* PACKAGE: " << prefix << "\n";

    // TODO: maybe consider polymorphic access to resources?
    debug_print_resource_group<osp::PrototypePart>(rPkg);
    debug_print_resource_group<osp::BlueprintVehicle>(rPkg);

    debug_print_resource_group<Magnum::Trade::ImageData2D>(rPkg);
    debug_print_resource_group<Magnum::Trade::MeshData>(rPkg);
    debug_print_resource_group<Magnum::GL::Texture2D>(rPkg);
    debug_print_resource_group<Magnum::GL::Mesh>(rPkg);

    debug_print_resource_group<adera::active::machines::ShipResourceType>(rPkg);
}

void debug_print_resources()
{
    for (auto const& [prefix, pkg] : g_packages.get_map())
    {
        debug_print_package(pkg, prefix);
    }
}


