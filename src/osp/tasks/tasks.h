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

#include "../keyed_vector.h"

#include <longeron/id_management/registry_stl.hpp>
#include <longeron/containers/intarray_multimap.hpp>

#include <Corrade/Containers/StridedArrayView.h>

#include <cstdint>
#include <vector>

namespace osp
{

enum class TaskId       : uint32_t { };
enum class TargetId     : uint32_t { };
enum class SemaphoreId  : uint32_t { };

struct Tasks
{
    using TaskInt       = uint32_t;
    using TargetInt     = uint32_t;
    using SemaphoreInt  = uint32_t;

    lgrn::IdRegistryStl<TaskId>                     m_taskIds;
    lgrn::IdRegistryStl<TargetId>                   m_targetIds;
    lgrn::IdRegistryStl<SemaphoreId>                m_semaIds;

    KeyedVec<SemaphoreId, unsigned int>             m_semaLimits;

    lgrn::IntArrayMultiMap<TaskInt, TargetId>       m_taskDependOn;         /// Tasks depend on (n) Targets
    lgrn::IntArrayMultiMap<TargetInt, TaskId>       m_targetDependents;     /// Targets have (n) Tasks that depend on it

    lgrn::IntArrayMultiMap<TaskInt, TargetId>       m_taskFulfill;          /// Tasks fulfill (n) Targets
    lgrn::IntArrayMultiMap<TargetInt, TaskId>       m_targetFulfilledBy;    /// Targets are fulfilled by (n) Tasks

    lgrn::IntArrayMultiMap<TaskInt, SemaphoreId>    m_taskAcquire;          /// Tasks acquire (n) Semaphores
    lgrn::IntArrayMultiMap<SemaphoreInt, TaskId>    m_semaAcquiredBy;       /// Semaphores are acquired by (n) Tasks
};

struct ExecutionContext
{

    // Number of times each task is queued to run
    std::vector<unsigned int> m_taskQueuedCounts;
    //std::vector<uint64_t>     m_taskQueuedBits;

    // TODO: Consider multithreading. something something work stealing...
    //  * Allow multiple threads to search for and execute tasks. Atomic access
    //    for ExecutionContext? Might be messy to implement.
    //  * Only allow one thread to search for tasks, assign tasks to other
    //    threads if they're available before running own task. Another thread
    //    can take over once it completes its task. May be faster as only one
    //    thread is modifying ExecutionContext, and easier to implement.
    //  * Plug into an existing work queue library?

}; // struct ExecutionContext

} // namespace osp
