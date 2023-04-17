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
#include "builder.h"

namespace osp
{

Tasks finalize(TaskBuilderData && data)
{
    using TargetCounts  = TaskBuilderData::TargetCounts;
    using TaskCounts    = TaskBuilderData::TaskCounts;
    using TargetInt     = Tasks::TargetInt;
    using TaskInt       = Tasks::TaskInt;

    Tasks out;

    KeyedVec<TaskId, TaskCounts>        taskCounts;
    KeyedVec<TargetId, TargetCounts>    targetCounts;

    taskCounts  .resize(data.m_taskIds  .capacity());
    targetCounts.resize(data.m_targetIds.capacity());

    // Count connections

    for (auto const [task, target] : data.m_targetDependEdges)
    {
        ++ taskCounts[task]    .m_dependingOn;
        ++ targetCounts[target].m_dependents;
    }

    for (auto const [task, target] : data.m_targetFulfillEdges)
    {
        ++ taskCounts[task]    .m_fulfills;
        ++ targetCounts[target].m_fulfilledBy;
    }

    // Allocate

    out.m_taskDependOn      .ids_reserve  (data.m_taskIds.capacity());
    out.m_taskDependOn      .data_reserve (data.m_targetDependEdges.size());
    out.m_targetDependents  .ids_reserve  (data.m_targetIds.capacity());
    out.m_targetDependents  .data_reserve (data.m_targetDependEdges.size());
    out.m_taskFulfill       .ids_reserve  (data.m_taskIds.capacity());
    out.m_taskFulfill       .data_reserve (data.m_targetFulfillEdges.size());
    out.m_targetFulfilledBy .ids_reserve  (data.m_targetIds.capacity());
    out.m_targetFulfilledBy .data_reserve (data.m_targetFulfillEdges.size());

    // Reserve partitions

    for (Tasks::TaskInt const task : data.m_taskIds.bitview().zeros())
    {
        if (std::size_t const size = taskCounts[TaskId(task)].m_dependingOn;
            size != 0)
        {
            out.m_taskDependOn.emplace(task, size);
        }
        if (std::size_t const size = taskCounts[TaskId(task)].m_fulfills;
            size != 0)
        {
            out.m_taskFulfill.emplace(task, size);
        }
    }

    for (Tasks::TargetInt const target : data.m_targetIds.bitview().zeros())
    {
        if (std::size_t const size = targetCounts[TargetId(target)].m_dependents;
            size != 0)
        {
            out.m_targetDependents.emplace(target, size);
        }

        if (std::size_t const size = targetCounts[TargetId(target)].m_fulfilledBy;
            size != 0)
        {
            out.m_targetFulfilledBy.emplace(target, size);
        }
    }

    // Place

    for (auto const [task, target] : data.m_targetDependEdges)
    {
        --taskCounts[task]    .m_dependingOn;
        --targetCounts[target].m_dependents;

        out.m_taskDependOn     [TaskInt(task)]     [taskCounts[task].m_dependingOn]     = target;
        out.m_targetDependents [TargetInt(target)] [targetCounts[target].m_dependents]  = task;
    }

    for (auto const [task, target] : data.m_targetFulfillEdges)
    {
        -- taskCounts[task]    .m_fulfills;
        -- targetCounts[target].m_fulfilledBy;

        out.m_taskFulfill       [TaskInt(task)]     [taskCounts[task].m_fulfills]        = target;
        out.m_targetFulfilledBy [TargetInt(target)] [targetCounts[target].m_fulfilledBy] = task;
    }

    out.m_taskIds       = std::move(data.m_taskIds);
    out.m_targetIds     = std::move(data.m_targetIds);
    out.m_semaIds       = std::move(data.m_semaIds);
    out.m_semaLimits    = std::move(data.m_semaLimits);

    return out;
}



} // namespace osp

