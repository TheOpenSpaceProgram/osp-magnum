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
#include "../types.h"

#include <iterator>
#include <longeron/containers/bit_view.hpp>

#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/ArrayViewStl.h>

#include <cassert>
#include <cstdint>
#include <vector>

namespace osp
{

struct TaskBuilderData
{
    struct TaskTargetPair
    {
        TaskId m_task;
        TargetId m_target;
    };

    struct TaskSemaphorePair
    {
        TaskId m_task;
        SemaphoreId m_sema;
    };

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

    lgrn::IdRegistryStl<TaskId>             m_taskIds;
    lgrn::IdRegistryStl<TargetId>           m_targetIds;
    lgrn::IdRegistryStl<SemaphoreId>        m_semaIds;
    KeyedVec<SemaphoreId, unsigned int>     m_semaLimits;

    std::vector<TaskTargetPair>             m_targetDependEdges;
    std::vector<TaskTargetPair>             m_targetFulfillEdges;
    std::vector<TaskSemaphorePair>          m_semaphoreEdges;

}; // TaskBuilderData

/**
 * @brief A convenient interface for setting up Tasks and required task data
 */
template <typename TASKBUILDER_T, typename TASKREF_T>
struct TaskBuilderBase : TaskBuilderData
{
public:

    TASKREF_T task()
    {
        TaskId const taskId = m_taskIds.create();

        std::size_t const capacity = m_taskIds.capacity();

        return task(taskId);
    };

    constexpr TASKREF_T task(TaskId const taskId) noexcept
    {
        return TASKREF_T{
            taskId,
            static_cast<TASKBUILDER_T&>(*this)
        };
    }

    template<typename TGT_STRUCT_T>
    TGT_STRUCT_T create_targets()
    {
        static_assert(sizeof(TGT_STRUCT_T) % sizeof(TargetId) == 0);
        constexpr std::size_t count = sizeof(TGT_STRUCT_T) / sizeof(TargetId);

        std::array<TargetId, count> out;

        m_targetIds.create(out.begin(), out.end());

        return reinterpret_cast<TGT_STRUCT_T&>(*out.data());
    }

}; // class TaskBuilderBase


template <typename TASKBUILDER_T, typename TASKREF_T>
struct TaskRefBase
{
    struct Empty {};

    constexpr operator TaskId() noexcept
    {
        return m_taskId;
    }

    constexpr Tasks & tasks() noexcept { return m_rBuilder.m_rTasks; }

    TASKREF_T& depends_on(ArrayView<TargetId const> const targets) noexcept
    {
        for (TargetId const target : targets)
        {
            m_rBuilder.m_targetDependEdges.push_back({m_taskId, target});
        }
        return static_cast<TASKREF_T&>(*this);
    }

    TASKREF_T& depends_on(std::initializer_list<TargetId const> targets) noexcept
    {
        return depends_on(Corrade::Containers::arrayView(targets));
    }

    TASKREF_T& fulfills(ArrayView<TargetId const> const targets) noexcept
    {
        for (TargetId const target : targets)
        {
            m_rBuilder.m_targetFulfillEdges.push_back({m_taskId, target});
        }
        return static_cast<TASKREF_T&>(*this);
    }

    TASKREF_T& fulfills(std::initializer_list<TargetId const> targets) noexcept
    {
        return fulfills(Corrade::Containers::arrayView(targets));
    }

    TaskId          m_taskId;
    TASKBUILDER_T   & m_rBuilder;

}; // struct TaskRefBase

Tasks finalize(TaskBuilderData && data);

} // namespace osp
