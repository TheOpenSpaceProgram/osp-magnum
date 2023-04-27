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
#pragma once

#include "tasks.h"

#include "../bitvector.h"

#include <Corrade/Containers/ArrayViewStl.h>
#include <Corrade/Containers/StridedArrayView.h>

#include <cassert>
#include <vector>

namespace osp
{

struct ExecContext
{
    void resize(Tasks const& tasks)
    {
        std::size_t const maxTargets    = tasks.m_targetIds.capacity();
        std::size_t const maxTasks      = tasks.m_taskIds.capacity();

        bitvector_resize(m_tasksQueued,         maxTasks);
        bitvector_resize(m_targetDirty,         maxTargets);
        bitvector_resize(m_targetWillBePending, maxTargets);
        m_targetPendingCount.resize(maxTargets);
        m_targetInUseCount  .resize(maxTargets);
    }

    BitVector_t                 m_tasksQueued;
    BitVector_t                 m_targetDirty;
    BitVector_t                 m_targetWillBePending;
    KeyedVec<TargetId, int>     m_targetPendingCount;
    KeyedVec<TargetId, int>     m_targetInUseCount;

    // TODO: Consider multithreading. something something work stealing...
    //  * Allow multiple threads to search for and execute tasks. Atomic access
    //    for ExecContext? Might be messy to implement.
    //  * Only allow one thread to search for tasks, assign tasks to other
    //    threads if they're available before running own task. Another thread
    //    can take over once it completes its task. May be faster as only one
    //    thread is modifying ExecContext, and easier to implement.
    //  * Plug into an existing work queue library?

}; // struct ExecContext

int enqueue_dirty(Tasks const& tasks, ExecGraph const& graph, ExecContext &rExec) noexcept;

void mark_completed_task(Tasks const& tasks, ExecGraph const& graph, ExecContext &rExec, TaskId const task, FulfillDirty_t dirty) noexcept;



} // namespace osp
