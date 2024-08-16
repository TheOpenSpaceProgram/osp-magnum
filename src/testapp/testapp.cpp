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
#include "feature_interfaces.h"

#include <adera/application.h>

#include <osp/core/Resources.h>
#include <osp/drawing/own_restypes.h>

#include <osp/vehicles/ImporterData.h>
#include <spdlog/fmt/ostr.h>

using namespace adera;
using namespace osp::fw;

namespace testapp
{

FeatureDef const ftrMain = feature_def("Main", [] (FeatureBuilder& rFB, Implement<FIMainApp> mainApp, entt::any pkg)
{
    rFB.data_emplace< AppContexts >             (mainApp.di.appContexts);
    rFB.data_emplace< MainLoopControl >         (mainApp.di.mainLoopCtrl);
    rFB.data_emplace< osp::Resources >          (mainApp.di.resources);
    rFB.data_emplace< FrameworkModify >         (mainApp.di.frameworkModify);
    rFB.data_emplace< std::vector<std::string> >(mainApp.di.cin);

    rFB.pipeline(mainApp.pl.mainLoop).loops(true).wait_for_signal(EStgOptn::ModifyOrSignal);
    rFB.pipeline(mainApp.pl.cin).parent(mainApp.pl.mainLoop);

    rFB.task()
        .name       ("Schedule Main Loop")
        .schedules  ({mainApp.pl.mainLoop(EStgOptn::Schedule)})
        .args       ({         mainApp.di.mainLoopCtrl})
        .func([] (MainLoopControl const& rMainLoopCtrl) noexcept -> osp::TaskActions
    {
        return rMainLoopCtrl.doUpdate ? osp::TaskActions{} : osp::TaskAction::Cancel;
    });

    rFB.task()
        .name       ("Read stdin buffer")
        .run_on     ({mainApp.pl.mainLoop(EStgOptn::Run)})
        .sync_with  ({mainApp.pl.cin(EStgIntr::Modify_)})
        .args       ({            mainApp.di.cin})
        .func([] (std::vector<std::string> &rCin) noexcept
    {
        rCin = NonBlockingStdInReader::instance().read();
    });
});

void TestApp::init()
{
    LGRN_ASSERTM( ! m_mainContext.has_value(), "Call init() only once. pretty please!");
    m_mainContext = m_framework.contextIds.create();

    ContextBuilder   cb { .m_ctx = m_mainContext, .m_rFW = m_framework };
    cb.add_feature(ftrMain);
    LGRN_ASSERTM(cb.m_errors.empty(), "Error adding main feature");
    ContextBuilder::apply(std::move(cb));

    auto const fiMain         = m_framework.get_interface<FIMainApp>(m_mainContext);
    auto       &rResources    = entt::any_cast<osp::Resources&>          (m_framework.data[fiMain.di.resources]);
    rResources.resize_types(osp::ResTypeIdReg_t::size());
    m_defaultPkg = rResources.pkg_create();

    m_pExecutor->load(m_framework);
    m_pExecutor->run(m_framework, fiMain.pl.mainLoop);
}

void TestApp::drive_main_loop()
{
    auto const fiMain         = m_framework.get_interface<FIMainApp>(m_mainContext);
    auto       &rMainLoopCtrl = entt::any_cast<MainLoopControl&>(m_framework.data[fiMain.di.mainLoopCtrl]);
    auto       &rFWModify     = entt::any_cast<FrameworkModify&>(m_framework.data[fiMain.di.frameworkModify]);

    if ( ! rFWModify.commands.empty() )
    {
        // Stop the framework main loop
        rMainLoopCtrl.doUpdate = false;
        m_pExecutor->signal(m_framework, fiMain.pl.mainLoop);
        m_pExecutor->wait(m_framework);

        if (m_pExecutor->is_running(m_framework))
        {
            OSP_LOG_CRITICAL("something is blocking the framework main loop from exiting. RIP");
            std::abort();
        }

        for (FrameworkModify::Command &rCmd : rFWModify.commands)
        {
            rCmd.func(std::move(rCmd.userData));
        }
        rFWModify.commands.clear();

        // Restart framework main loop
        m_pExecutor->load(m_framework);
        m_pExecutor->run(m_framework, fiMain.pl.mainLoop);
    }

    rMainLoopCtrl.doUpdate = true;

    m_pExecutor->signal(m_framework, fiMain.pl.mainLoop);
    m_pExecutor->wait(m_framework);
}

/*
void TestApp::close_sessions(osp::ArrayView<osp::Session> const sessions)
{
    using namespace osp;

    // Run cleanup pipelines
    for (Session &rSession : sessions)
    {
        if (rSession.m_cleanup != lgrn::id_null<osp::PipelineId>())
        {
            m_pExecutor->run(*this, rSession.m_cleanup);
        }
    }
    m_pExecutor->wait(*this);

    // Clear each session's TopData
    for (Session &rSession : sessions)
    {
        for (TopDataId const id : std::exchange(rSession.m_data, {}))
        {
            if (id != lgrn::id_null<TopDataId>())
            {
                m_topData[std::size_t(id)].reset();
            }
        }
    }

    // Clear each session's tasks and pipelines
    for (Session &rSession : sessions)
    {
        for (TaskId const task : rSession.m_tasks)
        {
            m_tasks.m_taskIds.remove(task);

            TopTask &rCurrTaskData = m_taskData[task];
            rCurrTaskData.m_debugName.clear();
            rCurrTaskData.m_dataUsed.clear();
            rCurrTaskData.m_func = nullptr;
        }
        rSession.m_tasks.clear();

        for (PipelineId const pipeline : rSession.m_pipelines)
        {
            m_tasks.m_pipelineIds.remove(pipeline);
            m_tasks.m_pipelineParents[pipeline] = lgrn::id_null<PipelineId>();
            m_tasks.m_pipelineInfo[pipeline]    = {};
            m_tasks.m_pipelineControl[pipeline] = {};
        }
        rSession.m_pipelines.clear();
    }
}


void TestApp::close_session(osp::Session &rSession)
{
    close_sessions(osp::ArrayView<osp::Session>(&rSession, 1));
}

void TestApp::clear_resource_owners()
{
    using namespace osp::restypes;

    // declares mainApp.di.resources
    OSP_DECLARE_GET_DATA_IDS(m_application, TESTAPP_DATA_APPLICATION);

    auto &rResources = osp::rFB.data_get<osp::Resources>(m_topData, mainApp.di.resources);

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
