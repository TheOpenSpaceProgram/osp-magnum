#include "flying_scene.h"
#include "ospjolt/joltinteg.h"
#include "ospjolt/joltinteg_fn.h"
#include "testapp/testapp.h"
#include <Magnum/MeshTools/Transform.h>
#include <Magnum/Primitives/Cone.h>
#include <Magnum/Primitives/Cube.h>
#include <Magnum/Primitives/Cylinder.h>
#include <Magnum/Primitives/Grid.h>
#include <Magnum/Primitives/Icosphere.h>
#include <Magnum/Trade/MeshData.h>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/resource_saver.hpp>
#include <godot_cpp/classes/viewport.hpp>
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
}

FlyingScene::FlyingScene() 
{
	// Initialize any variables here.
	//m_testApp.m_pExecutor = new ExecutorType();
	//m_testApp.m_topData.resize(64);
	//load_a_bunch_of_stuff();
}

FlyingScene::~FlyingScene() 
{
	//delete m_joltWorld;
	//delete (ExecutorType*) m_testApp.m_pExecutor;
}

void FlyingScene::_enter_tree() 
{
	RenderingServer* renderingServer = RenderingServer::get_singleton();
	m_scenario = get_world_3d()->get_scenario();
	m_viewport = get_viewport()->get_viewport_rid();
	m_mainCamera = renderingServer->camera_create();
	renderingServer->viewport_attach_camera(m_viewport, m_mainCamera);

	Transform3D cform = Transform3D(Basis(), Vector3(0, 0, 4));
	renderingServer->camera_set_transform(m_mainCamera, cform);
	renderingServer->viewport_set_scenario(m_viewport, m_scenario);

	m_lightInstance = renderingServer->instance_create();
	renderingServer->instance_set_scenario(m_lightInstance, m_scenario);
	
	RID light = renderingServer->directional_light_create();
	renderingServer->instance_set_base(m_lightInstance, light);
	
	Transform3D lform = Transform3D(Basis().rotated(Vector3(1, 1, 0), -1.3), Vector3(0., 0., 0.));
	renderingServer->instance_set_transform(m_lightInstance, lform);

	godot::UtilityFunctions::print("Created viewport, camera, scenario, and light");

	//init physics
	ospjolt::ACtxJoltWorld::initJoltGlobal();
	m_joltWorld = new ospjolt::ACtxJoltWorld();

	godot::UtilityFunctions::print("Init physics");

}

void FlyingScene::_ready() 
{
	RenderingServer* renderingServer = RenderingServer::get_singleton();
	RID instance = renderingServer->instance_create();
	renderingServer->instance_set_scenario(instance, m_scenario);

	RID mesh = renderingServer->make_sphere_mesh(10, 10, 1);
	renderingServer->instance_set_base(instance, mesh);

	Transform3D xform = Transform3D(Basis(), Vector3(0.5, 0, 0));
	renderingServer->instance_set_transform(instance, xform);

}

void FlyingScene::_physics_process(double delta) 
{
	// ospjolt::SysJolt::update_world() update the world
}

// void FlyingScene::load_a_bunch_of_stuff()
// {
//     using namespace osp::restypes;
//     using namespace Magnum;
// 	using Primitives::ConeFlag;
//     using Primitives::CylinderFlag;

//     osp::TopTaskBuilder builder{m_testApp.m_tasks, m_testApp.m_applicationGroup.m_edges, m_testApp.m_taskData};
//     auto const plApp = m_testApp.m_application.create_pipelines<PlApplication>(builder);

//     builder.pipeline(plApp.mainLoop).loops(true).wait_for_signal(EStgOptn::ModifyOrSignal);

//     // declares idResources and idMainLoopCtrl
//     OSP_DECLARE_CREATE_DATA_IDS(m_testApp.m_application, m_testApp.m_topData, TESTAPP_DATA_APPLICATION);

//     auto &rResources = osp::top_emplace<osp::Resources> (m_testApp.m_topData, idResources);
//     /* unused */       osp::top_emplace<MainLoopControl>(m_testApp.m_topData, idMainLoopCtrl);

//     builder.task()
//         .name       ("Schedule Main Loop")
//         .schedules  ({plApp.mainLoop(EStgOptn::Schedule)})
//         .push_to    (m_testApp.m_application.m_tasks)
//         .args       ({                  idMainLoopCtrl})
//         .func([] (MainLoopControl const& rMainLoopCtrl) noexcept -> osp::TaskActions
//     {
//         if (   ! rMainLoopCtrl.doUpdate
//             && ! rMainLoopCtrl.doSync
//             && ! rMainLoopCtrl.doResync
//             && ! rMainLoopCtrl.doRender)
//         {
//             return osp::TaskAction::Cancel;
//         }
//         else
//         {
//             return { };
//         }
//     });

//     rResources.resize_types(osp::ResTypeIdReg_t::size());

//     rResources.data_register<Trade::ImageData2D>(gc_image);
//     rResources.data_register<Trade::TextureData>(gc_texture);
//     rResources.data_register<osp::TextureImgSource>(gc_texture);
//     rResources.data_register<Trade::MeshData>(gc_mesh);
//     rResources.data_register<osp::ImporterData>(gc_importer);
//     rResources.data_register<osp::Prefabs>(gc_importer);
//     osp::register_tinygltf_resources(rResources);
//     m_testApp.m_defaultPkg = rResources.pkg_create();

//     // Load sturdy glTF files
//     const std::string_view datapath = { "OSPData/adera/" };
//     const std::vector<std::string_view> meshes =
//     {
//         "spamcan.sturdy.gltf",
//         "stomper.sturdy.gltf",
//         "ph_capsule.sturdy.gltf",
//         "ph_fuselage.sturdy.gltf",
//         "ph_engine.sturdy.gltf",
//         //"ph_plume.sturdy.gltf",
//         "ph_rcs.sturdy.gltf"
//         //"ph_rcs_plume.sturdy.gltf"
//     };

//     // TODO: Make new gltf loader. This will read gltf files and dump meshes,
//     //       images, textures, and other relevant data into osp::Resources
//     for (auto const& meshName : meshes)
//     {
//         osp::ResId res = osp::load_tinygltf_file(osp::string_concat(datapath, meshName), rResources, g_testApp.m_defaultPkg);
//         osp::assigns_prefabs_tinygltf(rResources, res);
//     }

//     // Add a default primitives
//     auto const add_mesh_quick = [&rResources = rResources, &m_testApp = m_testApp] (std::string_view const name, Trade::MeshData&& data)
//     {
//         osp::ResId const meshId = rResources.create(gc_mesh, m_testApp.m_defaultPkg, osp::SharedString::create(name));
//         rResources.data_add<Trade::MeshData>(gc_mesh, meshId, std::move(data));
//     };


//     Trade::MeshData &&cylinder = Magnum::MeshTools::transform3D( Primitives::cylinderSolid(3, 16, 1.0f, CylinderFlag::CapEnds), Matrix4::rotationX(Deg(90)), 0);
//     Trade::MeshData &&cone = Magnum::MeshTools::transform3D( Primitives::coneSolid(3, 16, 1.0f, ConeFlag::CapEnd), Matrix4::rotationX(Deg(90)), 0);

//     add_mesh_quick("cube", Primitives::cubeSolid());
//     add_mesh_quick("cubewire", Primitives::cubeWireframe());
//     add_mesh_quick("sphere", Primitives::icosphereSolid(2));
//     add_mesh_quick("cylinder", std::move(cylinder));
//     add_mesh_quick("cone", std::move(cone));
//     add_mesh_quick("grid64solid", Primitives::grid3DSolid({63, 63}));

//     OSP_LOG_INFO("Resource loading complete");
// }
