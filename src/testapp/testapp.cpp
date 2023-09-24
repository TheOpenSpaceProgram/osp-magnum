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
#include "identifiers.h"

#include <osp/core/Resources.h>
#include <osp/drawing/own_restypes.h>
#include <osp/tasks/top_execute.h>
#include <osp/tasks/top_utils.h>
#include <osp/vehicles/ImporterData.h>
#include <spdlog/fmt/ostr.h>

namespace testapp
{

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


template<typename FUNC_T>
static void resource_for_each_type(osp::ResTypeId const type, osp::Resources& rResources, FUNC_T&& do_thing)
{
    lgrn::IdRegistry<osp::ResId> const &rReg = rResources.ids(type);
    for (std::size_t i = 0; i < rReg.capacity(); ++i)
    {
        if (rReg.exists(osp::ResId(i)))
        {
            do_thing(osp::ResId(i));
        }
    }
}


void TestApp::clear_resource_owners()
{
    using namespace osp::restypes;

    // declares idResources
    OSP_DECLARE_CREATE_DATA_IDS(m_application, m_topData, TESTAPP_DATA_APPLICATION);

    auto &rResources = osp::top_get<osp::Resources>(m_topData, idResources);

    // Texture resources contain osp::TextureImgSource, which refererence counts
    // their associated image data
    resource_for_each_type(gc_texture, rResources, [&rResources] (osp::ResId const id)
    {
        auto * const pData = rResources.data_try_get<osp::TextureImgSource>(gc_texture, id);
        if (pData != nullptr)
        {
            rResources.owner_destroy(gc_image, std::move(*pData));
        }
    });

    // Importer data own a lot of other resources
    resource_for_each_type(gc_importer, rResources, [&rResources] (osp::ResId const id)
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
    });
}


//-----------------------------------------------------------------------------


void SingleThreadedExecutor::load(TestAppTasks& rAppTasks)
{
    osp::exec_conform(rAppTasks.m_tasks, m_execContext);
    m_execContext.doLogging = m_log != nullptr;
}

void SingleThreadedExecutor::run(TestAppTasks& rAppTasks, osp::PipelineId pipeline)
{
    osp::exec_request_run(m_execContext, pipeline);
}

void SingleThreadedExecutor::signal(TestAppTasks& rAppTasks, osp::PipelineId pipeline)
{
    osp::exec_signal(m_execContext, pipeline);
}

void SingleThreadedExecutor::wait(TestAppTasks& rAppTasks)
{
    if (m_log != nullptr)
    {
        m_log->info("\n>>>>>>>>>> Previous State Changes\n{}\n>>>>>>>>>> Current State\n{}\n",
                    osp::TopExecWriteLog  {rAppTasks.m_tasks, rAppTasks.m_taskData, rAppTasks.m_graph, m_execContext},
                    osp::TopExecWriteState{rAppTasks.m_tasks, rAppTasks.m_taskData, rAppTasks.m_graph, m_execContext} );
        m_execContext.logMsg.clear();
    }

    osp::exec_update(rAppTasks.m_tasks, rAppTasks.m_graph, m_execContext);
    osp::top_run_blocking(rAppTasks.m_tasks, rAppTasks.m_graph, rAppTasks.m_taskData, rAppTasks.m_topData, m_execContext);

    if (m_log != nullptr)
    {
        m_log->info("\n>>>>>>>>>> New State Changes\n{}",
                    osp::TopExecWriteLog{rAppTasks.m_tasks, rAppTasks.m_taskData, rAppTasks.m_graph, m_execContext} );
        m_execContext.logMsg.clear();
    }
}

bool SingleThreadedExecutor::is_running(TestAppTasks const& appTasks)
{
    return m_execContext.hasRequestRun || (m_execContext.pipelinesRunning != 0);
}


} // namespace testapp
