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

#include <cassert>
#include <cstdint>
#include <vector>

namespace osp
{

/**
 * @brief A convenient interface for setting up Tasks and required task data
 */
template <typename TASKBUILDER_T, typename TASKREF_T>
struct TaskBuilderBase
{
    constexpr TaskBuilderBase(Tasks &rTasks, TaskEdges &rEdges) noexcept
     : m_rTasks{rTasks}
     , m_rEdges{rEdges}
    { }

    TASKREF_T task()
    {
        TaskId const taskId = m_rTasks.m_taskIds.create();

        std::size_t const capacity = m_rTasks.m_taskIds.capacity();

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
    TGT_STRUCT_T create_pipelines()
    {
        static_assert(sizeof(TGT_STRUCT_T) % sizeof(PipelineId) == 0);
        constexpr std::size_t count = sizeof(TGT_STRUCT_T) / sizeof(PipelineId);

        std::array<PipelineId, count> out;

        m_rTasks.m_pipelineIds.create(out.begin(), out.end());

        return reinterpret_cast<TGT_STRUCT_T&>(*out.data());
    }

    template<std::size_t N>
    std::array<PipelineId, N> create_pipelines()
    {
        std::array<PipelineId, N> out;
        m_rTasks.m_pipelineIds.create(out.begin(), out.end());
        return out;
    }

    Tasks       & m_rTasks;
    TaskEdges   & m_rEdges;

}; // class TaskBuilderBase

struct PipelineSpec
{
    template<typename STAGE_ENUM_T>
    PipelineSpec(PipelineDef<STAGE_ENUM_T> const pipeline, STAGE_ENUM_T const stage)
     : m_pipeline{pipeline}
     , m_stage{StageId(stage)}
    { }

    PipelineId  m_pipeline;
    StageId     m_stage;
};

template <typename TASKBUILDER_T, typename TASKREF_T>
struct TaskRefBase
{
    struct Empty {};

    constexpr operator TaskId() noexcept
    {
        return m_taskId;
    }

    constexpr Tasks & tasks() noexcept { return m_rBuilder.m_rTasks; }

    template<typename RANGE_T>
    TASKREF_T& add_edges(std::vector<TplTaskPipelineStage>& rContainer, RANGE_T const& add)
    {
        for (PipelineSpec const spec : add)
        {
            rContainer.push_back({
                .task     = m_taskId,
                .pipeline = spec.m_pipeline,
                .stage    = spec.m_stage
            });
        }
        return static_cast<TASKREF_T&>(*this);
    }

    TASKREF_T& run_on(ArrayView<PipelineSpec const> const specs) noexcept
    {
        return add_edges(m_rBuilder.m_rEdges.m_runOn, specs);
    }

    TASKREF_T& run_on(std::initializer_list<PipelineSpec const> specs) noexcept
    {
        return add_edges(m_rBuilder.m_rEdges.m_runOn, specs);
    }

    TASKREF_T& sync_with(ArrayView<PipelineSpec const> const specs) noexcept
    {
        return add_edges(m_rBuilder.m_rEdges.m_syncWith, specs);
    }

    TASKREF_T& sync_with(std::initializer_list<PipelineSpec const> specs) noexcept
    {
        return add_edges(m_rBuilder.m_rEdges.m_syncWith, specs);
    }

    TASKREF_T& triggers(ArrayView<PipelineSpec const> const specs) noexcept
    {
        return add_edges(m_rBuilder.m_rEdges.m_triggers, specs);
    }

    TASKREF_T& triggers(std::initializer_list<PipelineSpec const> specs) noexcept
    {
        return add_edges(m_rBuilder.m_rEdges.m_triggers, specs);
    }

    TaskId          m_taskId;
    TASKBUILDER_T   & m_rBuilder;

}; // struct TaskRefBase

template<typename FUNC_T>
struct BasicBuilderTraits
{
    using Func_t    = FUNC_T;
    using FuncVec_t = KeyedVec<TaskId, FUNC_T>;

    struct Builder;

    struct Ref : public TaskRefBase<Builder, Ref>
    {
        Ref& func(FUNC_T && in);
    };

    struct Builder : public TaskBuilderBase<Builder, Ref>
    {
        Builder(Tasks& rTasks, TaskEdges& rEdges, FuncVec_t& rFuncs)
         : TaskBuilderBase<Builder, Ref>{ rTasks, rEdges }
         , m_rFuncs{rFuncs}
        { }
        Builder(Builder const& copy) = delete;
        Builder(Builder && move) = default;

        Builder& operator=(Builder const& copy) = delete;

        FuncVec_t & m_rFuncs;
    };
};

template<typename FUNC_T>
typename BasicBuilderTraits<FUNC_T>::Ref& BasicBuilderTraits<FUNC_T>::Ref::func(FUNC_T && in)
{
    this->m_rBuilder.m_rFuncs.resize(this->m_rBuilder.m_rTasks.m_taskIds.capacity());
    this->m_rBuilder.m_rFuncs[this->m_taskId] = std::move(in);
    return *this;
}

} // namespace osp
