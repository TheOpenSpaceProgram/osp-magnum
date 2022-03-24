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
#include "activescenes/common_scene.h"

#include "universes/simple.h"
#include "universes/planets.h"

#include <osp/Resource/resources.h>
#include <osp/string_concat.h>
#include <osp/logging.h>

#include <Magnum/MeshTools/Transform.h>
#include <Magnum/Primitives/Cylinder.h>
#include <Magnum/Primitives/Cube.h>
#include <Magnum/Primitives/Grid.h>
#include <Magnum/Primitives/Icosphere.h>
#include <Corrade/Utility/Arguments.h>

#include <iostream>
#include <memory>
#include <thread>
#include <unordered_map>

using namespace testapp;

/**
 * @brief Starts a spaghetti REPL line interface that gets inputs from stdin
 *
 * This interface can be used to run commands and load scenes
 *
 * @return An error code maybe
 */
int debug_cli_loop();

/**
 * @brief Starts Magnum application (ActiveApplication) thread g_magnumThread
 *
 * This initializes an OpenGL context, and opens the window
 */
void start_magnum_async();

/**
 * @brief As the name implies
 *
 * This should only be called once for the entire lifetime
 * of the program.
 *
 * prefer not to use names like this anywhere else but main.cpp
 */
void load_a_bunch_of_stuff();

/**
 * @brief Attempt to destroy everything in the universe
 */
bool destroy_universe();

// called only from commands to display information
void debug_print_help();
void debug_print_resources();

// Stores loaded resources
osp::Resources g_resources;
osp::PkgId g_defaultPkg;

// Test application supports 1 Active Scene
entt::any g_activeScene;
std::function<void(ActiveApplication&)> g_appSetup;

// Test application supports 1 Universe
std::shared_ptr<UniverseScene> g_universeScene;

// Magnum Application deals with window and OpenGL things
std::optional<ActiveApplication> g_activeApplication;
std::thread g_magnumThread;

// Loggers
std::shared_ptr<spdlog::logger> g_logTestApp;
std::shared_ptr<spdlog::logger> g_logMagnumApp;

// lazily save the arguments to pass to Magnum
int g_argc;
char** g_argv;

struct Option
{
    std::string_view m_desc;
    void(*m_run)();
};

std::unordered_map<std::string_view, Option> const g_scenes
{

    {"enginetest", {"Demonstrate basic game engine functionality", [] {

        using namespace enginetest;
        g_activeScene = setup_scene(g_resources, g_defaultPkg);

        g_appSetup = [] (ActiveApplication& rApp)
        {
            EngineTestScene& rScene
                    = entt::any_cast<EngineTestScene&>(g_activeScene);
            rApp.set_on_draw(generate_draw_func(rScene, rApp));
        };
    }}}
    ,
    {"physicstest", {"Physics lol", [] {

        g_activeScene.emplace<testapp::CommonTestScene>(g_resources);
        auto &rScene = entt::any_cast<CommonTestScene&>(g_activeScene);

        physicstest::setup_scene(rScene, g_defaultPkg);

        g_appSetup = [] (ActiveApplication& rApp)
        {
            auto& rScene
                    = entt::any_cast<CommonTestScene&>(g_activeScene);
            rApp.set_on_draw(physicstest::generate_draw_func(rScene, rApp));
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

        if(it == std::end(g_scenes))
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
            if (g_activeApplication.has_value())
            {
                // TODO: Figure out some way to reload the application while
                //       its still running.
                //       ie. Message it to destroy its GL resources and draw
                //           function, then load new scene
                std::cout << "Close application before openning new scene\n";
            }
            else
            {
                std::cout << "Loading scene: " << it->first << "\n";
                it->second.m_run();
                start_magnum_async();
            }
        }
        else if (command == "help")
        {
            debug_print_help();
        }
        else if (command == "reopen")
        {
            if (g_activeApplication.has_value())
            {
                std::cout << "Application is already open\n";
            }
            else if ( ! bool(g_activeScene) )
            {
                std::cout << "No existing scene loaded\n";
            }
            else
            {
                start_magnum_async();
            }

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

        // Setup GL resources
        osp::active::SysRenderGL::setup_context(g_activeApplication->get_render_gl());

        // Setup scene-specific renderer
        g_appSetup(*g_activeApplication);

        // Starts the main loop. This function is blocking, and will only return
        // once the window is closed. See ActiveApplication::drawEvent
        g_activeApplication->exec();

        OSP_LOG_INFO("Closed Magnum Application");

        osp::active::SysRenderGL::clear_resource_owners(g_activeApplication->get_render_gl(), g_resources);

        g_activeApplication.reset();

    });
    g_magnumThread.swap(t);
}

bool destroy_universe()
{
    // Make sure universe isn't in use anywhere else
    if (g_universeScene.unique())
    {
        OSP_LOG_WARN("Universe is still in use!");
        return false;
    }

    g_universeScene.reset();

    OSP_LOG_INFO("explosion* Universe destroyed!");

    return true;
}


void load_a_bunch_of_stuff()
{
    using namespace osp::restypes;
    using namespace Magnum;
    using Primitives::CylinderFlag;

    g_resources.resize_types(osp::resource_type_count());
    g_resources.data_register<Trade::MeshData>(gc_mesh);
    g_defaultPkg = g_resources.pkg_create();

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

    // TODO: Make new gltf loader. This will read gltf files and dump meshes,
    //       images, textures, and other relevant data into osp::Resources
//    for (auto meshName : meshes)
//    {
//        osp::AssetImporter::load_sturdy_file(
//            osp::string_concat(datapath, meshName), rDebugPack, rDebugPack);
//    }


    // Add a default primitives
    auto const add_mesh_quick = [] (std::string_view name, Trade::MeshData&& data)
    {
        osp::ResId const meshId = g_resources.create(gc_mesh, g_defaultPkg, name);
        g_resources.data_add<Trade::MeshData>(gc_mesh, meshId, std::forward<Trade::MeshData>(data));
    };

    add_mesh_quick("cube", Primitives::cubeSolid());
    add_mesh_quick("sphere", Primitives::icosphereSolid(2));
    add_mesh_quick("cylinder", Primitives::cylinderSolid(3, 16, 1.0f, CylinderFlag::CapEnds));
    add_mesh_quick("grid64solid", Primitives::grid3DSolid({63, 63}));

    OSP_LOG_INFO("Resource loading complete");
}

//-----------------------------------------------------------------------------

void debug_print_help()
{
    std::cout
        << "OSP-Magnum Temporary Debug CLI\n"
        << "Open a scene:\n";

    for (auto const& [name, rTestScn] : g_scenes)
    {
        std::cout << "* " << name << " - " << rTestScn.m_desc << "\n";
    }

    std::cout
        << "Other commands:\n"
        << "* list_pkg  - List Packages and Resources\n"
        << "* help      - Show this again\n"
        << "* reopen    - Re-open Magnum Application\n"
        << "* exit      - Deallocate everything and return memory to OS\n";
}

void debug_print_resources()
{
    // TODO: Add features to list resources in osp::Resources
    std::cout << "Not yet implemented!\n";
}


