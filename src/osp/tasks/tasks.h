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

struct TaskGraphStage
{
    std::vector<TaskId>                 m_runTasks;
    std::vector<TaskId>                 m_triggeredBy;
    std::vector<TaskId>                 m_syncedBy;
    std::vector<TplTaskPipelineStage>   m_enterReq;
};

struct TaskGraphPipeline
{
    KeyedVec<StageId, TaskGraphStage>   m_stages;
};

struct TaskGraph
{
    KeyedVec<PipelineId, TaskGraphPipeline>             m_pipelines;

    lgrn::IntArrayMultiMap<TaskInt, TplPipelineStage>   m_taskRunOn;
    lgrn::IntArrayMultiMap<TaskInt, TplPipelineStage>   m_taskTriggers;
    lgrn::IntArrayMultiMap<TaskInt, TplPipelineStage>   m_taskSync;

    lgrn::IntArrayMultiMap<TaskInt, SemaphoreId>        m_taskAcquire;      /// Tasks acquire (n) Semaphores
    lgrn::IntArrayMultiMap<SemaphoreInt, TaskId>        m_semaAcquiredBy;   /// Semaphores are acquired by (n) Tasks
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

constexpr StageId stage_next(StageId const in, int stageCount) noexcept
{
    int const next = int(in) + 1;
    return StageId( (next == stageCount) ? 0 : next );
}


} // namespace osp
