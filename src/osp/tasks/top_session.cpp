/**
 * Open Space Program
 * Copyright © 2019-2022 Open Space Program Project
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
#include "top_session.h"
#include "top_execute.h"
#include "execute.h"
#include "tasks.h"

#include <Corrade/Containers/ArrayViewStl.h>

#include <longeron/containers/bit_view.hpp>

#include <algorithm>

namespace osp
{

void top_close_session(
        Tasks &                 rTasks,
        TaskGraph const&        graph,
        TopTaskDataVec_t&       rTaskData,
        ArrayView<entt::any>    topData,
        ExecContext&            rExec,
        ArrayView<Session>      sessions)
{
    // Run cleanup pipelines
    for (Session &rSession : sessions)
    {
        if (rSession.m_cleanup.pipeline != lgrn::id_null<PipelineId>())
        {
            exec_trigger(rExec, {rSession.m_cleanup.pipeline, rSession.m_cleanup.stage});
        }
    }
    enqueue_dirty(rTasks, graph, rExec);
    top_run_blocking(rTasks, graph, rTaskData, topData, rExec);

    // Clear each session's TopData
    for (Session &rSession : sessions)
    {
        for (TopDataId const id : std::exchange(rSession.m_data, {}))
        {
            if (id != lgrn::id_null<TopDataId>())
            {
                topData[std::size_t(id)].reset();
            }
        }
    }

    // Clear each session's tasks
    for (Session &rSession : sessions)
    {
        for (TaskId const task : rSession.m_tasks)
        {
            rTasks.m_taskIds.remove(task);

            TopTask &rCurrTaskData = rTaskData[task];
            rCurrTaskData.m_debugName.clear();
            rCurrTaskData.m_dataUsed.clear();
            rCurrTaskData.m_func = nullptr;
        }
    }
}

} // namespace testapp
