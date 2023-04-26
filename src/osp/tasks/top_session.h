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

#include "builder.h"
#include "execute.h"
#include "tasks.h"
#include "top_tasks.h"

#include <entt/core/any.hpp>

#include <cassert>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

#include <osp/unpack.h>

#define OSP_STRUCT_BIND_C(outerFunc, inner, count, ...) auto const [__VA_ARGS__] = outerFunc<count>(inner)
#define OSP_STRUCT_BIND_B(x) x
#define OSP_STRUCT_BIND_A(...) OSP_STRUCT_BIND_B(OSP_STRUCT_BIND_C(__VA_ARGS__))

#define OSP_SESSION_UNPACK_DATA(session, name) OSP_STRUCT_BIND_A(osp::unpack, (session).m_data, OSP_DATA_##name);

#define OSP_SESSION_ACQUIRE_DATA(session, topData, name) OSP_STRUCT_BIND_A((session).acquire_data, (topData), OSP_DATA_##name);

namespace osp
{

/**
 * @brief A convenient group of TopData, Tasks, and Tags that work together to
 *        support a certain feature.
 *
 * Sessions only store vectors of integer IDs, and don't does not handle
 * ownership on its own. Close using osp::top_close_session before destruction.
 */
struct Session
{
    template <std::size_t N>
    std::array<TopDataId, N> acquire_data(ArrayView<entt::any> topData)
    {
        std::array<TopDataId, N> out;
        top_reserve(topData, 0, std::begin(out), std::end(out));
        m_data.assign(std::begin(out), std::end(out));
        return out;
    }

    template<typename TGT_STRUCT_T>
    TGT_STRUCT_T create_targets(TaskBuilderData &rBuilder)
    {
        static_assert(sizeof(TGT_STRUCT_T) % sizeof(TargetId) == 0);
        constexpr std::size_t count = sizeof(TGT_STRUCT_T) / sizeof(TargetId);

        std::type_info const& info = typeid(TGT_STRUCT_T);
        m_targetStructHash = info.hash_code();
        m_targetStructName = info.name();

        m_targets.resize(count);

        rBuilder.m_targetIds.create(m_targets.begin(), m_targets.end());

        return reinterpret_cast<TGT_STRUCT_T&>(*m_targets.data());
    }

    template<typename TGT_STRUCT_T>
    TGT_STRUCT_T get_targets() const
    {
        static_assert(sizeof(TGT_STRUCT_T) % sizeof(TargetId) == 0);
        constexpr std::size_t count = sizeof(TGT_STRUCT_T) / sizeof(TargetId);

        std::type_info const& info = typeid(TGT_STRUCT_T);
        LGRN_ASSERTMV(m_targetStructHash == info.hash_code(),
                      "get_targets must use the same struct given to create_targets",
                      m_targetStructHash, m_targetStructName,
                      info.hash_code(), info.name());
    }

    TaskId& task()
    {
        return m_tasks.emplace_back(lgrn::id_null<TaskId>());
    }

    std::vector<TopDataId>  m_data;
    std::vector<TargetId>   m_targets;
    std::vector<TaskId>     m_tasks;

    TargetId m_tgCleanupEvt{lgrn::id_null<TargetId>()};

    std::size_t m_targetStructHash{0};
    std::string m_targetStructName;
};

template<typename TGT_STRUCT_T>
std::vector<TargetId> to_target_vec(TGT_STRUCT_T const& targets)
{
    static_assert(sizeof(TGT_STRUCT_T) % sizeof(TargetId) == 0);
    constexpr std::size_t count = sizeof(TGT_STRUCT_T) / sizeof(TargetId);

    TargetId const* pData = reinterpret_cast<TargetId const*>(std::addressof(targets));

    return {pData, pData + count};
}

using Sessions_t = std::vector<Session>;

/**
 * @brief Close sessions, delete all their associated TopData, Tasks, and Targets.
 */
void top_close_session(Tasks& rTasks, TopTaskDataVec_t& rTaskData, ArrayView<entt::any> topData, ExecutionContext& rExec, ArrayView<Session> sessions);


} // namespace osp
