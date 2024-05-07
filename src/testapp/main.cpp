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

#include "MagnumApplication.h"
#include "testapp.h"
#include "scenarios.h"
#include "identifiers.h"
#include "sessions/common.h"
#include "sessions/magnum.h"

#include <osp/core/Resources.h>
#include <osp/core/string_concat.h>
#include <osp/drawing/own_restypes.h>
#include <osp/tasks/top_execute.h>
#include <osp/util/logging.h>
#include <osp/vehicles/ImporterData.h>
#include <osp/vehicles/load_tinygltf.h>

#include <Magnum/MeshTools/Transform.h>
#include <Magnum/Primitives/Cone.h>
#include <Magnum/Primitives/Cylinder.h>
#include <Magnum/Primitives/Cube.h>
#include <Magnum/Primitives/Grid.h>
#include <Magnum/Primitives/Icosphere.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/TextureData.h>
#include <Magnum/Trade/MeshData.h>

#include <Corrade/Containers/ArrayViewStl.h>
#include <Corrade/Utility/Arguments.h>

#include <entt/core/any.hpp>

#include <spdlog/sinks/stdout_color_sinks.h>

#include <array>
#include <fstream>
#include <iostream>
#include <memory>
#include <string_view>
#include <thread>
#include <unordered_map>

using namespace testapp;

/**
 * @brief Starts a spaghetti REPL (Read Evaluate Print Loop) interface that gets inputs from standard in
 *
 * This interface can be used to run commands and load scenes
 *
 * CLI -> Command line interface
 */
void cli_loop(int& argc, char** argv);

/**
 * @brief Starts Magnum application (MagnumApplication) thread g_magnumThread
 *
 * This initializes an OpenGL context, and opens the window
 */
void start_magnum_async(int& argc, char** argv);

/**
 * @brief As the name implies
 *
 * This should only be called once for the entire lifetime
 * of the program.
 *
 * prefer not to use names like this outside of testapp
 */
void load_a_bunch_of_stuff();

// called only from commands to display information
void print_help();
void print_resources();

TestApp g_testApp;

SingleThreadedExecutor g_executor;

std::thread g_magnumThread;

// Loggers
osp::Logger_t g_mainThreadLogger;
osp::Logger_t g_logExecutor;
osp::Logger_t g_logMagnumApp;

int main(int argc, char** argv)
{
    // Command line argument parsing
    Corrade::Utility::Arguments args;
    args.addSkippedPrefix("magnum", "Magnum options")
        .addOption("scene", "none")         .setHelp("scene",       "Set the scene to launch")
        .addOption("config")                .setHelp("config",      "path to configuration file to use")
        .addBooleanOption("norepl")         .setHelp("norepl",      "don't enter read, evaluate, print, loop.")
        .addBooleanOption("log-exec")       .setHelp("log-exec",    "Log Task/Pipeline Execution (Extremely chatty!)")
        // TODO .addBooleanOption('v', "verbose")   .setHelp("verbose",     "log verbosely")
        .setGlobalHelp("Helptext goes here.")
        .parse(argc, argv);

    // Setup logging
    auto pSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    pSink->set_pattern("[%T.%e] [%n] [%^%l%$] [%s:%#] %v");
    g_mainThreadLogger = std::make_shared<spdlog::logger>("main-thread", pSink);
    g_logExecutor  = std::make_shared<spdlog::logger>("executor", pSink);
    g_logMagnumApp = std::make_shared<spdlog::logger>("flight", std::move(pSink));

    // Set thread-local logger used by OSP_LOG_* macros
    osp::set_thread_logger(g_mainThreadLogger);

    g_testApp.m_pExecutor = &g_executor;

    if (args.isSet("log-exec"))
    {
        g_executor.m_log = g_logExecutor;
    }

    g_testApp.m_topData.resize(64);
    load_a_bunch_of_stuff();

    if(args.value("scene") != "none")
    {
        auto const it = scenarios().find(args.value("scene"));
        if(it == std::end(scenarios()))
        {
            OSP_LOG_ERROR("unknown scene");
            g_testApp.clear_resource_owners();
            return 1;
        }

        g_testApp.m_rendererSetup = it->second.m_setup(g_testApp);

        start_magnum_async(argc, argv);
    }

    if( ! args.isSet("norepl"))
    {
        cli_loop(argc, argv);
    }

    // Wait for magnum thread to exit if it exists.
    if (g_magnumThread.joinable())
    {
        g_magnumThread.join();
    }

    spdlog::shutdown();
    return 0;
}

void cli_loop(int& argc, char** argv)
{
    print_help();

    std::string command;

    while(true)
    {
        std::cout << "> ";
        std::cin >> command;

        bool const magnumOpen = ! g_testApp.m_renderer.m_sessions.empty();
        
        // First check to see if command is the name of a scenario.
        if (auto const it = scenarios().find(command); 
            it != std::end(scenarios()))
        {
            if (magnumOpen)
            {
                // TODO: Figure out some way to reload the application while
                //       its still running.
                //       ie. Message it to destroy its GL resources and draw
                //           function, then load new scene
                std::cout << "Close application before opening new scene\n";
            }
            else
            {
                std::cout << "Loading scene: " << it->first << "\n";

                // Close existing scene first
                if ( ! g_testApp.m_scene.m_sessions.empty() )
                {
                    g_testApp.close_sessions(g_testApp.m_scene.m_sessions);
                    g_testApp.m_scene.m_sessions.clear();
                    g_testApp.m_scene.m_edges.m_syncWith.clear();
                }

                g_testApp.m_rendererSetup = it->second.m_setup(g_testApp);
                start_magnum_async(argc, argv);
            }
        }
        else // Otherwise check all other commands. 
        { 
            if (command == "help") 
            {
                print_help();
            }
            else if (command == "reopen") 
            {
                if (magnumOpen)
                {
                    std::cout << "Application is already open\n";
                }
                else if (g_testApp.m_rendererSetup == nullptr)
                {
                    std::cout << "No existing scene loaded\n";
                }
                else
                {
                    start_magnum_async(argc, argv);
                }
            }
            else if (command == "list_pkg") 
            {
                print_resources();
            }
            else if (command == "exit") 
            {
                if (magnumOpen)
                {
                    OSP_DECLARE_GET_DATA_IDS(g_testApp.m_renderer.m_sessions[1], TESTAPP_DATA_MAGNUM); // declares idActiveApp
                    osp::top_get<MagnumApplication>(g_testApp.m_topData, idActiveApp).exit();

                    break;
                }
            }
            else 
            {
                std::cout << "That command doesn't do anything ._.\n";
            }
        }
    }

    g_testApp.clear_resource_owners();
}

void start_magnum_async(int& argc, char** argv)
{
    if (g_magnumThread.joinable())
    {
        g_magnumThread.join();
    }
    std::thread t([&argc, argv] {
        osp::set_thread_logger(g_logMagnumApp);

        // Start Magnum application session
        osp::TopTaskBuilder builder{g_testApp.m_tasks, g_testApp.m_renderer.m_edges, g_testApp.m_taskData};

        g_testApp.m_windowApp   = scenes::setup_window_app  (builder, g_testApp.m_topData, g_testApp.m_application);
        g_testApp.m_magnum      = scenes::setup_magnum      (builder, g_testApp.m_topData, g_testApp.m_application, g_testApp.m_windowApp, MagnumApplication::Arguments{ argc, argv });

        OSP_DECLARE_GET_DATA_IDS(g_testApp.m_magnum, TESTAPP_DATA_MAGNUM); // declares idActiveApp
        auto &rActiveApp = osp::top_get<MagnumApplication>(g_testApp.m_topData, idActiveApp);

        // Setup renderer sessions

        g_testApp.m_rendererSetup(g_testApp);

        g_testApp.m_graph = osp::make_exec_graph(g_testApp.m_tasks, {&g_testApp.m_renderer.m_edges, &g_testApp.m_scene.m_edges});
        g_executor.load(g_testApp);

        // Starts the main loop. This function is blocking, and will only return
        // once the window is closed. See MagnumApplication::drawEvent
        rActiveApp.exec();

        // Destruct draw function lambda first
        // EngineTest stores the entire renderer in here (if it's active)
        rActiveApp.set_osp_app({});
        
        // Closing sessions will delete their associated TopData and Tags
        g_testApp.m_pExecutor->run(g_testApp, g_testApp.m_windowApp.m_cleanup);
        g_testApp.close_sessions(g_testApp.m_renderer.m_sessions);
        g_testApp.m_renderer.m_sessions.clear();
        g_testApp.m_renderer.m_edges.m_syncWith.clear();

        g_testApp.close_session(g_testApp.m_magnum);
        g_testApp.close_session(g_testApp.m_windowApp);

        OSP_LOG_INFO("Closed Magnum Application");
    });
    g_magnumThread.swap(t);
}

void load_a_bunch_of_stuff()
{
    using namespace osp::restypes;
    using namespace Magnum;
    using Primitives::ConeFlag;
    using Primitives::CylinderFlag;

    osp::TopTaskBuilder builder{g_testApp.m_tasks, g_testApp.m_applicationGroup.m_edges, g_testApp.m_taskData};
    auto const plApp = g_testApp.m_application.create_pipelines<PlApplication>(builder);

    builder.pipeline(plApp.mainLoop).loops(true).wait_for_signal(EStgOptn::ModifyOrSignal);

    // declares idResources and idMainLoopCtrl
    OSP_DECLARE_CREATE_DATA_IDS(g_testApp.m_application, g_testApp.m_topData, TESTAPP_DATA_APPLICATION);

    auto &rResources = osp::top_emplace<osp::Resources> (g_testApp.m_topData, idResources);
    /* unused */       osp::top_emplace<MainLoopControl>(g_testApp.m_topData, idMainLoopCtrl);

    builder.task()
        .name       ("Schedule Main Loop")
        .schedules  ({plApp.mainLoop(EStgOptn::Schedule)})
        .push_to    (g_testApp.m_application.m_tasks)
        .args       ({                  idMainLoopCtrl})
        .func([] (MainLoopControl const& rMainLoopCtrl) noexcept -> osp::TaskActions
    {
        if (   ! rMainLoopCtrl.doUpdate
            && ! rMainLoopCtrl.doSync
            && ! rMainLoopCtrl.doResync
            && ! rMainLoopCtrl.doRender)
        {
            return osp::TaskAction::Cancel;
        }
        else
        {
            return { };
        }
    });

    rResources.resize_types(osp::ResTypeIdReg_t::size());

    rResources.data_register<Trade::ImageData2D>(gc_image);
    rResources.data_register<Trade::TextureData>(gc_texture);
    rResources.data_register<osp::TextureImgSource>(gc_texture);
    rResources.data_register<Trade::MeshData>(gc_mesh);
    rResources.data_register<osp::ImporterData>(gc_importer);
    rResources.data_register<osp::Prefabs>(gc_importer);
    osp::register_tinygltf_resources(rResources);
    g_testApp.m_defaultPkg = rResources.pkg_create();

    // Load sturdy glTF files
    const std::string_view datapath = { "OSPData/adera/" };
    const std::vector<std::string_view> meshes =
    {
        "spamcan.sturdy.gltf",
        "stomper.sturdy.gltf",
        "ph_capsule.sturdy.gltf",
        "ph_fuselage.sturdy.gltf",
        "ph_engine.sturdy.gltf",
        //"ph_plume.sturdy.gltf",
        "ph_rcs.sturdy.gltf"
        //"ph_rcs_plume.sturdy.gltf"
    };

    // TODO: Make new gltf loader. This will read gltf files and dump meshes,
    //       images, textures, and other relevant data into osp::Resources
    for (auto const& meshName : meshes)
    {
        osp::ResId res = osp::load_tinygltf_file(osp::string_concat(datapath, meshName), rResources, g_testApp.m_defaultPkg);
        osp::assigns_prefabs_tinygltf(rResources, res);
    }

    // Add a default primitives
    auto const add_mesh_quick = [&rResources = rResources] (std::string_view const name, Trade::MeshData&& data)
    {
        osp::ResId const meshId = rResources.create(gc_mesh, g_testApp.m_defaultPkg, osp::SharedString::create(name));
        rResources.data_add<Trade::MeshData>(gc_mesh, meshId, std::move(data));
    };

    Trade::MeshData &&cylinder = Magnum::MeshTools::transform3D( Primitives::cylinderSolid(3, 16, 1.0f, CylinderFlag::CapEnds), Matrix4::rotationX(Deg(90)), 0);
    Trade::MeshData &&cone = Magnum::MeshTools::transform3D( Primitives::coneSolid(3, 16, 1.0f, ConeFlag::CapEnd), Matrix4::rotationX(Deg(90)), 0);

    add_mesh_quick("cube", Primitives::cubeSolid());
    add_mesh_quick("cubewire", Primitives::cubeWireframe());
    add_mesh_quick("sphere", Primitives::icosphereSolid(2));
    add_mesh_quick("cylinder", std::move(cylinder));
    add_mesh_quick("cone", std::move(cone));
    add_mesh_quick("grid64solid", Primitives::grid3DSolid({63, 63}));

    OSP_LOG_INFO("Resource loading complete");
}

//-----------------------------------------------------------------------------

void print_help()
{
    std::size_t longestName = 0;
    for (auto const& [rName, rScenerio] : scenarios())
    {
        longestName = std::max(rName.size(), longestName);
    }

    std::cout
        << "OSP-Magnum Temporary Debug CLI\n"
        << "Open a scenario:\n";

    for (auto const& [rName, rScenerio] : scenarios())
    {
        std::string spaces(longestName - rName.length(), ' ');
        std::cout << "* " << rName << spaces << " - " << rScenerio.m_description << "\n";
    }

    std::cout
        << "Other commands:\n"
        << "* list_pkg  - List Packages and Resources\n"
        << "* help      - Show this again\n"
        << "* reopen    - Re-open Magnum Application\n"
        << "* exit      - Deallocate everything and return memory to OS\n";
}

void print_resources()
{
    // TODO: Add features to list resources in osp::Resources
    std::cout << "Not yet implemented!\n";
}
