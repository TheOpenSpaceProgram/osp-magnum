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

void top_run_blocking(Tags const& tags, Tasks const& tasks, TopTaskDataVec_t& rTaskData, ArrayView<entt::any> topData, ExecutionContext& rExec)
{
    std::vector<entt::any> topDataRefs;

    std::vector<uint64_t> enqueue(tags.m_tags.vec().size());

    std::vector<uint64_t> tasksToRun(tasks.m_tasks.vec().size());


    // Run until there's no tasks left to run
    while (true)
    {
        std::ranges::fill(tasksToRun, 0u);
        task_list_available(tags, tasks, rExec, tasksToRun);

        auto const tasksToRunBits = lgrn::bit_view(tasksToRun);
        unsigned int const availableCount = tasksToRunBits.count();

        if (availableCount == 0)
        {
            break;
        }

        // Choose first available task
        auto const task = TaskId(*tasksToRunBits.ones().begin());

        task_start(tags, tasks, rExec, task);

        TopTask &rTopTask = rTaskData.m_taskData.at(std::size_t(task));

        topDataRefs.clear();
        topDataRefs.reserve(rTopTask.m_dataUsed.size());
        for (TopDataId const dataId : rTopTask.m_dataUsed)
        {

            topDataRefs.push_back((dataId != lgrn::id_null<TopDataId>())
                                   ? topData[dataId].as_ref()
                                   : entt::any{});
        }

        bool enqueueHappened = false;

        // Task actually runs here. Results are not yet used for anything.
        rTopTask.m_func(WorkerContext{{}, {enqueue}, enqueueHappened}, topDataRefs);

        if (enqueueHappened)
        {
            task_enqueue(tags, tasks, rExec, enqueue);
            std::ranges::fill(enqueue, 0u);
        }

        task_finish(tags, tasks, rExec, task);
    }
}

void top_enqueue_quick(Tags const& tags, Tasks const& tasks, ExecutionContext& rExec, ArrayView<TagId const> tagsEnq)
{
    rExec.m_tagIncompleteCounts  .resize(tags.m_tags.capacity(), 0);
    rExec.m_tagRunningCounts     .resize(tags.m_tags.capacity(), 0);
    rExec.m_taskQueuedCounts     .resize(tasks.m_tasks.capacity(), 0);

    std::vector<uint64_t> tagsToRun(tags.m_tags.vec().size());
    to_bitspan(tagsEnq, tagsToRun);
    task_enqueue(tags, tasks, rExec, tagsToRun);
}

static void check_task_dependencies(Tags const& tags, Tasks const& tasks, TopTaskDataVec_t const& taskData, ExecutionContext const &rExec, std::vector<uint32_t> &rPath, uint32_t const task, bool& rGood)
{
    if (rExec.m_taskQueuedCounts.at(task) == 0)
    {
        return; // Only consider queued tasks
    }

    rPath.push_back(task);

    auto const taskTags2d   = task_tags_2d(tags, tasks);
    auto const tagDepends2d = tag_depends_2d(tags);

    std::vector<uint64_t> dependencyTagInts(tags.m_tags.vec().size(), 0x0);
    auto dependencyTagsBits = lgrn::bit_view(dependencyTagInts);

    // Loop through each of this task's tags to get dependent tags
    auto const taskTagsInts = taskTags2d[task].asContiguous();
    auto const view = lgrn::bit_view(taskTagsInts);
    for (uint32_t const currTag : view.ones())
    {
        auto const currTagDepends = tagDepends2d[currTag].asContiguous();

        for (TagId const dependTag : currTagDepends)
        {
            if (dependTag == lgrn::id_null<TagId>())
            {
                break;
            }

            dependencyTagsBits.set(std::size_t(dependTag));
        }
    }

    // Get all tasks that contain any of the dependent tags
    for (uint32_t const currTask : tasks.m_tasks.bitview().zeros())
    {
        if (any_bits_match(taskTags2d[currTask].asContiguous(), dependencyTagInts))
        {
            auto const &last = std::end(rPath);
            if (last == std::find(std::begin(rPath), last, currTask))
            {
                check_task_dependencies(tags, tasks, taskData, rExec, rPath, currTask, rGood);
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

bool debug_top_print_deadlock(Tags const& tags, Tasks const& tasks, TopTaskDataVec_t const& taskData, ExecutionContext const &rExec)
{
    std::vector<uint32_t> path;

    bool good = true;

    // Iterate all tasks
    for (uint32_t const currTask : tasks.m_tasks.bitview().zeros())
    {
        check_task_dependencies(tags, tasks, taskData, rExec, path, currTask, good);
    }

    return good;
}

} // namespace testapp
