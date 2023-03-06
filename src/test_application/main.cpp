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

#include "osp/tasks/top_utils.h"
#include <spdlog/sinks/stdout_color_sinks.h>

#include "ActiveApplication.h"
#include "activescenes/scenarios.h"
#include "activescenes/identifiers.h"
#include "activescenes/scene_renderer.h"

#include <osp/Resource/load_tinygltf.h>
#include <osp/Resource/resources.h>
#include <osp/Resource/ImporterData.h>

#include <osp/tasks/tasks.h>
#include <osp/tasks/top_tasks.h>
#include <osp/tasks/top_execute.h>
#include <osp/tasks/execute_simple.h>

#include <osp/string_concat.h>
#include <osp/logging.h>

#include <Magnum/MeshTools/Transform.h>
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

#include <array>
#include <iostream>
#include <memory>
#include <string_view>
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
 * @brief Deal with resource reference counts for a clean termination
 */
void clear_resource_owners();

// called only from commands to display information
void debug_print_help();
void debug_print_resources();

// TopData stores most application state, addressed using a TopDataId
std::vector<entt::any>  g_appTopData;

// TopTasks and are organized with Tags to form task graphs and events.
// Each TopTask is given a vector of TopDataIds its allowed to access
osp::Tasks              g_tasks;
osp::TopTaskDataVec_t   g_taskData;
osp::Tags               g_tags;

// Current execution state of TopTasks
// g_tasks, g_taskData, and g_tags stay constant during execution
osp::ExecutionContext   g_exec;

// Sessions bundle together and own TopDataIds, TopTaskIds, and TagsIds
// Sessions intend to add support for something to exist in the world
// eg, Adding support for physics or supporting a certain shader

// Application Session lasts the lifetime of the program.
// It stores the Resources class, addressable by a TopDataId
osp::Session            g_application;
osp::TopDataId          g_idResources{lgrn::id_null<osp::TopDataId>()};

// Default Package in Resources
osp::PkgId              g_defaultPkg;

// Sessions that make up the current scene
osp::Sessions_t         g_sceneSessions;

// Sessions for rendering. These only exist when the Magnum Application is open
osp::Session            g_magnum;
osp::Sessions_t         g_renderSessions;

// Magnum Application deals with window and OpenGL things
std::thread             g_magnumThread;

// Called when openning a Magnum Application
RendererSetup_t         g_rendererSetup;

// Loggers
std::shared_ptr<spdlog::logger> g_logTestApp;
std::shared_ptr<spdlog::logger> g_logMagnumApp;

// lazily save the arguments to pass to Magnum
int g_argc;
char** g_argv;

static void close_sessions(osp::Sessions_t &rSessions)
{
    osp::top_close_session(g_tags, g_tasks, g_taskData, g_appTopData, g_exec, rSessions);
    rSessions.clear();
}

static void close_session(osp::Session &rSession)
{
    osp::top_close_session(g_tags, g_tasks, g_taskData, g_appTopData, g_exec, osp::ArrayView<osp::Session>(&rSession, 1));
}

static MainView get_main_view()
{
    return {
        .m_topData      = g_appTopData,
        .m_rTags        = g_tags,
        .m_rTasks       = g_tasks,
        .m_rExec        = g_exec,
        .m_rTaskData    = g_taskData,
        .m_idResources  = g_idResources,
        .m_defaultPkg   = g_defaultPkg
    };
}


int main(int argc, char** argv)
{
    Corrade::Utility::Arguments args;
    args.addSkippedPrefix("magnum", "Magnum options")
        .addOption("scene", "none").setHelp("scene", "Set the scene to launch")
        .addOption("config").setHelp("config", "path to configuration file to use")
        .addBooleanOption("norepl").setHelp("norepl", "don't enter read, evaluate, print, loop.")
        .addBooleanOption('v', "verbose").setHelp("verbose", "log verbosely")
        .setGlobalHelp("Helptext goes here.")
        .parse(argc, argv);

    // just lazily save the arguments
    g_argc = argc;
    g_argv = argv;

    // Setup loggers
    {
        auto pSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        pSink->set_pattern("[%T.%e] [%n] [%^%l%$] [%s:%#] %v");
        g_logTestApp = std::make_shared<spdlog::logger>("testapp", pSink);
        g_logMagnumApp = std::make_shared<spdlog::logger>("flight", std::move(pSink));
    }

    osp::set_thread_logger(g_logTestApp); // Set logger for this thread

    g_appTopData.resize(64);
    load_a_bunch_of_stuff();

    if(args.value("scene") != "none")
    {
        auto const it = scenarios().find(args.value("scene"));
        if(it == std::end(scenarios()))
        {
            std::cerr << "unknown scene" << std::endl;
            clear_resource_owners();
            exit(-1);
        }

        g_rendererSetup = it->second.m_setup(get_main_view(), g_sceneSessions);

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

        bool magnumOpen = ! g_magnum.m_dataIds.empty();
        if (auto const it = scenarios().find(command);
            it != std::end(scenarios()))
        {
            if (magnumOpen)
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

                close_sessions(g_sceneSessions); // Close existing scene

                g_rendererSetup = it->second.m_setup(get_main_view(), g_sceneSessions);
                start_magnum_async();
            }
        }
        else if (command == "help")
        {
            debug_print_help();
        }
        else if (command == "reopen")
        {
            if (magnumOpen)
            {
                std::cout << "Application is already open\n";
            }
            else if ( ! bool(g_rendererSetup) )
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
            if (magnumOpen)
            {
                // Request exit if application exists
                OSP_SESSION_UNPACK_DATA(g_magnum, TESTAPP_APP_MAGNUM); // declares idActiveApp
                osp::top_get<ActiveApplication>(g_appTopData, idActiveApp).exit();
            }

            break;
        }
        else
        {
            std::cout << "that doesn't do anything ._.\n";
        }
    }

    clear_resource_owners();

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

        // Start Magnum application session
        Builder_t builder{g_tags, g_tasks, g_taskData};
        g_magnum = scenes::setup_magnum_application(builder, g_appTopData, g_tags, g_idResources, {g_argc, g_argv});
        OSP_SESSION_UNPACK_DATA(g_magnum, TESTAPP_APP_MAGNUM); // declares idActiveApp
        auto &rActiveApp = osp::top_get<ActiveApplication>(g_appTopData, idActiveApp);

        // Setup renderer sessions
        g_rendererSetup(get_main_view(), g_magnum, g_sceneSessions, g_renderSessions);

        // Starts the main loop. This function is blocking, and will only return
        // once the window is closed. See ActiveApplication::drawEvent
        rActiveApp.exec();

        // Destruct draw function lambda first
        // EngineTest stores the entire renderer in here (if it's active)
        rActiveApp.set_on_draw({});
        
        // Closing sessions will delete their associated TopData and Tags
        close_sessions(g_renderSessions);
        close_session(g_magnum);

        OSP_LOG_INFO("Closed Magnum Application");
    });
    g_magnumThread.swap(t);
}



void load_a_bunch_of_stuff()
{
    using namespace osp::restypes;
    using namespace Magnum;
    using Primitives::CylinderFlag;

    std::size_t const maxTags = 256; // aka: just two 64-bit integers
    std::size_t const maxTagsInts = maxTags / 64;

    g_tags.m_tags.reserve(maxTags);
    g_tags.m_tagDepends.resize(maxTags * g_tags.m_tagDependsPerTag,
                               lgrn::id_null<osp::TagId>());
    g_tags.m_tagLimits.resize(maxTagsInts);
    g_tags.m_tagExtern.resize(maxTagsInts);


    auto const [idResources] = g_application.acquire_data<1>(g_appTopData);
    g_idResources = idResources;

    auto &rResources = osp::top_emplace<osp::Resources>(g_appTopData, idResources);

    rResources.resize_types(osp::ResTypeIdReg_t::size());

    rResources.data_register<Trade::ImageData2D>(gc_image);
    rResources.data_register<Trade::TextureData>(gc_texture);
    rResources.data_register<osp::TextureImgSource>(gc_texture);
    rResources.data_register<Trade::MeshData>(gc_mesh);
    rResources.data_register<osp::ImporterData>(gc_importer);
    rResources.data_register<osp::Prefabs>(gc_importer);
    osp::register_tinygltf_resources(rResources);
    g_defaultPkg = rResources.pkg_create();

    // Load sturdy glTF files
    const std::string_view datapath = {"OSPData/adera/"};
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
        osp::ResId res = osp::load_tinygltf_file(osp::string_concat(datapath, meshName), rResources, g_defaultPkg);
        osp::assigns_prefabs_tinygltf(rResources, res);
    }

    // Add a default primitives
    auto const add_mesh_quick = [&rResources = rResources] (std::string_view const name, Trade::MeshData&& data)
    {
        osp::ResId const meshId = rResources.create(gc_mesh, g_defaultPkg, osp::SharedString::create(name));
        rResources.data_add<Trade::MeshData>(gc_mesh, meshId, std::move(data));
    };

    Trade::MeshData &&cylinder = Magnum::MeshTools::transform3D( Primitives::cylinderSolid(3, 16, 1.0f, CylinderFlag::CapEnds), Matrix4::rotationX(Deg(90)), 0);

    add_mesh_quick("cube", Primitives::cubeSolid());
    add_mesh_quick("cubewire", Primitives::cubeWireframe());
    add_mesh_quick("sphere", Primitives::icosphereSolid(2));
    add_mesh_quick("cylinder", std::move(cylinder));
    add_mesh_quick("grid64solid", Primitives::grid3DSolid({63, 63}));

    OSP_LOG_INFO("Resource loading complete");
}

using each_res_id_t = void(*)(osp::ResId);

static void resource_for_each_type(osp::ResTypeId const type, each_res_id_t const do_thing)
{
    auto &rResources = osp::top_get<osp::Resources>(g_appTopData, g_idResources);
    lgrn::IdRegistry<osp::ResId> const &rReg = rResources.ids(type);
    for (std::size_t i = 0; i < rReg.capacity(); ++i)
    {
        if (rReg.exists(osp::ResId(i)))
        {
            do_thing(osp::ResId(i));
        }
    }
}

void clear_resource_owners()
{
    using namespace osp::restypes;

    // Texture resources contain osp::TextureImgSource, which refererence counts
    // their associated image data
    resource_for_each_type(gc_texture, [] (osp::ResId const id)
    {
        auto &rResources = osp::top_get<osp::Resources>(g_appTopData, g_idResources);

        auto * const pData = rResources
                .data_try_get<osp::TextureImgSource>(gc_texture, id);
        if (pData != nullptr)
        {
            rResources.owner_destroy(gc_image, std::move(*pData));
        }
    });

    // Importer data own a lot of other resources
    resource_for_each_type(gc_importer, [] (osp::ResId const id)
    {
        auto &rResources = osp::top_get<osp::Resources>(g_appTopData, g_idResources);

        auto * const pData = rResources
                .data_try_get<osp::ImporterData>(gc_importer, id);
        if (pData != nullptr)
        {
            for (osp::ResIdOwner_t &rOwner : std::move(pData->m_images))
            {
                rResources.owner_destroy(gc_image, std::move(rOwner));
            }

            for (osp::ResIdOwner_t &rOwner : std::move(pData->m_textures))
            {
                rResources.owner_destroy(gc_texture, std::move(rOwner));
            }

            for (osp::ResIdOwner_t &rOwner : std::move(pData->m_meshes))
            {
                rResources.owner_destroy(gc_mesh, std::move(rOwner));
            }
        }
    });
}

//-----------------------------------------------------------------------------

void debug_print_help()
{

    std::size_t longestName = 0;
    for (auto const& [name, rTestScn] : scenarios())
    {
        longestName = std::max(name.size(), longestName);
    }

    std::cout
        << "OSP-Magnum Temporary Debug CLI\n"
        << "Open a scenario:\n";

    for (auto const& [name, rTestScn] : scenarios())
    {
        std::string spaces(longestName - name.length(), ' ');
        std::cout << "* " << name << spaces << " - " << rTestScn.m_desc << "\n";
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


