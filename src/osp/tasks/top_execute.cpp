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
#include "execute_simple.h"

#include <Corrade/Containers/ArrayViewStl.h>

#include <entt/core/any.hpp>

#include <vector>

namespace osp
{

void top_run_blocking(TaskTags& rTasks, TopTaskDataVec_t& rTaskData, ArrayView<entt::any> topData, ExecutionContext& rExec, ArrayView<TaskTags::Tag const> tags)
{
    rExec.m_tagIncompleteCounts  .resize(rTasks.m_tags.capacity(), 0);
    rExec.m_tagRunningCounts     .resize(rTasks.m_tags.capacity(), 0);
    rExec.m_taskQueuedCounts     .resize(rTasks.m_tasks.capacity(), 0);

    std::vector<uint64_t> tagsToRun(rTasks.m_tags.vec().size());
    to_bitspan(tags, tagsToRun);
    task_enqueue(rTasks, rExec, tagsToRun);

    std::vector<entt::any> dataRefs;
    WorkerContext context;

    // Run until there's no tasks left to run
    while (true)
    {
        std::vector<uint64_t> tasksToRun(rTasks.m_tasks.vec().size());
        task_list_available(rTasks, rExec, tasksToRun);
        auto const tasksToRunBits = lgrn::bit_view(tasksToRun);
        unsigned int const availableCount = tasksToRunBits.count();

        if (availableCount == 0)
        {
            break;
        }

        // Choose first available task
        auto const task = TaskTags::Task(*tasksToRunBits.ones().begin());

        task_start(rTasks, rExec, task);

        TopTask &rTopTask = rTaskData.m_taskData.at(std::size_t(task));
        dataRefs.resize(rTopTask.m_dataUsed.size());
        for (std::size_t i = 0; i < rTopTask.m_dataUsed.size(); ++i)
        {
            dataRefs.at(i) = topData[rTopTask.m_dataUsed[i]].as_ref();
        }

        rTopTask.m_func(context, dataRefs);

        task_finish(rTasks, rExec, task);
    }
}

} // namespace testapp
