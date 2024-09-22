/**
 * Open Space Program
 * Copyright © 2019-2024 Open Space Program Project
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
#include "framework.h"

#include <longeron/id_management/id_set_stl.hpp>

namespace osp::fw
{

void Framework::close_context(ContextId ctx)
{
    FeatureContext &rFtrCtx = m_contextData[ctx];

    // Clear all feature interfaces in the context
    for (FIInstanceId &rFIInstId : rFtrCtx.finterSlots)
    {
        if ( ! rFIInstId.has_value() )
        {
            continue;
        }

        FeatureInterface &rFIInst = m_fiinstData[rFIInstId];

        for (DataId const dataId : rFIInst.data)
        {
            m_data[dataId].reset();
            m_dataIds.remove(dataId);
        }
        rFIInst.data.clear();

        for (PipelineId const pipelineId : rFIInst.pipelines)
        {
            m_tasks.m_pipelineIds.remove(pipelineId);
            m_tasks.m_pipelineParents   [pipelineId] = lgrn::id_null<PipelineId>();
            m_tasks.m_pipelineInfo      [pipelineId] = {};
            m_tasks.m_pipelineControl   [pipelineId] = {};
        }
        rFIInst.pipelines.clear();

        rFIInst.context = {};
        rFIInst.type = {};

        rFIInstId = {};
    }
    // no clear, set all to {}

    lgrn::IdSetStl<TaskId> deletedTasks;
    deletedTasks.resize(m_tasks.m_taskIds.capacity());

    // Clear all sessions in the context
    for (FSessionId const sessionId : rFtrCtx.sessions)
    {
        FeatureSession &rFSession = m_fsessionData[sessionId];

        for (TaskId const taskId : rFSession.tasks)
        {
            m_tasks.m_taskIds.remove(taskId);
            deletedTasks.insert(taskId);

            TaskImpl &rTaskImpl = m_taskImpl[taskId];
            rTaskImpl.debugName.clear();
            rTaskImpl.args.clear();
            rTaskImpl.func = nullptr;
        }
    }

    auto newLast = std::remove_if(m_tasks.m_syncWith.begin(), m_tasks.m_syncWith.end(),
                                  [&deletedTasks] (TplTaskPipelineStage const &tpl)
    {
        return deletedTasks.contains(tpl.task);
    });
    m_tasks.m_syncWith.erase(newLast, m_tasks.m_syncWith.end());

    rFtrCtx.sessions.clear();
}


} // namespace osp::fw
