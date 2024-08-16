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

#include "Magnum/GL/DefaultFramebuffer.h"
#include "osp_drawing_gl/rendergl.h"

#include "testapp.h"
#include "scenarios.h"
#include "feature_interfaces.h"
#include "sessions/common.h"
#include "sessions/magnum.h"
#include "testapp/MagnumApplication.h"

#include <adera/application.h>

#include <osp/core/Resources.h>
#include <osp/core/string_concat.h>
#include <osp/drawing/own_restypes.h>
#include <osp/util/logging.h>
#include <osp/framework/builder.h>
#include <osp/framework/executor.h>
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
#include <iostream>
#include <memory>
#include <optional>
#include <string_view>
#include <unordered_map>

using namespace testapp;

using adera::MainLoopControl;
using adera::FrameworkModify;

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

std::optional<TestApp> g_testApp;

osp::fw::SingleThreadedExecutor g_executor;

osp::Logger_t g_mainThreadLogger;
osp::Logger_t g_logExecutor;
osp::Logger_t g_logMagnumApp;


using namespace osp::fw;

int g_argc;
char** g_argv;

struct MainLoopSignals
{
    osp::PipelineId mainLoop;
    osp::PipelineId inputs;
    osp::PipelineId renderSync;
    osp::PipelineId renderResync;
    osp::PipelineId sceneUpdate;
    osp::PipelineId sceneRender;
};

/**
 * @brief Runs Task/Pipeline main loop within MagnumApplication
 */
class CommonMagnumApp : public IOspApplication
{
public:
    CommonMagnumApp(TestApp &rTestApp, MainLoopControl &rMainLoopCtrl, MainLoopSignals signals) noexcept
     : m_rTestApp       { rTestApp }
     , m_rMainLoopCtrl  { rMainLoopCtrl }
     , m_signals        { signals }
    { }

    void run(MagnumApplication& rApp) override
    {
        // Start the main loop

//        PipelineId const mainLoop = m_rTestApp.m_application.get_pipelines<PlApplication>().mainLoop;
//        m_rTestApp.m_pExecutor->run(m_rTestApp, mainLoop);

//        // Resyncronize renderer

//        m_rMainLoopCtrl = MainLoopControl{
//            .doUpdate = false,
//            .doSync   = true,
//            .doResync = true,
//            .doRender = false,
//        };

//        signal_all();

//        m_rTestApp.m_pExecutor->wait(m_rTestApp);
    }

    void draw(MagnumApplication& rApp, float delta) override
    {
        // Magnum Application's main loop calls this
        m_rTestApp.drive_main_loop();

//        auto const magnum    = g_testApp->m_framework.get_interface<FIMagnum>   (g_testApp->m_mainContext);
//        auto       &rRenderGl = entt::any_cast<osp::draw::RenderGL&>  (g_testApp->m_framework.data[magnum.di.renderGl]);


//        Magnum::GL::defaultFramebuffer.bind();
//        auto &rFboColor = rRenderGl.m_texGl.get(rRenderGl.m_fboColor);
//        osp::draw::SysRenderGL::display_texture(rRenderGl, rFboColor);

//        m_rMainLoopCtrl = MainLoopControl{
//            .doUpdate = true,
//            .doSync   = true,
//            .doResync = false,
//            .doRender = true,
//        };

//        signal_all();

//        m_rTestApp.m_pExecutor->wait(m_rTestApp);
    }

    void exit(MagnumApplication& rApp) override
    {
//        m_rMainLoopCtrl = MainLoopControl{
//            .doUpdate = false,
//            .doSync   = false,
//            .doResync = false,
//            .doRender = false,
//        };

//        signal_all();

//        m_rTestApp.m_pExecutor->wait(m_rTestApp);

//        if (m_rTestApp.m_pExecutor->is_running(m_rTestApp))
//        {
//            // Main loop must have stopped, but didn't!
//            m_rTestApp.m_pExecutor->wait(m_rTestApp);
//            std::abort();
//        }
    }

private:

    void signal_all()
    {
//        m_rTestApp.m_pExecutor->signal(m_rTestApp, m_signals.mainLoop);
//        m_rTestApp.m_pExecutor->signal(m_rTestApp, m_signals.inputs);
//        m_rTestApp.m_pExecutor->signal(m_rTestApp, m_signals.renderSync);
//        m_rTestApp.m_pExecutor->signal(m_rTestApp, m_signals.renderResync);
//        m_rTestApp.m_pExecutor->signal(m_rTestApp, m_signals.sceneUpdate);
//        m_rTestApp.m_pExecutor->signal(m_rTestApp, m_signals.sceneRender);
    }

    TestApp         &m_rTestApp;
    MainLoopControl &m_rMainLoopCtrl;

    MainLoopSignals m_signals;
};

void eval_command(std::string_view const command, FrameworkModify &rFrameworkModify)
{
    // First check to see if command is the name of a scenario.
    auto const it = scenarios().find(command);
    if (it != std::end(scenarios()))
    {
        ScenarioOption const &rSro = it->second;

        rSro.loadFunc(g_testApp.value());
    }
    else // Otherwise check all other commands.
    {
        if (command == "help")
        {
            print_help();
        }
        else if (command == "reopen")
        {
            rFrameworkModify.commands.push_back({.func = [] (entt::any)
            {
                std::cout << "fish :3\n";

                auto const mainApp = g_testApp->m_framework.get_interface<FIMainApp>(g_testApp->m_mainContext);
                auto       &rAppCtxs = entt::any_cast<adera::AppContexts&>  (g_testApp->m_framework.data[mainApp.di.appContexts]);

                rAppCtxs.window = g_testApp->m_framework.contextIds.create();

                ContextBuilder   cb { .m_ctxScope = { g_testApp->m_mainContext}, .m_ctx = rAppCtxs.window, .m_rFW = g_testApp->m_framework };
                cb.add_feature(scenes::ftrWindowApp);
                cb.add_feature(scenes::ftrMagnum, MagnumApplication::Arguments{g_argc, g_argv});
                LGRN_ASSERTM(cb.m_errors.empty(), "Error adding main feature");
                ContextBuilder::apply(std::move(cb));

                main_loop_stack().push_back([] () -> bool {
                    auto &rTestApp = *g_testApp;

                    auto const mainApp   = g_testApp->m_framework.get_interface<FIMainApp>  (g_testApp->m_mainContext);
                    auto const &rAppCtxs = entt::any_cast<adera::AppContexts&>  (g_testApp->m_framework.data[mainApp.di.appContexts]);

                    auto const windowApp = g_testApp->m_framework.get_interface<FIWindowApp>(rAppCtxs.window);
                    auto const magnum    = g_testApp->m_framework.get_interface<FIMagnum>   (rAppCtxs.window);
                    auto       &rMainLoopCtrl = entt::any_cast<MainLoopControl&>  (g_testApp->m_framework.data[mainApp.di.mainLoopCtrl]);
                    auto       &rMagnumApp    = entt::any_cast<MagnumApplication&>(g_testApp->m_framework.data[magnum.di.magnumApp]);

                    MainLoopSignals const signals
                    {
                        .mainLoop     = mainApp     .pl.mainLoop,
                        .inputs       = windowApp   .pl.inputs,
                        .renderSync   = windowApp   .pl.sync,
                        .renderResync = windowApp   .pl.resync,
                        //.sceneUpdate  = scene                  .get_pipelines<PlScene>()         .update,
                        //.sceneRender  = sceneRenderer          .get_pipelines<PlSceneRenderer>() .render,
                    };
                    rMagnumApp.set_osp_app( std::make_unique<CommonMagnumApp>(g_testApp.value(), rMainLoopCtrl, signals) );

                    // Blocking. Main loop runs in MagnumApplication::drawEvent continues when the close button is pressed
                    rMagnumApp.exec();

                    rMagnumApp.set_osp_app({});

                    // Destructing MagnumApplication will close the window
                    g_testApp->m_framework.data[magnum.di.magnumApp].reset();

                    return false;
                });

            }});

        }
        else if (command == "list_pkg")
        {
            print_resources();
        }
        else if (command == "exit")
        {
            std::exit(0);
        }
        else
        {
            std::cout << "That command doesn't do anything ._.\n";
        }
    }
}

/**
 * Read-Eval-Print Loop
 */
auto const ftrREPL = feature_def("REPL", [] (FeatureBuilder& rFB, DependOn<FIMainApp> mainApp)
{
    rFB.task()
        .name       ("poll and evaluate commands")
        .run_on     ({mainApp.pl.mainLoop(EStgOptn::Run)})
        .sync_with  ({mainApp.pl.cin(EStgIntr::UseOrRun)})
        .args       ({                         mainApp.di.cin,        mainApp.di.frameworkModify})
        .func       ([] (std::vector<std::string> const &rCin, FrameworkModify &rFrameworkModify) noexcept
    {
        // Poll
        for (std::string const& str : rCin)
        {
            eval_command(str, rFrameworkModify);
        }
    });
});

int main(int argc, char** argv)
{
    g_argc = argc;
    g_argv = argv;
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

    if (args.isSet("log-exec"))
    {
        g_executor.m_log = g_logExecutor;
    }

    if(args.value("scene") != "none")
    {
        auto const it = scenarios().find(args.value("scene"));
        if(it == std::end(scenarios()))
        {
            OSP_LOG_ERROR("unknown scene");
            //g_testApp.clear_resource_owners();
            return 1;
        }

        //start_magnum_async(argc, argv);
    }

    register_stage_enums();

    NonBlockingStdInReader::instance().start_thread();

    TestApp &rTestApp = g_testApp.emplace();

    rTestApp.m_pExecutor = &g_executor;
    rTestApp.init();

    load_a_bunch_of_stuff();

    if( ! args.isSet("norepl"))
    {
        auto const fiMain         = rTestApp.m_framework.get_interface<FIMainApp>(rTestApp.m_mainContext);
        auto       &rFWModify     = entt::any_cast<FrameworkModify&>(rTestApp.m_framework.data[fiMain.di.frameworkModify]);
        rFWModify.commands.push_back({.func = [] (entt::any)
        {
            ContextBuilder cb { .m_ctx = g_testApp->m_mainContext, .m_rFW = g_testApp->m_framework };
            cb.add_feature(ftrREPL);
            LGRN_ASSERTM(cb.m_errors.empty(), "Error adding REPL feature");
            ContextBuilder::apply(std::move(cb));
            print_help();
        }});
    }

    std::vector<MainLoopFunc_t> &rMainLoopStack = main_loop_stack();
    rMainLoopStack.push_back( [] () -> bool
    {
        g_testApp->drive_main_loop();
        std::this_thread::sleep_for (std::chrono::milliseconds(100));
        return true;
    });

    // Main (thread) loop
    while ( ! rMainLoopStack.empty() )
    {
        MainLoopFunc_t func = rMainLoopStack.back();
        bool const keep = func();
        if ( ! keep )
        {
            rMainLoopStack.pop_back();
        }
    }

    spdlog::shutdown();
    return 0;
}

void load_a_bunch_of_stuff()
{
    using namespace osp::restypes;
    using namespace Magnum;
    using Primitives::ConeFlag;
    using Primitives::CylinderFlag;

    TestApp &rTestApp = g_testApp.value();

    auto const fiMain      = rTestApp.m_framework.get_interface<FIMainApp>(rTestApp.m_mainContext);
    auto       &rResources = entt::any_cast<osp::Resources&>(rTestApp.m_framework.data[fiMain.di.resources]);

    rResources.data_register<Trade::ImageData2D>(gc_image);
    rResources.data_register<Trade::TextureData>(gc_texture);
    rResources.data_register<osp::TextureImgSource>(gc_texture);
    rResources.data_register<Trade::MeshData>(gc_mesh);
    rResources.data_register<osp::ImporterData>(gc_importer);
    rResources.data_register<osp::Prefabs>(gc_importer);
    osp::register_tinygltf_resources(rResources);

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
        osp::ResId res = osp::load_tinygltf_file(osp::string_concat(datapath, meshName), rResources, rTestApp.m_defaultPkg);
        osp::assigns_prefabs_tinygltf(rResources, res);
    }

    // Add a default primitives
    auto const add_mesh_quick = [&rResources = rResources, &rTestApp] (std::string_view const name, Trade::MeshData&& data)
    {
        osp::ResId const meshId = rResources.create(gc_mesh, rTestApp.m_defaultPkg, osp::SharedString::create(name));
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
        std::cout << "* " << rName << spaces << " - " << rScenerio.description << "\n";
    }

    std::cout
        << "Other commands:\n"
        // << "* list_pkg  - List Packages and Resources\n"
        << "* help      - Show this again\n"
        << "* reopen    - Re-open Magnum Application\n"
        << "* exit      - Deallocate everything and return memory to OS\n";
}

void print_resources()
{
    // TODO: Add features to list resources in osp::Resources
    std::cout << "Not yet implemented!\n";
}
