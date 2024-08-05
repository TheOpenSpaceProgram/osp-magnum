/**
 * Open Space Program
 * Copyright © 2019-2024 Open Space Program Project
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

#include <osp/tasks/tasks.h>

#include <array>
#include <cassert>
#include <vector>

namespace osp
{

template<typename FUNC_T>
struct TaskRef
{
    using FuncVec_t = KeyedVec<TaskId, FUNC_T>;

    constexpr operator TaskId() noexcept
    {
        return taskId;
    }

    template<typename RANGE_T>
    TaskRef& add_edges(std::vector<TplTaskPipelineStage>& rContainer, RANGE_T const& add)
    {
        for (auto const [pipeline, stage] : add)
        {
            rContainer.push_back({
                .task     = taskId,
                .pipeline = pipeline,
                .stage    = stage
            });
        }
        return static_cast<TaskRef&>(*this);
    }

    TaskRef& run_on(TplPipelineStage const tpl) noexcept
    {
        rTasks.m_taskRunOn.resize(rTasks.m_taskIds.capacity());
        rTasks.m_taskRunOn[taskId] = tpl;

        return static_cast<TaskRef&>(*this);
    }

    TaskRef& schedules(TplPipelineStage const tpl) noexcept
    {
        rTasks.m_pipelineControl[tpl.pipeline].scheduler = taskId;

        return run_on(tpl);
    }

    TaskRef& sync_with(ArrayView<TplPipelineStage const> const specs) noexcept
    {
        return add_edges(rTasks.m_syncWith, specs);
    }

    TaskRef& sync_with(std::initializer_list<TplPipelineStage const> specs) noexcept
    {
        return add_edges(rTasks.m_syncWith, specs);
    }

    TaskRef& func(FUNC_T && in)
    {
        rFuncs.resize(rTasks.m_taskIds.capacity());
        rFuncs[taskId] = std::move(in);
        return *this;
    }

    TaskId      taskId;
    Tasks       &rTasks;
    FuncVec_t   &rFuncs;

}; // struct TaskRef

template <typename FUNC_T, typename ENUM_T>
struct PipelineRef
{
    using FuncVec_t = KeyedVec<TaskId, FUNC_T>;

    constexpr operator PipelineId() noexcept
    {
        return pipelineId;
    }

    PipelineRef& parent(PipelineId const parent)
    {
        rTasks.m_pipelineParents[pipelineId] = parent;
        return static_cast<PipelineRef&>(*this);
    }

    PipelineRef& parent_with_schedule(PipelineId const parent)
    {
        rTasks.m_pipelineParents[pipelineId] = parent;

        constexpr ENUM_T const scheduleStage = stage_schedule(ENUM_T{0});
        static_assert(scheduleStage != lgrn::id_null<ENUM_T>(), "Pipeline type has no schedule stage");

        TaskId const scheduler = rTasks.m_pipelineControl[parent].scheduler;
        LGRN_ASSERTM(scheduler != lgrn::id_null<TaskId>(), "Parent Pipeline has no scheduler task");

        rTasks.m_syncWith.push_back({
            .task     = scheduler,
            .pipeline = pipelineId,
            .stage    = StageId(scheduleStage)
        });

        return static_cast<PipelineRef&>(*this);
    }

    PipelineRef& loops(bool const loop)
    {
        rTasks.m_pipelineControl[pipelineId].isLoopScope = loop;
        return static_cast<PipelineRef&>(*this);
    }

    PipelineRef& wait_for_signal(ENUM_T stage)
    {
        rTasks.m_pipelineControl[pipelineId].waitStage = StageId(stage);
        return static_cast<PipelineRef&>(*this);
    }

    PipelineId  pipelineId;
    Tasks       &rTasks;
    FuncVec_t   &rFuncs;

}; // struct TaskRef

/**
 * @brief A convenient interface for setting up Tasks and required task data
 */
template<typename FUNC_T>
struct TaskBuilder
{
    using FuncVec_t = KeyedVec<TaskId, FUNC_T>;

    constexpr TaskBuilder(Tasks &rTasks, FuncVec_t &rFuncs) noexcept
     : rTasks{rTasks}
     , rFuncs{rFuncs}
    { }

    TaskRef<FUNC_T> task()
    {
        TaskId const taskId = rTasks.m_taskIds.create();

        std::size_t const capacity = rTasks.m_taskIds.capacity();

        return task(taskId);
    };

    [[nodiscard]] constexpr TaskRef<FUNC_T> task(TaskId const taskId) noexcept
    {
        return { taskId, rTasks, rFuncs };
    }

    template <typename ENUM_T>
    [[nodiscard]] constexpr PipelineRef<FUNC_T, ENUM_T> pipeline(PipelineDef<ENUM_T> pipelineDef) noexcept
    {
        return PipelineRef<FUNC_T, ENUM_T>{ pipelineDef.m_value, rTasks, rFuncs };
    }

    template<typename TGT_STRUCT_T>
    TGT_STRUCT_T create_pipelines(osp::ArrayView<PipelineId> const pipelinesOut)
    {
        static_assert(sizeof(TGT_STRUCT_T) % sizeof(PipelineDefBlank_t) == 0);
        constexpr std::size_t count = sizeof(TGT_STRUCT_T) / sizeof(PipelineDefBlank_t);

        LGRN_ASSERTMV(count == pipelinesOut.size() ,
                      "The number of members in TGT_STRUCT_T must match the number of output pipelines",
                      count, pipelinesOut.size());

        rTasks.m_pipelineIds.create(pipelinesOut.begin(), pipelinesOut.end());

        std::size_t const capacity = rTasks.m_pipelineIds.capacity();

        rTasks.m_pipelineInfo   .resize(capacity);
        rTasks.m_pipelineControl.resize(capacity);
        rTasks.m_pipelineParents.resize(capacity, lgrn::id_null<PipelineId>());

        // Set value members of TGT_STRUCT_T, asserted to contain only PipelineDef<...>
        // This is janky enough that rewriting the code below might cause it to ONLY SEGFAULT ON
        // RELEASE AND ISN'T CAUGHT BY ASAN WTF??? (on gcc 11)

        alignas(TGT_STRUCT_T) std::array<unsigned char, sizeof(TGT_STRUCT_T)> bytes;
        TGT_STRUCT_T *pOut = new(bytes.data()) TGT_STRUCT_T;

        for (std::size_t i = 0; i < count; ++i)
        {
            PipelineId const pl = pipelinesOut[i];
            unsigned char *pDefBytes = bytes.data() + sizeof(PipelineDefBlank_t)*i;

            *reinterpret_cast<PipelineId*>(pDefBytes + offsetof(PipelineDefBlank_t, m_value)) = pl;

            rTasks.m_pipelineInfo[pl].stageType = *reinterpret_cast<PipelineInfo::StageTypeId*>(pDefBytes + offsetof(PipelineDefBlank_t, m_type));
            rTasks.m_pipelineInfo[pl].name      = *reinterpret_cast<std::string_view*>(pDefBytes + offsetof(PipelineDefBlank_t, m_name));
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
        rTasks.m_pipelineIds.create(out.begin(), out.end());
        return out;
    }

    Tasks       &rTasks;
    FuncVec_t   &rFuncs;

}; // class TaskBuilderBase


} // namespace osp
