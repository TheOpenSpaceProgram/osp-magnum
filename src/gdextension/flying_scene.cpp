#include "flying_scene.h"

#include "osp/tasks/top_utils.h"
#include "osp/util/UserInputHandler.h"
#include "scenarios.h"
#include "sessions/godot.h"
#include "testapp/sessions/common.h"
#include "testapp/testapp.h"

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
#include <osp/core/Resources.h>
#include <osp/core/string_concat.h>
#include <osp/drawing/own_restypes.h>
#include <osp/tasks/top_execute.h>
#include <osp/util/logging.h>
#include <osp/vehicles/ImporterData.h>
#include <osp/vehicles/load_tinygltf.h>

using namespace godot;

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
    delete (ExecutorType *)m_testApp.m_pExecutor;
}

void FlyingScene::_enter_tree()
{
    godot::UtilityFunctions::print("Enter tree");
    m_testApp.m_pExecutor = new ExecutorType();
    m_testApp.m_topData.resize(64);
    load_a_bunch_of_stuff();
    godot::UtilityFunctions::print("Resources loaded");

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

    godot::UtilityFunctions::print("Created viewport, scenario, and light");

    godot::UtilityFunctions::print("Scene is ", m_scene);
    auto const it = scenarios().find(m_scene.utf8().get_data());
    if ( it == std::end(scenarios()) )
    {
        godot::UtilityFunctions::print("Unknown scene");
        m_testApp.clear_resource_owners();
        return;
    }

    m_testApp.m_rendererSetup = it->second.m_setup(m_testApp);
}

void FlyingScene::_ready()
{
    setup_app();
}

void FlyingScene::_physics_process(double delta)
{
    // ospjolt::SysJolt::update_world() update the world
}

void FlyingScene::_process(double delta)
{
    m_pUserInput->update_controls();
    draw_event();
    m_pUserInput->clear_events();
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

void FlyingScene::load_a_bunch_of_stuff()
{
    using namespace osp::restypes;
    using namespace Magnum;
    using Primitives::ConeFlag;
    using Primitives::CylinderFlag;

    osp::TopTaskBuilder builder{ m_testApp.m_tasks,
                                 m_testApp.m_applicationGroup.m_edges,
                                 m_testApp.m_taskData };
    auto const          plApp = m_testApp.m_application.create_pipelines<PlApplication>(builder);

    builder.pipeline(plApp.mainLoop).loops(true).wait_for_signal(EStgOptn::ModifyOrSignal);

    // declares idResources and idMainLoopCtrl
    OSP_DECLARE_CREATE_DATA_IDS(
        m_testApp.m_application, m_testApp.m_topData, TESTAPP_DATA_APPLICATION);

    auto &rResources    = osp::top_emplace<osp::Resources>(m_testApp.m_topData, idResources);
    auto &rMainLoopCtrl = osp::top_emplace<MainLoopControl>(m_testApp.m_topData, idMainLoopCtrl);
    m_mainLoopCtrl      = &rMainLoopCtrl; // set main loop control

    builder.task()
        .name("Schedule Main Loop")
        .schedules({ plApp.mainLoop(EStgOptn::Schedule) })
        .push_to(m_testApp.m_application.m_tasks)
        .args({ idMainLoopCtrl })
        .func([](MainLoopControl const &rMainLoopCtrl) noexcept -> osp::TaskActions {
            if ( ! rMainLoopCtrl.doUpdate && ! rMainLoopCtrl.doSync && ! rMainLoopCtrl.doResync
                 && ! rMainLoopCtrl.doRender )
            {
                return osp::TaskAction::Cancel;
            }
            else
            {
                return {};
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
    m_testApp.m_defaultPkg                       = rResources.pkg_create();

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
        osp::ResId res = osp::load_tinygltf_file(
            osp::string_concat(datapath, meshName), rResources, m_testApp.m_defaultPkg);
        osp::assigns_prefabs_tinygltf(rResources, res);
    }

    // Add a default primitives
    auto const add_mesh_quick = [&rResources = rResources, &m_testApp = m_testApp](
                                    std::string_view const name, Trade::MeshData &&data) {
        osp::ResId const meshId =
            rResources.create(gc_mesh, m_testApp.m_defaultPkg, osp::SharedString::create(name));
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

    // OSP_LOG_INFO("Resource loading complete");
}

void FlyingScene::setup_app()
{
    osp::TopTaskBuilder builder{ m_testApp.m_tasks,
                                 m_testApp.m_renderer.m_edges,
                                 m_testApp.m_taskData };

    m_testApp.m_windowApp =
        scenes::setup_window_app(builder, m_testApp.m_topData, m_testApp.m_application);
    m_testApp.m_magnum = scenes::setup_godot(
        this, builder, m_testApp.m_topData, m_testApp.m_application, m_testApp.m_windowApp);

    godot::UtilityFunctions::print("setup_godot");

    // Setup renderer sessions

    m_testApp.m_rendererSetup(m_testApp);
    godot::UtilityFunctions::print("renderer");
    m_testApp.m_graph = osp::make_exec_graph(
        m_testApp.m_tasks, { &m_testApp.m_renderer.m_edges, &m_testApp.m_scene.m_edges });
    m_executor.load(m_testApp);
    m_testApp.m_pExecutor = &m_executor;
    // Start the main loop

    osp::PipelineId const mainLoop =
        m_testApp.m_application.get_pipelines<PlApplication>().mainLoop;
    m_testApp.m_pExecutor->run(m_testApp, mainLoop);

    // Resyncronize renderer

    *m_mainLoopCtrl = MainLoopControl{
        .doUpdate = false,
        .doSync   = true,
        .doResync = true,
        .doRender = false,
    };

    signal_all();

    m_testApp.m_pExecutor->wait(m_testApp);
}

void FlyingScene::draw_event()
{
    *m_mainLoopCtrl = MainLoopControl{
        .doUpdate = true,
        .doSync   = true,
        .doResync = false,
        .doRender = true,
    };

    signal_all();

    m_testApp.m_pExecutor->wait(m_testApp);
}

void FlyingScene::_input(const Ref<InputEvent> &input)
{
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
        m_pUserInput->event_raw(
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
            m_pUserInput->event_raw(
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
        m_pUserInput->scroll_delta(scroll_delta);
        return;
    }

    // mouse motion
    auto inputMouseMotion = dynamic_cast<InputEventMouseMotion *>(input.ptr());
    if ( inputMouseMotion != nullptr )
    {
        godot::Vector2 mdelta = inputMouseMotion->get_relative();
        // Godot mouse position is in floats apparently. Error is probably minimal.
        m_pUserInput->mouse_delta({ static_cast<int>(mdelta.x), static_cast<int>(mdelta.y) });
        return;
    }
};

void FlyingScene::destroy_app()
{

    *m_mainLoopCtrl = MainLoopControl{
        .doUpdate = false,
        .doSync   = false,
        .doResync = false,
        .doRender = false,
    };

    signal_all();

    m_testApp.m_pExecutor->wait(m_testApp);

    if ( m_testApp.m_pExecutor->is_running(m_testApp) )
    {
        // Main loop must have stopped, but didn't!
        m_testApp.m_pExecutor->wait(m_testApp);
        std::abort();
    }

    // Closing sessions will delete their associated TopData and Tags
    m_testApp.m_pExecutor->run(m_testApp, m_testApp.m_windowApp.m_cleanup);
    m_testApp.close_sessions(m_testApp.m_renderer.m_sessions);
    m_testApp.m_renderer.m_sessions.clear();
    m_testApp.m_renderer.m_edges.m_syncWith.clear();

    m_testApp.close_session(m_testApp.m_magnum);
    m_testApp.close_session(m_testApp.m_windowApp);

    // leak test app cause there is a free bug. FIXME of course.
    auto leak_tp = new TestApp;
    std::swap(*leak_tp, m_testApp);
}

void FlyingScene::set_scene(String const &scene)
{
    m_scene = scene;
}

String const &FlyingScene::get_scene() const
{
    return m_scene;
}

