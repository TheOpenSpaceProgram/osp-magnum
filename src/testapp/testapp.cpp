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

void run_cleanup(osp::fw::ContextId ctx, osp::fw::Framework &rFW, osp::fw::IExecutor &rExec)
{
    auto const cleanup = rFW.get_interface<FICleanupContext>(ctx);
    if ( cleanup.id.has_value() )
    {
        // Run cleanup pipeline for the window context
        rExec.task_finish(rFW, cleanup.tasks.blockSchedule);
        rExec.wait(rFW);

        if (rExec.is_running(rFW, cleanup.loopblks.cleanup))
        {
            OSP_LOG_CRITICAL("Deadlock in cleanup pipeline");
            std::abort();
        }
    }
}


/*
void TestApp::init()
{
}


void TestApp::drive_default_main_loop()
{

}

void TestApp::drive_scene_cycle(UpdateParams p)
{
    Framework &rFW = m_framework;

    auto const mainApp          = rFW.get_interface<FIMainApp>  (m_mainContext);
    auto const &rAppCtxs        = rFW.data_get<AppContexts>     (mainApp.di.appContexts);
    auto       &rMainLoopCtrl   = rFW.data_get<MainLoopControl> (mainApp.di.mainLoopCtrl);
    //rMainLoopCtrl.doUpdate      = p.update;

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

// SYNCEXEC
    //m_pExecutor->task_finish(m_framework, mainApp.pl.mainLoop);
    //m_pExecutor->task_finish(m_framework, windowApp.pl.inputs);
    //m_pExecutor->task_finish(m_framework, windowApp.pl.sync);
    //m_pExecutor->task_finish(m_framework, windowApp.pl.resync);

    m_pExecutor->wait(m_framework);
}

void TestApp::run_context_cleanup(ContextId ctx)
{
    auto const cleanup = m_framework.get_interface<FICleanupContext> (ctx);
    if ( cleanup.id.has_value() )
    {
        // Run cleanup pipeline for the window context
//        m_pExecutor->run(m_framework, cleanup.pl.cleanup);
//        m_pExecutor->wait(m_framework);

//        if (m_pExecutor->is_running(m_framework))
//        {
//            OSP_LOG_CRITICAL("Deadlock in cleanup pipeline");
//            std::abort();
//        }
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
*/



} // namespace testapp
