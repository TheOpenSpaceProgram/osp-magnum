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

#include <longeron/id_management/registry_stl.hpp>

#include <Corrade/Containers/ArrayView.h>

#include <cstdint>
#include <vector>

namespace osp
{

using BitSpan_t = Corrade::Containers::ArrayView<uint64_t>;
using BitSpanConst_t = Corrade::Containers::ArrayView<uint64_t const>;


struct TaskTags
{
    enum class Task     : uint32_t {};
    enum class Tag      : uint32_t {};

    lgrn::IdRegistryStl<Task>       m_tasks;
    lgrn::IdRegistryStl<Tag, true>  m_tags;

    // Dependency tags restricts a task from running until all tasks containing
    // tags it depends on are complete.
    // m_tagDepends is partitioned based on m_tagDependsPerTag: AAAABBBBCCCCDDDD
    // lgrn::null<Tag> used as null, or termination per-tag
    std::vector<Tag>                m_tagDepends;
    std::size_t                     m_tagDependsPerTag{8};

    // Limit sets how many tasks using a certain tag can run simultaneously
    std::vector<unsigned int>       m_tagLimits;

    // Bit positions are used to store which tags a task contains
    // partitioned based on tag_ints_per_task(): AAAABBBBCCCCDDDD
    std::vector<uint64_t>           m_taskTags;

    std::size_t tag_ints_per_task() const noexcept { return m_tags.vec().capacity(); }

}; // struct TaskTags


template <typename FUNC_T>
struct TaskFunctions
{
    std::vector<FUNC_T> m_taskFunctions;

}; // struct TaskFunctions

template <typename FUNC_T, typename FUNCB_T>
void task_data(TaskFunctions<FUNC_T> &rFunctions, TaskTags::Task const task, FUNCB_T&& func)
{
    rFunctions.m_taskFunctions.resize(
            std::max(rFunctions.m_taskFunctions.size(), std::size_t(task) + 1));
    rFunctions.m_taskFunctions[std::size_t(task)] = std::forward<FUNCB_T>(func);
}

struct ExecutionContext
{
    // Tag counts from running or queued tasks; used for Dependencies
    std::vector<unsigned int> m_tagIncompleteCounts;
    // Tag counts from running tasks; used for Limits
    std::vector<unsigned int> m_tagRunningCounts;

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
