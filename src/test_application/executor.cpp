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
#include "executor.h"

#include <osp/tasks/top_execute.h>

#include <spdlog/fmt/ostr.h>

namespace testapp
{

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
