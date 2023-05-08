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

#include "execute.h"


namespace osp
{

static bool try_enqueue_task(Tasks const& tasks, ExecGraph const& graph, ExecContext &rExec, TaskId const task, TargetId const calledFrom) noexcept
{
    if (rExec.m_tasksQueued.contains(task))
    {
        return false; // task already queued
    }

    bool const prevTriggered    = rExec.m_taskTriggered.test(std::size_t(task));
    bool justTriggered          = false;
    bool dontEnqueueYet         = false;

    for (DependOn const dependOn : graph.m_taskDependOn[std::size_t(task)])
    {
        // If true: depend-on target is pending, wait for other tasks to finish
        dontEnqueueYet |= rExec.m_targetPendingCount[dependOn.m_target] != 0;

        // If true: depend-on target is fulfilled by a task that will be enqueued, wait for it
        dontEnqueueYet |= rExec.m_targetWillBePending.test(std::size_t(dependOn.m_target));

        if ( ! prevTriggered && dependOn.m_target == calledFrom)
        {
            justTriggered = dependOn.m_trigger;
        }
    }

    // 'Triggered' indicates that one of the (trigger=true) depend-on targets became dirty for the
    // period of time since this task last ran.
    if (justTriggered)
    {
        rExec.m_taskTriggered.set(std::size_t(task));
    }

    if (dontEnqueueYet)
    {
        return false;
    }

    // At least one of the trigger depend-on targets must have been dirty previously to enqueue
    if ( ! (justTriggered || prevTriggered) )
    {
        return false;
    }

    // Enqueue task

    for (DependOn const dependOn : graph.m_taskDependOn[std::size_t(task)])
    {
        ++ rExec.m_targetInUseCount[dependOn.m_target];
    }

    for (TargetId const fulfill : graph.m_taskFulfill[std::size_t(task)])
    {
        ++ rExec.m_targetPendingCount[fulfill];
    }

    rExec.m_tasksQueued.emplace(task);

    return true;
}

int enqueue_dirty(Tasks const& tasks, ExecGraph const& graph, ExecContext &rExec) noexcept
{
    // Find out which targets will be pending
    for (std::size_t const target : rExec.m_targetDirty.ones())
    {
        rExec.m_targetDirty.set(target);

        for (TaskId const dependent : graph.m_targetDependents[target])
        {
            for (TargetId const fulfill : graph.m_taskFulfill[std::size_t(dependent)])
            {
                rExec.m_targetWillBePending.set(std::size_t(fulfill));
            }
        }
    }

    int tasksEnqueued = 0;

    for (std::size_t const target : rExec.m_targetDirty.ones())
    {
        for (TaskId const dependent : graph.m_targetDependents[target])
        {
            tasksEnqueued += int(try_enqueue_task(tasks, graph, rExec, dependent, TargetId(target)));
        }
    }
    rExec.m_targetWillBePending.reset();
    rExec.m_targetDirty.reset();

    return tasksEnqueued;
}

void mark_completed_task(Tasks const& tasks, ExecGraph const& graph, ExecContext &rExec, TaskId const task, FulfillDirty_t dirty) noexcept
{
    LGRN_ASSERTMV(rExec.m_tasksQueued.contains(task),
                  "Task must be queued to have been allowed to run", std::size_t(task));

    rExec.m_taskTriggered.reset(std::size_t(task));

    for (DependOn const dependOn : graph.m_taskDependOn[std::size_t(task)])
    {
        -- rExec.m_targetInUseCount[dependOn.m_target];
    }

    auto const taskFulfill = lgrn::Span<TargetId const>(graph.m_taskFulfill[std::size_t(task)]);

    for (int i = 0; i < taskFulfill.size(); ++i)
    {
        TargetId const fulfill = taskFulfill[i];

        -- rExec.m_targetPendingCount[fulfill];

        if (rExec.m_targetPendingCount[fulfill] == 0)
        {
            if (dirty.test(i))
            {
                rExec.m_targetDirty.set(std::size_t(fulfill));
            }
        }
    }

    rExec.m_tasksQueued.erase(task);
}

} // namespace osp
