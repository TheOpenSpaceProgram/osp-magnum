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

#include "top_tasks.h"
#include "top_utils.h"
#include "tasks.h"

#include <entt/core/any.hpp>

#include <cassert>
#include <type_traits>
#include <utility>
#include <vector>

#include <osp/unpack.h>

#define _OSP_AUTO_STRUCTURE_B(outerFunc, inner, count, ...) auto const [__VA_ARGS__] = outerFunc<count>(inner)
#define _OSP_AUTO_STRUCTURE_A(outerFunc, inner, name) _OSP_AUTO_STRUCTURE_B(outerFunc, inner, name)

#define OSP_SESSION_UNPACK_TAGS(session, name)  _OSP_AUTO_STRUCTURE_A(osp::unpack, (session).m_tagIds, OSP_TAGS_##name);
#define OSP_SESSION_UNPACK_DATA(session, name)  _OSP_AUTO_STRUCTURE_A(osp::unpack, (session).m_dataIds, OSP_DATA_##name);

#define OSP_SESSION_ACQUIRE_TAGS(session, tags, name) _OSP_AUTO_STRUCTURE_A((session).acquire_tags, (tags), OSP_TAGS_##name);
#define OSP_SESSION_ACQUIRE_DATA(session, topData, name) _OSP_AUTO_STRUCTURE_A((session).acquire_data, (topData), OSP_DATA_##name);

namespace osp
{

struct Session
{
    template <std::size_t N>
    std::array<TopDataId, N> acquire_data(ArrayView<entt::any> topData)
    {
        std::array<TopDataId, N> out;
        top_reserve(topData, 0, std::begin(out), std::end(out));
        m_dataIds.assign(std::begin(out), std::end(out));
        return out;
    }

    template <std::size_t N>
    std::array<TagId, N> acquire_tags(Tags &rTags)
    {
        std::array<TagId, N> out;
        rTags.m_tags.create(std::begin(out), std::end(out));
        m_tagIds.assign(std::begin(out), std::end(out));
        return out;
    }

    TaskId& task()
    {
        return m_taskIds.emplace_back(lgrn::id_null<TaskId>());
    }

    std::vector<TopDataId>  m_dataIds;
    std::vector<TagId>      m_tagIds;
    std::vector<TaskId>     m_taskIds;

    TagId m_tgCleanupEvt{lgrn::id_null<TagId>()};
};

void top_close_session(Tags& rTags, Tasks& rTasks, TopTaskDataVec_t& rTaskData, ArrayView<entt::any> topData, ExecutionContext& rExec, ArrayView<Session> sessions);


} // namespace osp
