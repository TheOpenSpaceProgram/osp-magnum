/**
 * Open Space Program
 * Copyright © 2019-2023 Open Space Program Project
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
#include "testapp.h"

#include <adera_app/feature_interfaces.h>
#include <adera_app/application.h>
#include <adera_app/features/common.h>

#include <osp/core/Resources.h>
#include <osp/framework/builder.h>
#include <osp/drawing/own_restypes.h>
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

#include <spdlog/fmt/ostr.h>

using namespace adera;

using namespace osp::fw;
using namespace ftr_inter;

namespace testapp
{

void TestApp::init()
{
    auto const fiMain         = m_framework.get_interface<FIMainApp>(m_mainContext);
    auto       &rResources    = m_framework.data_get<osp::Resources&>(fiMain.di.resources);
    rResources.resize_types(osp::ResTypeIdReg_t::size());
    m_defaultPkg = rResources.pkg_create();

    load_a_bunch_of_stuff();

    m_pExecutor->load(m_framework);
    m_pExecutor->run(m_framework, fiMain.pl.mainLoop);
}

void TestApp::drive_default_main_loop()
{
    auto const mainApp        = m_framework.get_interface<FIMainApp>(m_mainContext);
    auto       &rMainLoopCtrl = m_framework.data_get<MainLoopControl&>(mainApp.di.mainLoopCtrl);
    auto       &rFWModify     = m_framework.data_get<FrameworkModify&>(mainApp.di.frameworkModify);

    if ( ! rFWModify.commands.empty() )
    {
        // Stop the framework main loop
        rMainLoopCtrl.doUpdate = false;
        m_pExecutor->signal(m_framework, mainApp.pl.mainLoop);
        m_pExecutor->wait(m_framework);

        if (m_pExecutor->is_running(m_framework))
        {
            OSP_LOG_CRITICAL("something is blocking the framework main loop from exiting. RIP");
            std::abort();
        }

        for (FrameworkModify::Command &rCmd : rFWModify.commands)
        {
            rCmd.func(m_framework, rCmd.ctx, std::move(rCmd.userData));
        }
        rFWModify.commands.clear();

        // Restart framework main loop
        m_pExecutor->load(m_framework);
        m_pExecutor->run(m_framework, mainApp.pl.mainLoop);

        return;
    }


    auto const fiMain         = m_framework.get_interface<FIMainApp>(m_mainContext);

    rMainLoopCtrl.doUpdate = true;

    m_pExecutor->signal(m_framework, fiMain.pl.mainLoop);
    m_pExecutor->wait(m_framework);
}

void TestApp::drive_scene_cycle(UpdateParams p)
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

    m_pExecutor->signal(m_framework, mainApp.pl.mainLoop);
    m_pExecutor->signal(m_framework, windowApp.pl.inputs);
    m_pExecutor->signal(m_framework, windowApp.pl.sync);
    m_pExecutor->signal(m_framework, windowApp.pl.resync);

    m_pExecutor->wait(m_framework);
}

void TestApp::run_context_cleanup(ContextId ctx)
{
    auto const cleanup = m_framework.get_interface<FICleanupContext> (ctx);
    if ( cleanup.id.has_value() )
    {
        // Run cleanup pipeline for the window context
        m_pExecutor->run(m_framework, cleanup.pl.cleanup);
        m_pExecutor->wait(m_framework);

        if (m_pExecutor->is_running(m_framework))
        {
            OSP_LOG_CRITICAL("Deadlock in cleanup pipeline");
            std::abort();
        }
    }
}

void TestApp::clear_resource_owners()
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

void TestApp::load_a_bunch_of_stuff()
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
        osp::ResId res = osp::load_tinygltf_file(osp::string_concat(datapath, meshName), rResources, m_defaultPkg);
        osp::assigns_prefabs_tinygltf(rResources, res);
    }

    // Add a default primitives
    auto const add_mesh_quick = [&rResources = rResources, this] (std::string_view const name, Trade::MeshData&& data)
    {
        osp::ResId const meshId = rResources.create(gc_mesh, m_defaultPkg, osp::SharedString::create(name));
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


} // namespace testapp
