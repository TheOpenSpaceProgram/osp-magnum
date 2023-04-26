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
#include "tasks.h"

namespace osp
{

struct TaskCounts
{
    unsigned int m_dependingOn  {0};
    unsigned int m_fulfills     {0};
    unsigned int m_acquires     {0};
};

struct TargetCounts
{
    unsigned int m_dependents   {0};
    unsigned int m_fulfilledBy  {0};
};


ExecGraph make_exec_graph(Tasks const& tasks, ArrayView<TaskEdges const* const> const data)
{
    using TargetInt     = Tasks::TargetInt;
    using TaskInt       = Tasks::TaskInt;

    ExecGraph out;

    KeyedVec<TaskId, TaskCounts>        taskCounts;
    KeyedVec<TargetId, TargetCounts>    targetCounts;

    taskCounts  .resize(tasks.m_taskIds  .capacity());
    targetCounts.resize(tasks.m_targetIds.capacity());

    // Count connections

    std::size_t totalDependEdges = 0;
    std::size_t totalFulfillEdges = 0;

    for (TaskEdges const* pEdges : data)
    {
        totalDependEdges += pEdges->m_targetDependEdges.size();
        totalFulfillEdges += pEdges->m_targetFulfillEdges.size();

        for (auto const [task, target] : pEdges->m_targetDependEdges)
        {
            ++ taskCounts[task]    .m_dependingOn;
            ++ targetCounts[target].m_dependents;
        }

        for (auto const [task, target] : pEdges->m_targetFulfillEdges)
        {
            ++ taskCounts[task]    .m_fulfills;
            ++ targetCounts[target].m_fulfilledBy;
        }
    }

    // Allocate

    out.m_taskDependOn      .ids_reserve  (tasks.m_taskIds.capacity());
    out.m_taskDependOn      .data_reserve (totalDependEdges);
    out.m_targetDependents  .ids_reserve  (tasks.m_targetIds.capacity());
    out.m_targetDependents  .data_reserve (totalDependEdges);
    out.m_taskFulfill       .ids_reserve  (tasks.m_taskIds.capacity());
    out.m_taskFulfill       .data_reserve (totalFulfillEdges);
    out.m_targetFulfilledBy .ids_reserve  (tasks.m_targetIds.capacity());
    out.m_targetFulfilledBy .data_reserve (totalFulfillEdges);

    // Reserve partitions

    for (Tasks::TaskInt const task : tasks.m_taskIds.bitview().zeros())
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

    for (Tasks::TargetInt const target : tasks.m_targetIds.bitview().zeros())
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

    // Place connections into allocated partitions

    for (TaskEdges const* pEdges : data)
    {
        for (auto const [task, target] : pEdges->m_targetDependEdges)
        {
            auto const dependOn     = lgrn::Span<TargetId>  (out.m_taskDependOn[TaskInt(task)]);
            auto const dependents   = lgrn::Span<TaskId>    (out.m_targetDependents[TargetInt(target)]);

            dependOn    [dependOn.size()    - taskCounts[task].m_dependingOn]       = target;
            dependents  [dependents.size()  - targetCounts[target].m_dependents]    = task;

            --taskCounts[task]    .m_dependingOn;
            --targetCounts[target].m_dependents;
        }

        for (auto const [task, target] : pEdges->m_targetFulfillEdges)
        {
            auto const fulfills     = lgrn::Span<TargetId>  (out.m_taskFulfill[TaskInt(task)]);
            auto const fulfilledBy  = lgrn::Span<TaskId>    (out.m_targetFulfilledBy[TargetInt(target)]);

            fulfills    [fulfills.size()    - taskCounts[task].m_fulfills]          = target;
            fulfilledBy [fulfilledBy.size() - targetCounts[target].m_fulfilledBy]   = task;

            -- taskCounts[task]    .m_fulfills;
            -- targetCounts[target].m_fulfilledBy;
        }
    }


    return out;
}



} // namespace osp

