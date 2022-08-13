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

#include <Corrade/Containers/StridedArrayView.h>

#include <cstdint>
#include <vector>

namespace osp
{

using bit_int_t = uint64_t;

enum class TaskId : uint32_t {};
enum class TagId  : uint32_t {};

struct Tags
{
    lgrn::IdRegistryStl<TagId, true>    m_tags;

    // Dependency tags restricts a task from running until all tasks containing
    // tags it depends on are complete.
    // 2D with m_tagDependsPerTag elements per row, [tag][dependency]
    // lgrn::null<Tag> used as null, or termination per-tag
    std::vector<TagId>                  m_tagDepends;
    std::size_t                         m_tagDependsPerTag{8};

    // Limit sets how many tasks using a certain tag can run simultaneously
    std::vector<unsigned int>           m_tagLimits;

    // Enqueues another tag when all task finish
    std::vector<TagId>                  m_tagEnqueues;

    // Restricts associated enqueued tasks from running until externally set
    // Resets once all associated tasks are complete
    std::vector<bit_int_t>              m_tagExtern;

    [[nodiscard]] std::size_t constexpr tag_ints_per_task() const noexcept { return m_tags.vec().size(); }

}; // struct Tags

struct Tasks
{
    lgrn::IdRegistryStl<TaskId>         m_tasks;

    // Bit positions are used to store which tags a task contains.
    // 2D with Tags::m_tags.vec().size() elements per row. [task][tagsInt]
    // partitioned based on Tags::tag_ints_per_task(): AAAABBBBCCCCDDDD
    std::vector<bit_int_t>              m_taskTags;

}; // struct Tasks

constexpr Corrade::Containers::StridedArrayView2D<TagId> tag_depends_2d(Tags& rTags) noexcept
{
    return {Corrade::Containers::arrayView(rTags.m_tagDepends.data(), rTags.m_tagDepends.size()),
            {rTags.m_tags.capacity(), rTags.m_tagDependsPerTag}};
}

constexpr Corrade::Containers::StridedArrayView2D<TagId const> tag_depends_2d(Tags const& rTags) noexcept
{
    return {Corrade::Containers::arrayView(rTags.m_tagDepends.data(), rTags.m_tagDepends.size()),
            {rTags.m_tags.capacity(), rTags.m_tagDependsPerTag}};
}

constexpr Corrade::Containers::StridedArrayView2D<bit_int_t> task_tags_2d(Tags const& rTags, Tasks &rTasks) noexcept
{
    return {Corrade::Containers::arrayView(rTasks.m_taskTags.data(), rTasks.m_taskTags.size()),
            {rTasks.m_tasks.capacity(), rTags.tag_ints_per_task()}};
}

constexpr Corrade::Containers::StridedArrayView2D<bit_int_t const> task_tags_2d(Tags const& rTags, Tasks const &rTasks) noexcept
{
    return {Corrade::Containers::arrayView(rTasks.m_taskTags.data(), rTasks.m_taskTags.size()),
            {rTasks.m_tasks.capacity(), rTags.tag_ints_per_task()}};
}

template <typename DATA_T>
struct TaskDataVec
{
    std::vector<DATA_T> m_taskData;

}; // struct TaskDataVec

template <typename DATA_T, typename RHS_T>
void task_data(TaskDataVec<DATA_T> &rData, TaskId const task, RHS_T&& rhs)
{
    rData.m_taskData.resize(
            std::max(rData.m_taskData.size(), std::size_t(task) + 1));
    rData.m_taskData[std::size_t(task)] = std::forward<RHS_T>(rhs);
}

struct ExecutionContext
{
    // Tag counts from running or queued tasks; used for Dependencies
    std::vector<unsigned int> m_tagIncompleteCounts;
    // Tag counts from running tasks; used for Limits
    std::vector<unsigned int> m_tagRunningCounts;
    // External tags
    std::vector<uint64_t> m_tagExternTriggers;

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
