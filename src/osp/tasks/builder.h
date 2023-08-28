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
template <typename TRAITS_T>
struct TaskBuilderBase
{
    using Builder_t     = typename TRAITS_T::Builder_t;
    using TaskRef_t     = typename TRAITS_T::TaskRef_t;

    template <typename ENUM_T>
    using PipelineRef_t = typename TRAITS_T::template PipelineRef_t<ENUM_T>;

    constexpr TaskBuilderBase(Tasks &rTasks, TaskEdges &rEdges) noexcept
     : m_rTasks{rTasks}
     , m_rEdges{rEdges}
    { }

    TaskRef_t task()
    {
        TaskId const taskId = m_rTasks.m_taskIds.create();

        std::size_t const capacity = m_rTasks.m_taskIds.capacity();

        return task(taskId);
    };

    [[nodiscard]] constexpr TaskRef_t task(TaskId const taskId) noexcept
    {
        return TaskRef_t{
            taskId,
            static_cast<Builder_t&>(*this)
        };
    }

    template <typename ENUM_T>
    [[nodiscard]] constexpr PipelineRef_t<ENUM_T> pipeline(PipelineDef<ENUM_T> pipelineDef) noexcept
    {
        return PipelineRef_t<ENUM_T>{
            pipelineDef.m_value,
            static_cast<Builder_t&>(*this)
        };
    }

    template<typename TGT_STRUCT_T>
    TGT_STRUCT_T create_pipelines(osp::ArrayView<PipelineId> const pipelinesOut)
    {
        static_assert(sizeof(TGT_STRUCT_T) % sizeof(PipelineDefBlank_t) == 0);
        constexpr std::size_t count = sizeof(TGT_STRUCT_T) / sizeof(PipelineDefBlank_t);

        LGRN_ASSERTMV(count == pipelinesOut.size() ,
                      "The number of members in TGT_STRUCT_T must match the number of output pipelines",
                      count, pipelinesOut.size());

        m_rTasks.m_pipelineIds.create(pipelinesOut.begin(), pipelinesOut.end());

        std::size_t const capacity = m_rTasks.m_pipelineIds.capacity();

        m_rTasks.m_pipelineInfo   .resize(capacity);
        m_rTasks.m_pipelineControl.resize(capacity);
        m_rTasks.m_pipelineParents.resize(capacity, lgrn::id_null<PipelineId>());

        // Set m_value members of TGT_STRUCT_T, asserted to contain only PipelineDef<...>
        // This is janky enough that rewriting the code below might cause it to ONLY SEGFAULT ON
        // RELEASE AND ISN'T CAUGHT BY ASAN WTF??? (on gcc 11)

        alignas(TGT_STRUCT_T) std::array<unsigned char, sizeof(TGT_STRUCT_T)> bytes;
        TGT_STRUCT_T *pOut = new(bytes.data()) TGT_STRUCT_T;

        for (std::size_t i = 0; i < count; ++i)
        {
            PipelineId const pl = pipelinesOut[i];
            unsigned char *pDefBytes = bytes.data() + sizeof(PipelineDefBlank_t)*i;

            *reinterpret_cast<PipelineId*>(pDefBytes + offsetof(PipelineDefBlank_t, m_value)) = pl;

            m_rTasks.m_pipelineInfo[pl].stageType = *reinterpret_cast<PipelineInfo::stage_type_t*>(pDefBytes + offsetof(PipelineDefBlank_t, m_type));
            m_rTasks.m_pipelineInfo[pl].name      = *reinterpret_cast<std::string_view*>(pDefBytes + offsetof(PipelineDefBlank_t, m_name));
        }

        return *pOut;
    }

    template<typename TGT_STRUCT_T>
    [[nodiscard]] TGT_STRUCT_T create_pipelines()
    {
        static_assert(sizeof(TGT_STRUCT_T) % sizeof(PipelineDefBlank_t) == 0);
        constexpr std::size_t count = sizeof(TGT_STRUCT_T) / sizeof(PipelineDefBlank_t);

        std::array<PipelineId, count> pipelines;

        return create_pipelines<TGT_STRUCT_T>(osp::ArrayView<PipelineId>{pipelines.data(), count});
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


template <typename TRAITS_T>
struct TaskRefBase
{
    using Builder_t     = typename TRAITS_T::Builder_t;
    using TaskRef_t     = typename TRAITS_T::TaskRef_t;

    constexpr operator TaskId() noexcept
    {
        return m_taskId;
    }

    constexpr Tasks & tasks() noexcept { return m_rBuilder.m_rTasks; }

    template<typename RANGE_T>
    TaskRef_t& add_edges(std::vector<TplTaskPipelineStage>& rContainer, RANGE_T const& add)
    {
        for (auto const [pipeline, stage] : add)
        {
            rContainer.push_back({
                .task     = m_taskId,
                .pipeline = pipeline,
                .stage    = stage
            });
        }
        return static_cast<TaskRef_t&>(*this);
    }

    TaskRef_t& run_on(TplPipelineStage const tpl) noexcept
    {
        m_rBuilder.m_rTasks.m_taskRunOn.resize(m_rBuilder.m_rTasks.m_taskIds.capacity());
        m_rBuilder.m_rTasks.m_taskRunOn[m_taskId] = tpl;

        return static_cast<TaskRef_t&>(*this);
    }

    TaskRef_t& schedules(TplPipelineStage const tpl) noexcept
    {
        m_rBuilder.m_rTasks.m_pipelineControl[tpl.pipeline].scheduler = m_taskId;

        return run_on(tpl);
    }

    TaskRef_t& sync_with(ArrayView<TplPipelineStage const> const specs) noexcept
    {
        return add_edges(m_rBuilder.m_rEdges.m_syncWith, specs);
    }

    TaskRef_t& sync_with(std::initializer_list<TplPipelineStage const> specs) noexcept
    {
        return add_edges(m_rBuilder.m_rEdges.m_syncWith, specs);
    }

    TaskId          m_taskId;
    Builder_t       & m_rBuilder;

}; // struct TaskRefBase

template <typename TRAITS_T, typename ENUM_T>
struct PipelineRefBase
{
    using Builder_t     = typename TRAITS_T::Builder_t;
    using PipelineRef_t = typename TRAITS_T::template PipelineRef_t<ENUM_T>;
    using TaskRef_t     = typename TRAITS_T::TaskRef_t;

    constexpr operator PipelineId() noexcept
    {
        return m_pipelineId;
    }

    PipelineRef_t& parent(PipelineId const parent)
    {
        m_rBuilder.m_rTasks.m_pipelineParents[m_pipelineId] = parent;
        return static_cast<PipelineRef_t&>(*this);
    }

    PipelineRef_t& parent_with_schedule(PipelineId const parent)
    {
        m_rBuilder.m_rTasks.m_pipelineParents[m_pipelineId] = parent;

        constexpr ENUM_T const scheduleStage = stage_schedule(ENUM_T{0});
        static_assert(scheduleStage != lgrn::id_null<ENUM_T>(), "Pipeline type has no schedule stage");

        TaskId const scheduler = m_rBuilder.m_rTasks.m_pipelineControl[parent].scheduler;
        LGRN_ASSERTM(scheduler != lgrn::id_null<TaskId>(), "Parent Pipeline has no scheduler task");

        m_rBuilder.m_rEdges.m_syncWith.push_back({
            .task     = scheduler,
            .pipeline = m_pipelineId,
            .stage    = StageId(scheduleStage)
        });

        return static_cast<PipelineRef_t&>(*this);
    }

    PipelineRef_t& loops(bool const loop)
    {
        m_rBuilder.m_rTasks.m_pipelineControl[m_pipelineId].isLoopScope = loop;
        return static_cast<PipelineRef_t&>(*this);
    }

    PipelineRef_t& wait_for_signal(ENUM_T stage)
    {
        m_rBuilder.m_rTasks.m_pipelineControl[m_pipelineId].waitStage = StageId(stage);
        return static_cast<PipelineRef_t&>(*this);
    }

    PipelineId      m_pipelineId;
    Builder_t       & m_rBuilder;

}; // struct TaskRefBase

template<typename FUNC_T>
struct BasicBuilderTraits
{
    using Func_t    = FUNC_T;
    using FuncVec_t = KeyedVec<TaskId, FUNC_T>;

    struct Builder;
    struct TaskRef;

    template <typename PipelineType>
    struct PipelineRef;

    using Builder_t     = Builder;

    using TaskRef_t     = TaskRef;

    template <typename ENUM_T>
    using PipelineRef_t = PipelineRef<ENUM_T>;

    template <typename ENUM_T>
    struct PipelineRef : public PipelineRefBase<BasicBuilderTraits, ENUM_T>
    { };

    struct Builder : public TaskBuilderBase<BasicBuilderTraits>
    {
        Builder(Tasks& rTasks, TaskEdges& rEdges, FuncVec_t& rFuncs)
         : TaskBuilderBase<BasicBuilderTraits>{ rTasks, rEdges }
         , m_rFuncs{rFuncs}
        { }
        Builder(Builder const& copy) = delete;
        Builder(Builder && move) = default;

        Builder& operator=(Builder const& copy) = delete;

        FuncVec_t & m_rFuncs;
    };

    struct TaskRef : public TaskRefBase<BasicBuilderTraits>
    {
        TaskRef& func(FUNC_T && in);
    };

}; // struct BasicBuilderTraits

template<typename FUNC_T>
typename BasicBuilderTraits<FUNC_T>::TaskRef& BasicBuilderTraits<FUNC_T>::TaskRef::func(FUNC_T && in)
{
    this->m_rBuilder.m_rFuncs.resize(this->m_rBuilder.m_rTasks.m_taskIds.capacity());
    this->m_rBuilder.m_rFuncs[this->m_taskId] = std::move(in);
    return *this;
}

} // namespace osp
