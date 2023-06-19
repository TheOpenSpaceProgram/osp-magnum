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

constexpr std::size_t gc_maxStages = 16;

using StageBits_t   = std::bitset<gc_maxStages>;

using TaskInt       = uint32_t;
using PipelineInt   = uint32_t;
using StageInt      = uint8_t;
using SemaphoreInt  = uint32_t;

enum class TaskId       : TaskInt       { };
enum class PipelineId   : PipelineInt   { };
enum class StageId      : StageInt      { };
enum class SemaphoreId  : SemaphoreInt  { };


struct Tasks
{
    lgrn::IdRegistryStl<TaskId>                     m_taskIds;
    lgrn::IdRegistryStl<PipelineId>                 m_pipelineIds;
    lgrn::IdRegistryStl<SemaphoreId>                m_semaIds;

    KeyedVec<SemaphoreId, unsigned int>             m_semaLimits;
};

struct TplTaskPipelineStage
{
    TaskId      task;
    PipelineId  pipeline;
    StageId     stage;
};

struct TplPipelineStage
{
    PipelineId  pipeline;
    StageId     stage;
};

struct TplTaskSemaphore
{
    TaskId      task;
    SemaphoreId semaphore;
};

struct TaskEdges
{
    std::vector<TplTaskPipelineStage>   m_runOn;
    std::vector<TplTaskPipelineStage>   m_syncWith;
    std::vector<TplTaskPipelineStage>   m_triggers;
    std::vector<TplTaskSemaphore>       m_semaphoreEdges;
};

enum class AnyStageId               : uint32_t { };

enum class RunTaskId                : uint32_t { };
enum class RunStageId               : uint32_t { };
enum class TriggerId                : uint32_t { };

enum class StageReqTaskId           : uint32_t { };
enum class ReverseStageReqTaskId    : uint32_t { };

enum class TaskReqStageId           : uint32_t { };
enum class ReverseTaskReqStageId    : uint32_t { };

struct StageRequiresTask
{
    AnyStageId  ownStage    { lgrn::id_null<AnyStageId>() };

    // Task needs to be complete for requirement to be satisfied
    // All requirements must be satisfied to proceed to the next stage
    TaskId      reqTask     { lgrn::id_null<TaskId>() };
    PipelineId  reqPipeline { lgrn::id_null<PipelineId>() };
    StageId     reqStage    { lgrn::id_null<StageId>() };
};

struct TaskRequiresStage
{
    TaskId      ownTask     { lgrn::id_null<TaskId>() };

    // Pipeline must be on a certain stage for requirement to be satisfied.
    // All requirements must be satisfied for the task to be unblocked
    PipelineId  reqPipeline { lgrn::id_null<PipelineId>() };
    StageId     reqStage    { lgrn::id_null<StageId>() };
};

struct TaskGraph
{
    // Each pipeline has multiple stages.
    // PipelineId <--> many AnyStageIds
    KeyedVec<PipelineId, AnyStageId>                pipelineToFirstAnystg;
    KeyedVec<AnyStageId, PipelineId>                anystgToPipeline;

    // Each stage has multiple tasks to run
    // AnyStageId --> TaskInStageId --> many TaskId
    KeyedVec<AnyStageId, RunTaskId>                 anystgToFirstRuntask;
    KeyedVec<RunTaskId, TaskId>                     runtaskToTask;

    // Each task runs on many stages
    // TaskId --> TaskRunStageId --> many AnyStageId
    KeyedVec<TaskId, RunStageId>                    taskToFirstRunstage;
    KeyedVec<RunStageId, AnyStageId>                runstageToAnystg;

    // Tasks trigger stages of pipelines
    // TaskId --> TriggerId --> many TplPipelineStage
    KeyedVec<TaskId, TriggerId>                     taskToFirstTrigger;
    KeyedVec<TriggerId, TplPipelineStage>           triggerToPlStage;

    // Each stage has multiple entrance requirements.
    // AnyStageId <--> many StageEnterReqId
    KeyedVec<AnyStageId, StageReqTaskId>            anystgToFirstStgreqtask;
    KeyedVec<StageReqTaskId, StageRequiresTask>     stgreqtaskData;
    // Tasks need to know which stages refer to them
    // TaskId --> ReverseStageReqId --> many AnyStageId
    KeyedVec<TaskId, ReverseStageReqTaskId>         taskToFirstRevStgreqtask;
    KeyedVec<ReverseStageReqTaskId, AnyStageId>     revStgreqtaskToStage;

    // Task requires pipelines to be on certain stages.
    // TaskId <--> TaskReqId
    KeyedVec<TaskId, TaskReqStageId>                taskToFirstTaskreqstg;
    KeyedVec<TaskReqStageId, TaskRequiresStage>     taskreqstgData;
    // Stages need to know which tasks require them
    // StageId --> ReverseTaskReqId --> many TaskId
    KeyedVec<AnyStageId, ReverseTaskReqStageId>     anystgToFirstRevTaskreqstg;
    KeyedVec<ReverseTaskReqStageId, TaskId>         revTaskreqstgToTask;


    // not yet used
    lgrn::IntArrayMultiMap<TaskInt, SemaphoreId>    taskAcquire;      /// Tasks acquire (n) Semaphores
    lgrn::IntArrayMultiMap<SemaphoreInt, TaskId>    semaAcquiredBy;   /// Semaphores are acquired by (n) Tasks
};


/**
 * @brief Bitset returned by tasks to determine which fulfill targets should be marked dirty
 */
using FulfillDirty_t = lgrn::BitView<std::array<uint64_t, 1>>;


TaskGraph make_exec_graph(Tasks const& tasks, ArrayView<TaskEdges const* const> data);

inline TaskGraph make_exec_graph(Tasks const& tasks, std::initializer_list<TaskEdges const* const> data)
{
    return make_exec_graph(tasks, arrayView(data));
}

template <typename KEY_T, typename VALUE_T, typename GETSIZE_T, typename CLAIM_T>
inline void fanout_partition(KeyedVec<KEY_T, VALUE_T>& rVec, GETSIZE_T&& get_size, CLAIM_T&& claim) noexcept
{
    using key_int_t     = lgrn::underlying_int_type_t<KEY_T>;
    using value_int_t   = lgrn::underlying_int_type_t<VALUE_T>;

    value_int_t currentId = 0;
    for (value_int_t i = 0; i < rVec.size(); ++i)
    {
        value_int_t const size = get_size(KEY_T(i));

        rVec[KEY_T(i)] = VALUE_T(currentId);

        if (size != 0)
        {
            value_int_t const nextId = currentId + size;

            for (value_int_t j = currentId; j < nextId; ++j)
            {
                claim(KEY_T(i), VALUE_T(j));
            }

            currentId = nextId;
        }
    }
}

template <typename KEY_T, typename VALUE_T>
inline auto fanout_size(KeyedVec<KEY_T, VALUE_T> const& vec, KEY_T key) -> lgrn::underlying_int_type_t<VALUE_T>
{
    using key_int_t     = lgrn::underlying_int_type_t<KEY_T>;
    using value_int_t   = lgrn::underlying_int_type_t<VALUE_T>;

    value_int_t const firstIdx  = value_int_t(vec[key]);
    value_int_t const lastIdx   = value_int_t(vec[KEY_T(key_int_t(key) + 1)]);

    return lastIdx - firstIdx;
}

template <typename VIEWTYPE_T, typename KEY_T, typename VALUE_T>
inline decltype(auto) fanout_view(KeyedVec<KEY_T, VALUE_T> const& vec, KeyedVec<VALUE_T, VIEWTYPE_T> const& access, KEY_T key)
{
    using key_int_t     = lgrn::underlying_int_type_t<KEY_T>;
    using value_int_t   = lgrn::underlying_int_type_t<VALUE_T>;

    value_int_t const firstIdx  = value_int_t(vec[key]);
    value_int_t const lastIdx   = value_int_t(vec[KEY_T(key_int_t(key) + 1)]);

    return arrayView(access.data(), access.size()).slice(firstIdx, lastIdx);
}


template <typename KEY_T, typename VALUE_T>
inline VALUE_T id_from_count(KeyedVec<KEY_T, VALUE_T> const& vec, KEY_T const key, lgrn::underlying_int_type_t<VALUE_T> const count)
{
    return VALUE_T( lgrn::underlying_int_type_t<VALUE_T>(vec[KEY_T(lgrn::underlying_int_type_t<KEY_T>(key) + 1)]) - count );
}

inline AnyStageId anystg_from(TaskGraph const& graph, PipelineId const pl, StageId stg) noexcept
{
    return AnyStageId(uint32_t(graph.pipelineToFirstAnystg[pl]) + uint32_t(stg));
}

inline StageId stage_from(TaskGraph const& graph, PipelineId const pl, AnyStageId const stg) noexcept
{
    return StageId(uint32_t(stg) - uint32_t(graph.pipelineToFirstAnystg[pl]));
}

inline StageId stage_from(TaskGraph const& graph, AnyStageId const stg) noexcept
{
    return stage_from(graph, graph.anystgToPipeline[stg], stg);
}

constexpr StageId stage_next(StageId const in, int stageCount) noexcept
{
    int const next = int(in) + 1;
    return StageId( (next == stageCount) ? 0 : next );
}

constexpr StageId stage_prev(StageId const in, int stageCount) noexcept
{
    return StageId( (int(in)==0) ? (stageCount-1) : (int(in)-1) );
}

template <typename ENUM_T>
struct PipelineDef
{
    operator PipelineId() const noexcept { return m_value; }
    operator std::size_t() const noexcept { return std::size_t(m_value); }

    PipelineId& operator=(PipelineId const assign) { m_value = assign; return m_value; }

    PipelineId m_value;
};

} // namespace osp
