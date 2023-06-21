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
#include "top_execute.h"
#include "top_worker.h"
#include "execute.h"

#include "../logging.h"

#include <Corrade/Containers/ArrayViewStl.h>

#include <entt/core/any.hpp>

#include <algorithm>
#include <sstream>
#include <vector>

namespace osp
{

void top_run_blocking(Tasks const& tasks, TaskGraph const& graph, TopTaskDataVec_t& rTaskData, ArrayView<entt::any> topData, ExecContext& rExec, WorkerContext worker)
{
    std::vector<entt::any> topDataRefs;

    // Run until there's no tasks left to run
    while (true)
    {
        auto const runTasksLeft     = rExec.tasksQueuedRun.size();
        auto const blockedTasksLeft = rExec.tasksQueuedBlocked.size();

        if (runTasksLeft+blockedTasksLeft == 0)
        {
            break;
        }

        if (runTasksLeft != 0)
        {
            TaskId const task = rExec.tasksQueuedRun.at(0);
            TopTask &rTopTask = rTaskData[task];

            std::cout << "running: " << rTopTask.m_debugName << "\n";

            topDataRefs.clear();
            topDataRefs.reserve(rTopTask.m_dataUsed.size());
            for (TopDataId const dataId : rTopTask.m_dataUsed)
            {
                topDataRefs.push_back((dataId != lgrn::id_null<TopDataId>())
                                       ? topData[dataId].as_ref()
                                       : entt::any{});
            }

            // Task actually runs here. Results are not yet used for anything.
            TriggerOut_t const status = rTopTask.m_func(worker, topDataRefs);

            mark_completed_task(tasks, graph, rExec, task, status);
        }

        enqueue_dirty(tasks, graph, rExec);
    }
}


} // namespace testapp
