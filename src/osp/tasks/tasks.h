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

#include <osp/core/array_view.h>
#include <osp/core/keyed_vector.h>
#include <osp/core/strong_id.h>

#include <longeron/id_management/registry_stl.hpp>
#include <longeron/containers/intarray_multimap.hpp>

#include <entt/core/family.hpp>

#include <cstdint>
#include <string_view>
#include <unordered_map>
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

using PipelineTypeId    = osp::StrongId<std::uint32_t, struct DummyForPipelineTypeId>;
using LoopBlockId       = osp::StrongId<std::uint32_t, struct DummyForLoopBlockId>;
using TaskId            = osp::StrongId<std::uint32_t, struct DummyForTaskId>;
using PipelineId        = osp::StrongId<std::uint32_t, struct DummyForPipelineId>;
using StageId           = osp::StrongId<std::uint8_t,  struct DummyForStageId>;
using SemaphoreId       = osp::StrongId<std::uint32_t, struct DummyForSemaphoreId>;

//-----------------------------------------------------------------------------

struct PipelineTypeInfo
{
    struct StageInfo
    {
        std::string name;

        /// Only 1 per pipeline
        bool isSchedule     {false};

        // Only 1 per pipeline
        //bool useSignal     {false};

        /// Cancel connected
        bool useCancel        {false};
    };

    std::string debugName;
    osp::KeyedVec<StageId, StageInfo> stages;
};

struct PipelineTypeIdReg
{
    static PipelineTypeIdReg& instance();

    [[nodiscard]] constexpr lgrn::IdRegistryStl<PipelineTypeId> const& ids() const { return m_ids; }
    [[nodiscard]] constexpr PipelineTypeInfo const& get(PipelineTypeId id) const { return m_pltypes[id]; }

    template<typename ENUM_T>
    constexpr PipelineTypeId get_or_register_pltype() { return get_or_register_pltype(std::type_index(typeid(ENUM_T))); }

    PipelineTypeId get_or_register_pltype(std::type_index typeidx)
    {
        auto const [it, success] = m_typeToPltype.try_emplace(typeidx);

        if (success)
        {
            // new type ID created
            it->second = m_ids.create();
            m_pltypes.resize(m_ids.size());
        }
        // else, existing ID was obtained

        return it->second;
    }

    // Eventually add functions to add pipeline types at runtime, but those don't exist yet.

    template<typename ENUM_T>
    void assign_pltype_info(PipelineTypeInfo info)
    {
        m_pltypes[get_or_register_pltype<ENUM_T>()] = std::move(info);
    }

    void assign_pltype_info(PipelineTypeId const id, PipelineTypeInfo info)
    {
        m_pltypes[id] = std::move(info);
    }

private:
    PipelineTypeIdReg() = default;

    lgrn::IdRegistryStl<PipelineTypeId>                 m_ids;
    osp::KeyedVec<PipelineTypeId, PipelineTypeInfo>     m_pltypes;
    std::unordered_map<std::type_index, PipelineTypeId> m_typeToPltype;
};

struct LoopBlock
{
    LoopBlockId parent;

    TaskId scheduleCondition;
};

struct Pipeline
{
    std::string_view    name;
    PipelineTypeId      type;

    /// Must never be null.
    LoopBlockId block;

    /// Read output value of this task as the condition to cancel this pipeline. Can be null
    TaskId scheduleCondition;
};

struct TaskSyncToPipeline
{
    TaskId      task;
    PipelineId  pipeline;
    StageId     stage;
};

struct Tasks
{
    lgrn::IdRegistryStl<LoopBlockId>    loopblkIds;
    lgrn::IdRegistryStl<TaskId>         taskIds;
    lgrn::IdRegistryStl<PipelineId>     pipelineIds;

    KeyedVec<LoopBlockId, LoopBlock>    loopblkInst;
    KeyedVec<PipelineId, Pipeline>      pipelineInst;

    std::vector<TaskSyncToPipeline>     syncs;
};


struct TplPipelineStage
{
    PipelineId  pipeline;
    StageId     stage;
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

    constexpr TplPipelineStage tpl(ENUM_T stage) const noexcept { return { m_value, StageId(std::size_t(stage)) }; }

    constexpr TplPipelineStage operator()(ENUM_T stage) const noexcept { return tpl(stage); }

    std::string_view            m_name;
    std::type_index             m_type { std::type_index(typeid(ENUM_T)) };
    PipelineId                  m_value { lgrn::id_null<PipelineId>() };
};

using PipelineDefBlank_t = PipelineDef<int>;

struct PipelineDefInfo
{
    std::string_view        name;
    PipelineTypeId          type;
};


} // namespace osp
