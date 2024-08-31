#include "flying_scene.h"

#include "scenarios.h"
#include "feature_interfaces.h"
#include "sessions/godot.h"
#include "spdlog/pattern_formatter.h"
#include "spdlog/sinks/callback_sink.h"

#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/resource_saver.hpp>
#include <godot_cpp/classes/surface_tool.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector2.hpp>

#include <adera_app/feature_interfaces.h>
#include <adera_app/application.h>
#include <adera_app/features/common.h>
#include <adera_app/features/jolt.h>
#include <adera_app/features/misc.h>
#include <adera_app/features/physics.h>
#include <adera_app/features/shapes.h>
#include <adera_app/features/terrain.h>
#include <adera_app/features/universe.h>
#include <adera_app/features/vehicles.h>
#include <adera_app/features/vehicles_machines.h>

#include <osp/core/Resources.h>
#include <osp/core/string_concat.h>
#include <osp/drawing/own_restypes.h>
#include <osp/framework/builder.h>
#include <osp/util/UserInputHandler.h>
#include <osp/util/logging.h>
#include <osp/vehicles/ImporterData.h>
#include <osp/vehicles/load_tinygltf.h>

#include <Corrade/Utility/Debug.h>
#include <Magnum/MeshTools/Transform.h>
#include <Magnum/Primitives/Cone.h>
#include <Magnum/Primitives/Cube.h>
#include <Magnum/Primitives/Cylinder.h>
#include <Magnum/Primitives/Grid.h>
#include <Magnum/Primitives/Icosphere.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/TextureData.h>

#include <spdlog/sinks/stdout_color_sinks.h>

#include <string_view>

using namespace adera;
using namespace ftr_inter;
using namespace godot;
using namespace osp::draw;
using namespace osp::fw;
using namespace ospgdext;

void FlyingScene::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("get_scene"), &FlyingScene::get_scene);
    ClassDB::bind_method(D_METHOD("set_scene", "scene"), &FlyingScene::set_scene);
    ClassDB::add_property(
        "FlyingScene", PropertyInfo(Variant::STRING, "scene"), "set_scene", "get_scene");
}

FlyingScene::FlyingScene()
{
    // setup the Debug thingies
    new Corrade::Utility::Debug{ &m_dbgStream };
    new Corrade::Utility::Warning{ &m_warnStream };
    new Corrade::Utility::Error{ &m_errStream };
}

FlyingScene::~FlyingScene()
{
    //delete (ExecutorType *)m_pExecutor;
}

osp::Logger_t g_mainThreadLogger;


class GodotLogSink final : public spdlog::sinks::base_sink<spdlog::details::null_mutex>
{
protected:
    void sink_it_(spdlog::details::log_msg const &msg) override
    {
        spdlog::memory_buf_t formatBuf;
        this->formatter_->format(msg, formatBuf);
        formatBuf.push_back('\x00');

        // kinda stupid that print functions only support godot::String, which is utf-32, and
        // godot just converts it back to utf-8 to print to the terminal.
        godot::String gdStr{formatBuf.begin()};
        switch (msg.level)
        {
            case spdlog::level::err:
                godot::UtilityFunctions::printerr(std::move(gdStr));
                break;
            default:
                godot::UtilityFunctions::print(std::move(gdStr));
                break;
        }
    }
    void flush_() override { }
};

void FlyingScene::_enter_tree() // practically main()?
{
    auto pSink = std::make_shared<GodotLogSink>();
    pSink->set_pattern("[%T.%e] [%n] [%^%l%$] [%s:%#] %v");
    g_mainThreadLogger = std::make_shared<spdlog::logger>("main-thread", pSink);
    osp::set_thread_logger(g_mainThreadLogger);


    OSP_LOG_INFO("Enter tree");
    register_stage_enums();

    m_mainContext = m_framework.m_contextIds.create();

    ContextBuilder mainCB { m_mainContext, {}, m_framework };
    mainCB.add_feature(ftrMain);
    ContextBuilder::finalize(std::move(mainCB));

    auto const fiMain         = m_framework.get_interface<FIMainApp>(m_mainContext);
    auto       &rResources    = m_framework.data_get<osp::Resources&>(fiMain.di.resources);
    rResources.resize_types(osp::ResTypeIdReg_t::size());
    m_defaultPkg = rResources.pkg_create();


    load_a_bunch_of_stuff();
    OSP_LOG_INFO("Resources loaded");

    RenderingServer *renderingServer = RenderingServer::get_singleton();
    m_scenario                       = get_world_3d()->get_scenario();
    m_viewport                       = get_viewport()->get_viewport_rid();

    m_lightInstance                  = renderingServer->instance_create();
    renderingServer->instance_set_scenario(m_lightInstance, m_scenario);

    RID light = renderingServer->directional_light_create();
    renderingServer->light_set_distance_fade(light, false, 0., 0., 0.);
    renderingServer->light_set_shadow(light, false);
    renderingServer->instance_set_base(m_lightInstance, light);

    Transform3D lform = Transform3D(Basis().rotated(Vector3(1, 1, 1), -1.), Vector3(0., 0., 0.));
    renderingServer->instance_set_transform(m_lightInstance, lform);

    OSP_LOG_INFO("Created viewport, scenario, and light");

    CharString const utf8 = m_scene.utf8();
    OSP_LOG_INFO("Scene is {}", utf8.ptr());
    auto const it = scenarios().find("vehicles" /*m_scene.utf8().get_data()*/);
    if ( it == std::end(scenarios()) )
    {
        OSP_LOG_INFO("Unknown scene");
        clear_resource_owners();
        return;
    }
    ScenarioOption const &rSelectedScenario = it->second;

    // Loads data into the framework; contains nothing godot-related
    rSelectedScenario.loadFunc(m_framework, m_mainContext, m_defaultPkg);


}

void FlyingScene::_ready()
{
    // Setup godot-related stuff based on whatever features the scenario loaded into the framework


    setup_app();
}

void FlyingScene::_physics_process(double delta)
{
    // ospjolt::SysJolt::update_world() update the world
}

void FlyingScene::_process(double delta)
{
    auto const mainApp  = m_framework.get_interface<FIMainApp>(m_mainContext);
    auto const& appCtxs = m_framework.data_get<AppContexts>(mainApp.di.appContexts);
    auto const windowApp = m_framework.get_interface<FIWindowApp>(appCtxs.window);
    auto &rUserInput = m_framework.data_get<UserInputHandler>(windowApp.di.userInput);

    rUserInput.update_controls();
    draw_event();
    rUserInput.clear_events();
    // print the corrade messages
    if ( m_dbgStream.tellp() != std::streampos(0) )
    {
        godot::UtilityFunctions::print(m_dbgStream.str().data());
        m_dbgStream.str("");
    }
    if ( m_warnStream.tellp() != std::streampos(0) )
    {
        godot::UtilityFunctions::print(m_warnStream.str().data());
        m_warnStream.str("");
    }
    if ( m_errStream.tellp() != std::streampos(0) )
    {
        godot::UtilityFunctions::print(m_errStream.str().data());
        m_errStream.str("");
    }
}

void FlyingScene::_exit_tree()
{
    destroy_app();
}

void FlyingScene::drive_scene_cycle(UpdateParams p)
{
    Framework &rFW = m_framework;

    auto const mainApp          = rFW.get_interface<FIMainApp>  (m_mainContext);
    auto const &rAppCtxs        = rFW.data_get<AppContexts>     (mainApp.di.appContexts);
    auto       &rMainLoopCtrl   = rFW.data_get<MainLoopControl> (mainApp.di.mainLoopCtrl);
    rMainLoopCtrl.doUpdate      = p.update;

    auto const scene            = rFW.get_interface<FIScene>    (rAppCtxs.scene);
    if (scene.id.has_value())
    {
        auto       &rSceneLoopCtrl  = rFW.data_get<SceneLoopControl>(scene.di.loopControl);
        auto       &rDeltaTimeIn    = rFW.data_get<float>           (scene.di.deltaTimeIn);
        rSceneLoopCtrl.doSceneUpdate = p.sceneUpdate;
        rDeltaTimeIn                = p.deltaTimeIn;
    }

    auto const windowApp        = rFW.get_interface<FIWindowApp>      (rAppCtxs.window);
    auto       &rWindowLoopCtrl = rFW.data_get<WindowAppLoopControl>  (windowApp.di.windowAppLoopCtrl);
    rWindowLoopCtrl.doRender    = p.render;
    rWindowLoopCtrl.doSync      = p.sync;
    rWindowLoopCtrl.doResync    = p.resync;

    m_executor.signal(m_framework, mainApp.pl.mainLoop);
    m_executor.signal(m_framework, windowApp.pl.inputs);
    m_executor.signal(m_framework, windowApp.pl.sync);
    m_executor.signal(m_framework, windowApp.pl.resync);

    m_executor.wait(m_framework);
}

void FlyingScene::run_context_cleanup(ContextId ctx)
{
    auto const cleanup = m_framework.get_interface<FICleanupContext> (ctx);
    if ( cleanup.id.has_value() )
    {
        // Run cleanup pipeline for the window context
        m_executor.run(m_framework, cleanup.pl.cleanup);
        m_executor.wait(m_framework);

        if (m_executor.is_running(m_framework))
        {
            OSP_LOG_CRITICAL("Deadlock in cleanup pipeline");
            std::abort();
        }
    }
}

void FlyingScene::clear_resource_owners()
{
    using namespace osp::restypes;

    auto const mainApp = m_framework.get_interface<FIMainApp>(m_mainContext);

    auto &rResources = m_framework.data_get<osp::Resources>(mainApp.di.resources);

    // Texture resources contain osp::TextureImgSource, which refererence counts
    // their associated image data
    for (osp::ResId const id : rResources.ids(gc_texture))
    {
        auto * const pData = rResources.data_try_get<osp::TextureImgSource>(gc_texture, id);
        if (pData != nullptr)
        {
            rResources.owner_destroy(gc_image, std::move(*pData));
        }
    };

    // Importer data own a lot of other resources
    for (osp::ResId const id : rResources.ids(gc_importer))
    {
        auto * const pData = rResources.data_try_get<osp::ImporterData>(gc_importer, id);
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
    };
}

void FlyingScene::load_a_bunch_of_stuff()
{
    using namespace osp::restypes;
    using namespace Magnum;
    using Primitives::ConeFlag;
    using Primitives::CylinderFlag;

    auto const fiMain      = m_framework.get_interface<FIMainApp>(m_mainContext);
    auto       &rResources = m_framework.data_get<osp::Resources&>(fiMain.di.resources);
    rResources.data_register<Trade::ImageData2D>(gc_image);
    rResources.data_register<Trade::TextureData>(gc_texture);
    rResources.data_register<osp::TextureImgSource>(gc_texture);
    rResources.data_register<Trade::MeshData>(gc_mesh);
    rResources.data_register<osp::ImporterData>(gc_importer);
    rResources.data_register<osp::Prefabs>(gc_importer);
    osp::register_tinygltf_resources(rResources);
    // Load sturdy glTF files
    // FIXME this works in editor, but probably not for exported game.
    const std::string_view              datapath = { "OSPData/adera/" };
    const std::vector<std::string_view> meshes   = {
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
    for ( auto const &meshName : meshes )
    {
        auto str = osp::string_concat(datapath, meshName);
        osp::ResId res = osp::load_tinygltf_file(str, rResources, m_defaultPkg);
        if (res != lgrn::id_null<ResId>())
        {
            osp::assigns_prefabs_tinygltf(rResources, res);
        }
    }

    // Add a default primitives
    auto const add_mesh_quick = [&rResources = rResources, this] (std::string_view const name, Trade::MeshData&& data)
    {
        osp::ResId const meshId = rResources.create(gc_mesh, m_defaultPkg, osp::SharedString::create(name));
        rResources.data_add<Trade::MeshData>(gc_mesh, meshId, std::move(data));
    };

    Trade::MeshData &&cylinder = Magnum::MeshTools::transform3D(
        Primitives::cylinderSolid(3, 16, 1.0f, CylinderFlag::CapEnds),
        Matrix4::rotationX(Deg(90)),
        0);
    Trade::MeshData &&cone = Magnum::MeshTools::transform3D(
        Primitives::coneSolid(3, 16, 1.0f, ConeFlag::CapEnd), Matrix4::rotationX(Deg(90)), 0);

    add_mesh_quick("cube", Primitives::cubeSolid());
    add_mesh_quick("cubewire", Primitives::cubeWireframe());
    add_mesh_quick("sphere", Primitives::icosphereSolid(2));
    add_mesh_quick("cylinder", std::move(cylinder));
    add_mesh_quick("cone", std::move(cone));
    add_mesh_quick("grid64solid", Primitives::grid3DSolid({ 63, 63 }));

    OSP_LOG_INFO("Resource loading complete");
}

ContextId make_scene_renderer(Framework &rFW, ContextId mainCtx, ContextId sceneCtx, ContextId windowCtx, PkgId defaultPkg)
{
    auto const godot = rFW.get_interface<FIGodot>         (windowCtx);

    ContextId const scnRdrCtx = rFW.m_contextIds.create();

    // TODO: comment below is lying, and just adds features without guidance.

    // Choose which renderer features to use based on information on which features the scene
    // context contains.

    ContextBuilder  scnRdrCB { scnRdrCtx, { mainCtx, windowCtx, sceneCtx }, rFW };

    if (rFW.get_interface<FIScene>(sceneCtx).id.has_value())
    {
        scnRdrCB.add_feature(ftrSceneRenderer);
        scnRdrCB.add_feature(ftrGodotScene);

        auto scnRender      = rFW.get_interface<FISceneRenderer>        (scnRdrCtx);
        auto &rScnRender    = rFW.data_get<draw::ACtxSceneRender>       (scnRender.di.scnRender);
        //auto &rCamera       = rFW.data_get<draw::Camera>                (gdScn.di.camera);

        MaterialId const matFlat  = rScnRender.m_materialIds.create();
        rScnRender.m_materials.resize(rScnRender.m_materialIds.size());

        scnRdrCB.add_feature(ftrCameraControlGD);

        scnRdrCB.add_feature(ftrFlatMaterial, matFlat);
        scnRdrCB.add_feature(ftrThrower);
        scnRdrCB.add_feature(ftrPhysicsShapesDraw, matFlat);
        scnRdrCB.add_feature(ftrCursor, TplPkgIdMaterialId{defaultPkg, matFlat});


        if (rFW.get_interface_id<FIPrefabs>(sceneCtx).has_value())
        {
            scnRdrCB.add_feature(ftrPrefabDraw, matFlat);
        }

        if (rFW.get_interface_id<FIVehicleSpawn>(sceneCtx).has_value())
        {
            scnRdrCB.add_feature(ftrVehicleControl);
            scnRdrCB.add_feature(ftrVehicleCamera);
            scnRdrCB.add_feature(ftrVehicleSpawnDraw);
        }
        else
        {
            scnRdrCB.add_feature(ftrCameraFree);
        }

        if (rFW.get_interface_id<FIRocketsJolt>(sceneCtx).has_value())
        {
            scnRdrCB.add_feature(ftrMagicRocketThrustIndicator, TplPkgIdMaterialId{ defaultPkg, matFlat });
        }



    }



    ContextBuilder::finalize(std::move(scnRdrCB));
    return scnRdrCtx;
} // make_scene_renderer

void FlyingScene::setup_app()
{
    // Setup Godot 'window application' renderer context
    // This is intended to stay alive as long as godot is open (forever), unlike the scene renderer
    // which is intended to be swapped out when the scene changes.

    auto      const mainApp   = m_framework.get_interface<FIMainApp>(m_mainContext);
    ContextId const sceneCtx  = m_framework.data_get<AppContexts>(mainApp.di.appContexts).scene;
    ContextId const windowCtx = m_framework.m_contextIds.create();
    ContextBuilder  windowCB { windowCtx, { m_mainContext, sceneCtx }, m_framework };
    windowCB.add_feature(adera::ftrWindowApp);
    windowCB.add_feature(ftrGodot, entt::make_any<godot::FlyingScene*>(this));
    ContextBuilder::finalize(std::move(windowCB));

    OSP_LOG_INFO("Setup godot");

    // Setup scene renderer sessions

    ContextId const sceneRenderCtx = make_scene_renderer(m_framework, m_mainContext, sceneCtx, windowCtx, m_defaultPkg);

    // All contexts and features are now created, keep track of them

    auto &rAppCtxs = m_framework.data_get<AppContexts>(mainApp.di.appContexts);
    rAppCtxs.window = windowCtx;
    rAppCtxs.sceneRender = sceneRenderCtx;

    // Start the main loop

    m_executor.load(m_framework);
    m_executor.run(m_framework, mainApp.pl.mainLoop);

    // Resynchronize renderer; Resync+Sync without stepping through time.
    // This makes sure meshes, textures, shaders, and other GPU-related resources specified by
    // the scene are properly loaded and assigned to entities within the renderer.
    drive_scene_cycle({.deltaTimeIn = 0.0f,
                       .update      = true,
                       .sceneUpdate = false,
                       .resync      = true,
                       .sync        = true,
                       .render      = false });
}

void FlyingScene::draw_event()
{
    drive_scene_cycle({.deltaTimeIn = 1.0f/60.0f,
                       .update      = true,
                       .sceneUpdate = true,
                       .resync      = false,
                       .sync        = true,
                       .render      = true });
}

void FlyingScene::_input(const Ref<InputEvent> &input)
{
    auto const mainApp  = m_framework.get_interface<FIMainApp>(m_mainContext);
    auto const& appCtxs = m_framework.data_get<AppContexts>(mainApp.di.appContexts);
    auto const windowApp = m_framework.get_interface<FIWindowApp>(appCtxs.window);
    auto &rUserInput = m_framework.data_get<UserInputHandler>(windowApp.di.userInput);


    // FIXME using dynamic_cast is a bit ugly, but afaik it's the best way if we bypass Godot's
    // InputMap (which we probably shouldn't in the long term)

    // keyboard
    auto inputKey = dynamic_cast<InputEventKey *>(input.ptr());
    if ( inputKey != nullptr )
    {
        if ( inputKey->is_echo() )
        {
            return;
        }
        EButtonEvent dir;
        if ( inputKey->is_pressed() )
        {
            dir = EButtonEvent::Pressed;
        }
        else if ( inputKey->is_released() )
        {
            dir = EButtonEvent::Released;
        }
        else
        {
            return;
        }
        rUserInput.event_raw(
            osp::input::sc_keyboard, (int)inputKey->get_physical_keycode(), dir);
        return;
    }

    // mouse button
    auto inputMouseButton = dynamic_cast<InputEventMouseButton *>(input.ptr());
    if ( inputMouseButton != nullptr )
    {
        if ( inputMouseButton->get_button_index() <= MOUSE_BUTTON_MIDDLE )
        {
            EButtonEvent dir;
            if ( inputMouseButton->is_pressed() )
            {
                dir = EButtonEvent::Pressed;
            }
            else if ( inputMouseButton->is_released() )
            {
                dir = EButtonEvent::Released;
            }
            else
            {
                return;
            }
            rUserInput.event_raw(
                osp::input::sc_mouse, (int)inputMouseButton->get_button_index(), dir);
            return;
        }
        osp::Vector2i scroll_delta;
        if ( inputMouseButton->get_button_index() == MOUSE_BUTTON_WHEEL_UP )
        {
            scroll_delta.y() += static_cast<int>(inputMouseButton->get_factor());
        }
        else if ( inputMouseButton->get_button_index() == MOUSE_BUTTON_WHEEL_DOWN )
        {
            scroll_delta.y() -= static_cast<int>(inputMouseButton->get_factor());
        }
        else if ( inputMouseButton->get_button_index() == MOUSE_BUTTON_WHEEL_RIGHT )
        {
            scroll_delta.x() += static_cast<int>(inputMouseButton->get_factor());
        }
        else if ( inputMouseButton->get_button_index() == MOUSE_BUTTON_WHEEL_LEFT )
        {
            scroll_delta.x() -= static_cast<int>(inputMouseButton->get_factor());
        }
        rUserInput.scroll_delta(scroll_delta);
        return;
    }

    // mouse motion
    auto inputMouseMotion = dynamic_cast<InputEventMouseMotion *>(input.ptr());
    if ( inputMouseMotion != nullptr )
    {
        godot::Vector2 mdelta = inputMouseMotion->get_relative();
        // Godot mouse position is in floats apparently. Error is probably minimal.
        rUserInput.mouse_delta({ static_cast<int>(mdelta.x), static_cast<int>(mdelta.y) });
        return;
    }
};

void FlyingScene::destroy_app()
{
    OSP_LOG_INFO("Destroy App");

    // Stops the pipeline loop
    drive_scene_cycle({.deltaTimeIn = 0.0f,
                       .update      = false,
                       .sceneUpdate = false,
                       .resync      = false,
                       .sync        = false,
                       .render      = false });
    if (m_executor.is_running(m_framework))
    {
        OSP_LOG_CRITICAL("Expected main loop to stop, but something is blocking it and cannot exit");
        std::abort();
    }

    auto const mainApp  = m_framework.get_interface<FIMainApp>(m_mainContext);
    auto const appCtxs = m_framework.data_get<AppContexts>(mainApp.di.appContexts);

    run_context_cleanup(appCtxs.sceneRender);
    run_context_cleanup(appCtxs.window);
    run_context_cleanup(appCtxs.scene);
    run_context_cleanup(appCtxs.main);

    if (m_executor.is_running(m_framework))
    {
        OSP_LOG_CRITICAL("Expected main loop to stop, but something is blocking it and cannot exit");
        std::abort();
    }

    m_framework.close_context(appCtxs.sceneRender);
    m_framework.close_context(appCtxs.window);
    m_framework.close_context(appCtxs.scene);
    m_framework.close_context(appCtxs.main);


    clear_resource_owners();

    // leak test app cause there is a free bug. FIXME of course.
//    auto leak_tp = new TestApp;
//    std::swap(*leak_tp, m_testApp);*/
}

void FlyingScene::set_scene(String const &scene)
{
    m_scene = scene;
}

String const &FlyingScene::get_scene() const
{
    return m_scene;
}

