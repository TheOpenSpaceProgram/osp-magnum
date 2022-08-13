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

#include "../logging.h"

#include <Corrade/Containers/ArrayViewStl.h>

#include <entt/core/any.hpp>

#include <sstream>
#include <vector>

using Corrade::Containers::ArrayView;
using Corrade::Containers::StridedArrayView2D;
using Corrade::Containers::arrayView;

namespace osp
{

void top_run_blocking(TaskTags& rTasks, TopTaskDataVec_t& rTaskData, ArrayView<entt::any> topData, ExecutionContext& rExec)
{
    std::vector<entt::any> dataRefs;
    WorkerContext context;

    std::vector<uint64_t> tmpEnqueue(rTasks.m_tags.vec().size());

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

        TopTaskStatus const status = rTopTask.m_func(context, dataRefs);

        BitSpan_t enqueue = (status == TopTaskStatus::Success) ? tmpEnqueue : BitSpan_t{};

        task_finish(rTasks, rExec, task, enqueue);
    }
}

void top_enqueue_quick(TaskTags& rTasks, ExecutionContext& rExec, ArrayView<TaskTags::Tag const> tags)
{
    rExec.m_tagIncompleteCounts  .resize(rTasks.m_tags.capacity(), 0);
    rExec.m_tagRunningCounts     .resize(rTasks.m_tags.capacity(), 0);
    rExec.m_taskQueuedCounts     .resize(rTasks.m_tasks.capacity(), 0);

    std::vector<uint64_t> tagsToRun(rTasks.m_tags.vec().size());
    to_bitspan(tags, tagsToRun);
    task_enqueue(rTasks, rExec, tagsToRun);
}

static void check_task_dependencies(TaskTags const& tags, TopTaskDataVec_t const& taskData, std::vector<uint32_t> &rPath, uint32_t const task, bool& rGood)
{
    rPath.push_back(task);

    StridedArrayView2D<uint64_t const> const taskTags2d{
            arrayView(tags.m_taskTags), {tags.m_tasks.capacity(), tags.tag_ints_per_task()}};
    StridedArrayView2D<TaskTags::Tag const> const tagDepends2d{
            arrayView(tags.m_tagDepends), {tags.m_tags.capacity(), tags.m_tagDependsPerTag}};

    std::vector<uint64_t> dependencyTagInts(tags.m_tags.vec().size(), 0x0);
    auto dependencyTagsBits = lgrn::bit_view(dependencyTagInts);

    // Loop through each of this task's tags to get dependent tags
    auto const taskTagsInts = taskTags2d[task].asContiguous();
    auto const view = lgrn::bit_view(taskTagsInts);
    for (uint32_t const currTag : view.ones())
    {
        auto const currTagDepends = tagDepends2d[currTag].asContiguous();

        for (TaskTags::Tag const dependTag : currTagDepends)
        {
            if (dependTag == lgrn::id_null<TaskTags::Tag>())
            {
                break;
            }

            dependencyTagsBits.set(std::size_t(dependTag));
        }
    }

    // Get all tasks that contain any of the dependent tags
    for (uint32_t const currTask : tags.m_tasks.bitview().zeros())
    {
        if (any_bits_match(taskTags2d[currTask].asContiguous(), dependencyTagInts))
        {
            auto const &last = std::end(rPath);
            if (last == std::find(std::begin(rPath), last, currTask))
            {
                check_task_dependencies(tags, taskData, rPath, currTask, rGood);
            }
            else
            {
                rGood = false;
                std::ostringstream ss;
                ss << "TopTask Circular Dependency Detected!\n";

                for (uint32_t const pathTask : rPath)
                {
                    ss << "* [" << pathTask << "]: " << taskData.m_taskData[pathTask].m_debugName << "\n";
                }
                ss << "* Loops back to [" << currTask << "]\n";
                OSP_LOG_ERROR("{}", ss.str());
            }
        }
    }

    rPath.pop_back();
}

bool debug_top_verify(TaskTags const& tags, TopTaskDataVec_t const& taskData)
{
    std::vector<uint32_t> path;

    bool good = true;

    // Iterate all tasks
    for (uint32_t const currTask : tags.m_tasks.bitview().zeros())
    {
        check_task_dependencies(tags, taskData, path, currTask, good);
    }

    return good;
}

} // namespace testapp
