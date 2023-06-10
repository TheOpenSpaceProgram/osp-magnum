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

//struct TaskGraphStage
//{
//    std::vector<TaskId>                 m_runTasks;
//    std::vector<TaskId>                 m_triggeredBy;
//    std::vector<TaskId>                 m_syncedBy;
//    std::vector<TplTaskPipelineStage>   m_enterReq;
//};

//struct TaskGraphPipeline
//{
//    KeyedVec<StageId, TaskGraphStage>   m_stages;
//};

enum class AnyStageId               : uint32_t { };
enum class RunTaskId                : uint32_t { };

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
    KeyedVec<AnyStageId, ReverseTaskReqStageId>     stageToFirstRevTaskreqstg;
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

template <typename ENUM_T>
struct PipelineDef
{
    operator PipelineId() const noexcept { return m_value; }
    operator std::size_t() const noexcept { return std::size_t(m_value); }

    PipelineId& operator=(PipelineId const assign) { m_value = assign; return m_value; }

    PipelineId m_value;
};

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

} // namespace osp
