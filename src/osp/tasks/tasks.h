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

#include "../core/array_view.h"
#include "../core/keyed_vector.h"

#include <longeron/id_management/registry_stl.hpp>
#include <longeron/containers/intarray_multimap.hpp>

#include <entt/core/family.hpp>

#include <ostream>
#include <string_view>
#include <variant>
#include <vector>
#include <span>

#include <cstdint>

#define OSP_DECLARE_STAGE_NAMES(type, ...)                                                    \
    constexpr std::span<std::string_view const> stage_names([[maybe_unused]] type _) noexcept \
    {                                                                                         \
        return std::initializer_list<std::string_view>{__VA_ARGS__};                          \
    }

#define OSP_DECLARE_STAGE_SCHEDULE(type, schedule_enum)              \
    constexpr type stage_schedule([[maybe_unused]] type _) noexcept  \
    {                                                                \
        return schedule_enum;                                        \
    }

#define OSP_DECLARE_STAGE_NO_SCHEDULE(type)                          \
    constexpr type stage_schedule([[maybe_unused]] type _) noexcept  \
    {                                                                \
        return lgrn::id_null<type>();                                \
    }

namespace osp
{

constexpr std::size_t gc_maxStages = 16;

using StageBits_t   = std::bitset<gc_maxStages>;

using TaskInt       = std::uint32_t;
using PipelineInt   = std::uint32_t;
using StageInt      = std::uint8_t;
using SemaphoreInt  = std::uint32_t;

enum class TaskId       : TaskInt       { };
enum class PipelineId   : PipelineInt   { };
enum class StageId      : StageInt      { };
enum class SemaphoreId  : SemaphoreInt  { };

//-----------------------------------------------------------------------------

struct PipelineInfo
{
    using stage_type_family_t = entt::family<struct StageTypeDummy>;
    using stage_type_t        = stage_type_family_t::value_type;

    static inline KeyedVec<stage_type_t, std::span<std::string_view const>> sm_stageNames;

    template <typename STAGE_ENUM_T>
    static constexpr void register_stage_enum()
    {
        PipelineInfo::stage_type_t const type = PipelineInfo::stage_type_family_t::value<STAGE_ENUM_T>;
        PipelineInfo::sm_stageNames[type] = stage_names(STAGE_ENUM_T{});
    }

    std::string_view    name;
    std::string_view    category;
    stage_type_t        stageType { lgrn::id_null<stage_type_t>() };
};

struct PipelineControl
{
    TaskId      scheduler   { lgrn::id_null<TaskId>() };
    StageId     waitStage   { lgrn::id_null<StageId>() };
    bool        isLoopScope { false };
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

//-----------------------------------------------------------------------------

struct Tasks
{
    lgrn::IdRegistryStl<TaskId>                     m_taskIds;
    lgrn::IdRegistryStl<PipelineId>                 m_pipelineIds;
    lgrn::IdRegistryStl<SemaphoreId>                m_semaIds;

    KeyedVec<SemaphoreId, unsigned int>             m_semaLimits;

    KeyedVec<PipelineId, PipelineInfo>              m_pipelineInfo;
    KeyedVec<PipelineId, PipelineId>                m_pipelineParents;
    KeyedVec<PipelineId, PipelineControl>           m_pipelineControl;

    KeyedVec<TaskId, TplPipelineStage>              m_taskRunOn;
};

struct TaskEdges
{
    std::vector<TplTaskPipelineStage>   m_syncWith;

    //std::vector<TplTaskSemaphore>       m_semaphoreEdges;
};

using PipelineTreePos_t = uint32_t;

enum class AnyStageId               : std::uint32_t { };

enum class RunTaskId                : std::uint32_t { };
enum class RunStageId               : std::uint32_t { };

enum class StageReqTaskId           : std::uint32_t { };
enum class ReverseStageReqTaskId    : std::uint32_t { };

enum class TaskReqStageId           : std::uint32_t { };
enum class ReverseTaskReqStageId    : std::uint32_t { };

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
    KeyedVec<AnyStageId, ReverseTaskReqStageId>     anystgToFirstRevTaskreqstg;
    KeyedVec<ReverseTaskReqStageId, TaskId>         revTaskreqstgToTask;

    // N-ary tree structure represented as an array of descendant counts. Each node's subtree of
    // descendants is positioned directly after it within the array.
    // Example for tree structure "A(  B(C(D)), E(F(G(H,I)))  )"
    // * Descendant Count array: [A:8, B:2, C:1, D:0, E:4, F:3, G:2, H:0, I:0]
    KeyedVec<PipelineTreePos_t, uint32_t>           pltreeDescendantCounts;
    KeyedVec<PipelineTreePos_t, PipelineId>         pltreeToPipeline;
    KeyedVec<PipelineId, PipelineTreePos_t>         pipelineToPltree;
    KeyedVec<PipelineId, PipelineTreePos_t>         pipelineToLoopScope;

    // not yet used
    //lgrn::IntArrayMultiMap<TaskInt, SemaphoreId>    taskAcquire;      /// Tasks acquire (n) Semaphores
    //lgrn::IntArrayMultiMap<SemaphoreInt, TaskId>    semaAcquiredBy;   /// Semaphores are acquired by (n) Tasks

}; // struct TaskGraph


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
    return AnyStageId(std::uint32_t(graph.pipelineToFirstAnystg[pl]) + std::uint32_t(stg));
}

inline StageId stage_from(TaskGraph const& graph, PipelineId const pl, AnyStageId const stg) noexcept
{
    return StageId(std::uint32_t(stg) - std::uint32_t(graph.pipelineToFirstAnystg[pl]));
}

inline StageId stage_from(TaskGraph const& graph, AnyStageId const stg) noexcept
{
    return stage_from(graph, graph.anystgToPipeline[stg], stg);
}

template <typename ENUM_T>
struct PipelineDef
{
    constexpr PipelineDef() = default;
    constexpr PipelineDef(std::string_view name)
     : m_name{name}
    { }

    operator PipelineId() const noexcept { return m_value; }
    operator std::size_t() const noexcept { return std::size_t(m_value); }

    constexpr PipelineId& operator=(PipelineId const assign) noexcept { m_value = assign; return m_value; }

    constexpr TplPipelineStage tpl(ENUM_T stage) const noexcept { return { m_value, StageId(stage) }; }

    constexpr TplPipelineStage operator()(ENUM_T stage) const noexcept { return { m_value, StageId(stage) }; }

    std::string_view m_name;

    PipelineInfo::stage_type_t m_type { PipelineInfo::stage_type_family_t::value<ENUM_T> };

    PipelineId m_value { lgrn::id_null<PipelineId>() };
};

using PipelineDefBlank_t = PipelineDef<int>;

} // namespace osp
