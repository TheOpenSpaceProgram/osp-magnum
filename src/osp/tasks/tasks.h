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
#include "../types.h"

#include <longeron/id_management/registry_stl.hpp>
#include <longeron/containers/intarray_multimap.hpp>

#include <cstdint>
#include <ostream>
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
};

struct TaskEdges
{
    struct TargetPair
    {
        TaskId      m_task;
        TargetId    m_target;
        bool        m_trigger; // unused for Fulfill edges
    };

    struct SemaphorePair
    {
        TaskId      m_task;
        SemaphoreId m_sema;
    };

    std::vector<TargetPair>             m_targetDependEdges;
    std::vector<TargetPair>             m_targetFulfillEdges;
    std::vector<SemaphorePair>          m_semaphoreEdges;
};

struct DependOn
{
    TargetId    m_target;
    bool        m_trigger;

    constexpr operator TargetId() const noexcept { return m_target; }
};

struct ExecGraph
{
    lgrn::IntArrayMultiMap<Tasks::TaskInt, DependOn>        m_taskDependOn;         /// Tasks depend on (n) Targets
    lgrn::IntArrayMultiMap<Tasks::TargetInt, TaskId>        m_targetDependents;     /// Targets have (n) Tasks that depend on it

    lgrn::IntArrayMultiMap<Tasks::TaskInt, TargetId>        m_taskFulfill;          /// Tasks fulfill (n) Targets
    lgrn::IntArrayMultiMap<Tasks::TargetInt, TaskId>        m_targetFulfilledBy;    /// Targets are fulfilled by (n) Tasks

    lgrn::IntArrayMultiMap<Tasks::TaskInt, SemaphoreId>     m_taskAcquire;          /// Tasks acquire (n) Semaphores
    lgrn::IntArrayMultiMap<Tasks::SemaphoreInt, TaskId>     m_semaAcquiredBy;       /// Semaphores are acquired by (n) Tasks
};

/**
 * @brief Bitset returned by tasks to determine which fulfill targets should be marked dirty
 */
using FulfillDirty_t = lgrn::BitView<std::array<uint64_t, 1>>;


ExecGraph make_exec_graph(Tasks const& tasks, ArrayView<TaskEdges const* const> data);

inline ExecGraph make_exec_graph(Tasks const& tasks, std::initializer_list<TaskEdges const* const> data)
{
    return make_exec_graph(tasks, arrayView(data));
}

} // namespace osp
