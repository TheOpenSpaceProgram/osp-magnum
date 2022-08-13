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

#include <osp/Active/opengl/SysRenderGL.h>

#include <osp/Resource/load_tinygltf.h>
#include <osp/Resource/resources.h>
#include <osp/Resource/ImporterData.h>

#include <osp/tasks/tasks.h>
#include <osp/tasks/top_tasks.h>
#include <osp/tasks/top_execute.h>
#include <osp/tasks/execute_simple.h>

#include <osp/string_concat.h>
#include <osp/logging.h>

#include <osp/unpack.h>

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
 * @brief Setup a scene that uses CommonTestScene
 */
template <typename SCENE_T>
void setup_common_scene();

/**
 * @brief Attempt to destroy everything in the universe
 */
bool destroy_universe();

/**
 * @brief Deal with resource reference counts for a clean termination
 */
void clear_resource_owners();

// called only from commands to display information
void debug_print_help();
void debug_print_resources();

// Application state
osp::Tags               g_tags;
osp::Tasks              g_tasks;
osp::TopTaskDataVec_t   g_taskData;
std::vector<entt::any>  g_appTopData;
osp::ExecutionContext   g_exec;

osp::Session g_application;
osp::Session g_magnum;
osp::Session g_scene;
osp::Session g_sceneRender;

// Stores loaded resources in g_appTopData
osp::PkgId g_defaultPkg;

// Magnum Application deals with window and OpenGL things
std::thread g_magnumThread;

// Called when openning a Magnum Application
std::function<void()> g_appSetup;

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

static ActiveApplication& get_magnum_application()
{
    return entt::any_cast<ActiveApplication&>(g_appTopData.at(std::size_t(g_magnum.m_dataIds[0])));
}

static osp::Resources& get_resources()
{
    return entt::any_cast<osp::Resources&>(g_appTopData.at(std::size_t(g_application.m_dataIds[0])));
}

std::unordered_map<std::string_view, Option> const g_scenes
{

    {"enginetest", {"Demonstrate basic game engine functionality", [] {
        osp::TopDataId const mdiScene = osp::top_reserve(g_appTopData);
        g_scene.m_dataIds = {mdiScene};
        g_appTopData[mdiScene] = enginetest::setup_scene(get_resources(), g_defaultPkg);

        g_appSetup = [] ()
        {
            auto& rScene = osp::top_get<enginetest::EngineTestScene>(g_appTopData, g_scene.m_dataIds[0]);

            auto const [idActiveApp, idRenderGl, idUserInput] = osp::unpack<3>(g_magnum.m_dataIds);
            auto &rActiveApp    = osp::top_get<ActiveApplication>(g_appTopData, idActiveApp);
            auto &rRenderGl     = osp::top_get<osp::active::RenderGL>(g_appTopData, idRenderGl);
            auto &rUserInput    = osp::top_get<osp::input::UserInputHandler>(g_appTopData, idUserInput);

            rActiveApp.set_on_draw(enginetest::generate_draw_func(rScene, rActiveApp, rRenderGl, rUserInput));
        };
    }}}
    ,
    {"physicstest", {"Physics lol", [] {
        setup_common_scene<scenes::PhysicsTest>();
    }}},
    {"vehicletest", {"Vehicle and glTF", [] {
        //setup_common_scene<scenes::VehicleTest>();
    }}},
    {"universetest", {"Vehicle and glTF", [] {
        //setup_common_scene<scenes::UniverseTest>();
    }}}
};


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
        auto const it = g_scenes.find(args.value("scene"));
        if(it == std::end(g_scenes))
        {
            std::cerr << "unknown scene" << std::endl;
            clear_resource_owners();
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

void close_session(osp::Session &rSession)
{
    osp::top_close_session(g_tags, g_tasks, g_taskData, g_appTopData, g_exec, rSession);
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
        if (auto const it = g_scenes.find(command);
            it != std::end(g_scenes))
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
                // close existing scene
                close_session(g_scene);
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
            if (magnumOpen)
            {
                std::cout << "Application is already open\n";
            }
            else if ( ! bool(g_appSetup) )
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
                // request exit if application exists
                get_magnum_application().exit();
            }
            destroy_universe();
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

        auto &rDataIds = g_magnum.m_dataIds;
        rDataIds.resize(3);
        osp::top_reserve(g_appTopData, 0, std::begin(rDataIds), std::end(rDataIds));
        auto const [idActiveApp, idRenderGl, idUserInput] = osp::unpack<3>(rDataIds);
        g_magnum.m_tags.resize(4);
        g_tags.m_tags.create(std::begin(g_magnum.m_tags), std::end(g_magnum.m_tags));

        // Order-dependent; ActiveApplication construction starts OpenGL context
        auto &rUserInput    = osp::top_emplace<osp::input::UserInputHandler>(g_appTopData, idUserInput, 12);
        auto &rActiveApp    = osp::top_emplace<ActiveApplication>(g_appTopData, idActiveApp, ActiveApplication::Arguments{g_argc, g_argv}, rUserInput);
        auto &rRenderGl     = osp::top_emplace<osp::active::RenderGL>(g_appTopData, idRenderGl);

        // Configure the controls
        config_controls(rUserInput);

        // Setup GL resources
        osp::active::SysRenderGL::setup_context(rRenderGl);

        rActiveApp.set_on_destruct([&rRenderGl, idRenderGl = idRenderGl] ()
        {
            osp::active::SysRenderGL::clear_resource_owners(rRenderGl, get_resources());
            g_appTopData[std::size_t(idRenderGl)].reset();
        });

        // Setup scene-specific renderer
        g_appSetup();

        // Starts the main loop. This function is blocking, and will only return
        // once the window is closed. See ActiveApplication::drawEvent
        rActiveApp.exec();

        close_session(g_sceneRender);
        close_session(g_magnum);

        OSP_LOG_INFO("Closed Magnum Application");
    });
    g_magnumThread.swap(t);
}

template <typename SCENE_T>
void setup_common_scene()
{
    MainView mainView
    {
        .m_topData      = g_appTopData,
        .m_rTags        = g_tags,
        .m_rTasks       = g_tasks,
        .m_rTaskData    = g_taskData,
        .m_idResources  = g_application.m_dataIds[0]
    };

    g_scene.m_dataIds.clear();
    g_scene.m_dataIds.resize(24);
    osp::top_reserve(g_appTopData, 0, std::begin(g_scene.m_dataIds), std::end(g_scene.m_dataIds));

    g_scene.m_tags.resize(64);
    g_tags.m_tags.create(std::begin(g_scene.m_tags), std::end(g_scene.m_tags));

    SCENE_T::setup_scene(mainView, g_defaultPkg, g_scene);

    // Renderer and draw function is created when g_appSetup is invoked
    g_appSetup = [mainView] () mutable
    {
        auto const [idActiveApp, idRenderGl, idUserInput] = osp::unpack<3>(g_magnum.m_dataIds);
        auto &rActiveApp = osp::top_get<ActiveApplication>(g_appTopData, idActiveApp);

        auto &rDataIds = g_sceneRender.m_dataIds;
        rDataIds.resize(16);
        osp::top_reserve(g_appTopData, 0, std::begin(rDataIds), std::end(rDataIds));

        g_sceneRender.m_tags.resize(24);
        g_tags.m_tags.create(std::begin(g_sceneRender.m_tags), std::end(g_sceneRender.m_tags));

        SCENE_T::setup_renderer_gl(mainView, g_application, g_scene, g_magnum, g_sceneRender);

        auto const [tgRenderEvt, tgInputEvt, tgGlUse] = osp::unpack<3>(g_magnum.m_tags);
        auto const [tgCleanupEvt, tgResyncEvt, tgSyncEvt, tgSceneEvt, tgTimeEvt] = osp::unpack<5>(g_scene.m_tags);

        g_scene.m_onCleanup = tgCleanupEvt;

        if ( ! osp::debug_top_verify(g_tags, g_tasks, g_taskData))
        {
            get_magnum_application().exit();
            OSP_LOG_INFO("Errors detected, scene closed.");
            return;
        }

        osp::top_enqueue_quick(g_tags, g_tasks, g_exec, {tgSyncEvt, tgResyncEvt});
        osp::top_run_blocking(g_tags, g_tasks, g_taskData, g_appTopData, g_exec);

        auto const runTags = {tgSyncEvt, tgSceneEvt, tgTimeEvt, tgRenderEvt, tgInputEvt};

        rActiveApp.set_on_draw( [runTagsVec = std::vector<osp::TagId>(runTags)] (ActiveApplication& rApp, float delta)
        {
            osp::top_enqueue_quick(g_tags, g_tasks, g_exec, runTagsVec);
            osp::top_run_blocking(g_tags, g_tasks, g_taskData, g_appTopData, g_exec);
        });
    };
}

bool destroy_universe()
{
    // Make sure universe isn't in use anywhere else

    OSP_LOG_INFO("explosion* Universe destroyed!");

    return true;
}

using Corrade::Containers::arrayView;

void load_a_bunch_of_stuff()
{
    using namespace osp::restypes;
    using namespace Magnum;
    using Primitives::CylinderFlag;

    std::size_t const maxTags = 128; // aka: just two 64-bit integers
    std::size_t const maxTagsInts = maxTags / 64;

    g_tags.m_tags.reserve(maxTags);
    g_tags.m_tagDepends.resize(maxTags * g_tags.m_tagDependsPerTag,
                               lgrn::id_null<osp::TagId>());
    g_tags.m_tagLimits.resize(maxTagsInts);
    g_tags.m_tagExtern.resize(maxTagsInts);
    g_tags.m_tagEnqueues.resize(maxTags, lgrn::id_null<osp::TagId>());

    g_application.m_dataIds = { osp::top_reserve(g_appTopData) };

    auto &rResources = osp::top_emplace<osp::Resources>(g_appTopData, g_application.m_dataIds[0]);

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

    add_mesh_quick("cube", Primitives::cubeSolid());
    add_mesh_quick("sphere", Primitives::icosphereSolid(2));
    add_mesh_quick("cylinder", Primitives::cylinderSolid(3, 16, 1.0f, CylinderFlag::CapEnds));
    add_mesh_quick("grid64solid", Primitives::grid3DSolid({63, 63}));

    OSP_LOG_INFO("Resource loading complete");
}

using each_res_id_t = void(*)(osp::ResId);

static void resource_for_each_type(osp::ResTypeId const type, each_res_id_t const do_thing)
{
    auto &rResources = osp::top_get<osp::Resources>(g_appTopData, g_application.m_dataIds[0]);
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
        auto &rResources = osp::top_get<osp::Resources>(g_appTopData, g_application.m_dataIds[0]);

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
        auto &rResources = osp::top_get<osp::Resources>(g_appTopData, g_application.m_dataIds[0]);

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


