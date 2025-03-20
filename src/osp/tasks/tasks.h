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
/**
 * @file
 * @brief Data structures for representing a complex multi-threaded application as a set of Tasks,
 *        organized with 'Pipelines' to determine when Tasks must run.
 *
 * Refer to docs/tasks.md
 *
 * This file only represents the graph structure of tasks. It does NOT contain...
 * * calls to run or execute Tasks
 * * data for Tasks (function signature, task name, etc...)
 */
#pragma once

#include "../core/array_view.h"
#include "../core/keyed_vector.h"

#include <longeron/id_management/registry_stl.hpp>
#include <longeron/containers/intarray_multimap.hpp>

#include <entt/core/family.hpp>

#include <cstdint>
#include <string_view>
#include <vector>

#define OSP_DECLARE_STAGE_NAMES(type, ...)                                                                  \
    inline osp::ArrayView<std::string_view const> stage_names([[maybe_unused]] type _) noexcept             \
    {                                                                                                       \
        static auto const arr = std::initializer_list<std::string_view>{__VA_ARGS__};                       \
        return osp::arrayView(arr);                                                                         \
    }

#define OSP_DECLARE_STAGE_SCHEDULE(type, schedule_enum)                     \
    constexpr inline type stage_schedule([[maybe_unused]] type _) noexcept  \
    {                                                                       \
        return schedule_enum;                                               \
    }

#define OSP_DECLARE_STAGE_NO_SCHEDULE(type)                                 \
    constexpr inline type stage_schedule([[maybe_unused]] type _) noexcept  \
    {                                                                       \
        return lgrn::id_null<type>();                                       \
    }

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

//-----------------------------------------------------------------------------

struct PipelineInfo
{
    using StageTypeFamily_t   = entt::family<struct StageTypeDummy>;
    using StageTypeId         = StageTypeFamily_t::value_type;

    static inline KeyedVec<StageTypeId, ArrayView<std::string_view const>> sm_stageNames;

    template <typename STAGE_ENUM_T>
    static inline constexpr void register_stage_enum()
    {
        PipelineInfo::StageTypeId const type = PipelineInfo::StageTypeFamily_t::value<STAGE_ENUM_T>;
        PipelineInfo::sm_stageNames[type] = stage_names(STAGE_ENUM_T{});
    }

    std::string_view    name;
    std::string_view    category;
    StageTypeId         stageType { lgrn::id_null<StageTypeId>() };
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

    std::vector<TplTaskPipelineStage>   m_syncWith;
};



/**
 * allows some form of reflection
 *
 * @code
 *
 * struct MyPipelines {
 *      PipelineDef<EStgOptn> foo {"foo"};
 *      PipelineDef<EStgIntr> bar {"bar"};
 * };
 * // ...
 *
 * MyPipelines pl{};
 *
 * @endcode
 */
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

    std::string_view            m_name;
    PipelineInfo::StageTypeId   m_type { PipelineInfo::StageTypeFamily_t::value<ENUM_T> };
    PipelineId                  m_value { lgrn::id_null<PipelineId>() };
};

using PipelineDefBlank_t = PipelineDef<int>;

struct PipelineDefInfo
{
    std::string_view            name;
    PipelineInfo::StageTypeId   type;
};


} // namespace osp
