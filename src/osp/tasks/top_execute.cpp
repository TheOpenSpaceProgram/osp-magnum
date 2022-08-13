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
    std::vector<entt::any> dataRefs;
    WorkerContext context;

    std::vector<uint64_t> tmpEnqueue(tags.m_tags.vec().size());

    // Run until there's no tasks left to run
    while (true)
    {
        std::vector<uint64_t> tasksToRun(tasks.m_tasks.vec().size());
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
        dataRefs.resize(rTopTask.m_dataUsed.size());
        for (std::size_t i = 0; i < rTopTask.m_dataUsed.size(); ++i)
        {
            dataRefs.at(i) = topData[rTopTask.m_dataUsed[i]].as_ref();
        }

        TopTaskStatus const status = rTopTask.m_func(context, dataRefs);

        BitSpan_t enqueue = (status == TopTaskStatus::Success) ? tmpEnqueue : BitSpan_t{};

        task_finish(tags, tasks, rExec, task, enqueue);
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

static void check_task_dependencies(Tags const& tags, Tasks const& tasks, TopTaskDataVec_t const& taskData, std::vector<uint32_t> &rPath, uint32_t const task, bool& rGood)
{
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
                check_task_dependencies(tags, tasks, taskData, rPath, currTask, rGood);
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

bool debug_top_verify(Tags const& tags, Tasks const& tasks, TopTaskDataVec_t const& taskData)
{
    std::vector<uint32_t> path;

    bool good = true;

    // Iterate all tasks
    for (uint32_t const currTask : tasks.m_tasks.bitview().zeros())
    {
        check_task_dependencies(tags, tasks, taskData, path, currTask, good);
    }

    return good;
}

void top_close_session(Tags& rTags, Tasks& rTasks, TopTaskDataVec_t& rTaskData, ArrayView<entt::any> topData, ExecutionContext& rExec, Session &rSession)
{
    // Run cleanup tasks
    if (TagId const tgCleanup = std::exchange(rSession.m_onCleanup, lgrn::id_null<TagId>());
        tgCleanup != lgrn::id_null<TagId>())
    {
        top_enqueue_quick(rTags, rTasks, rExec, {tgCleanup});
        top_run_blocking(rTags, rTasks, rTaskData, topData, rExec);
    }

    // Clear session's TopData
    for (TopDataId const id : std::exchange(rSession.m_dataIds, {}))
    {
        if (id != lgrn::id_null<TopDataId>())
        {
            topData[std::size_t(id)].reset();
        }
    }

    // Get this session's tags, and clear its tasks
    std::vector<uint64_t> tagsOwnedBits(rTags.m_tags.vec().size());
    to_bitspan(rSession.m_tags, tagsOwnedBits);
    task_for_each(rTags, rTasks, [&tagsOwnedBits, &rTasks, &rTaskData]
            (uint32_t const currTask, ArrayView<uint64_t const> const currTags)
    {
        if (any_bits_match(tagsOwnedBits, currTags))
        {
            rTasks.m_tasks.remove(TaskId(currTask));
            TopTask &rCurrTaskData = rTaskData.m_taskData[currTask];
            rCurrTaskData.m_dataUsed.clear();
            rCurrTaskData.m_func = nullptr;
        }
    });

    // Clear tags
    for (TagId const tag : std::exchange(rSession.m_tags, {}))
    {
        if (tag != lgrn::id_null<TagId>())
        {
            rTags.m_tags.remove(tag);
        }
    }
}

} // namespace testapp
