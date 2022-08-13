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

#include <longeron/containers/bit_view.hpp>

#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/ArrayViewStl.h>

#include <cassert>
#include <cstdint>
#include <vector>

namespace osp
{

using Corrade::Containers::ArrayView;

inline void set_tags(
        Corrade::Containers::ArrayView<uint32_t const>  tagsIn,
        ArrayView<bit_int_t>                            taggedInts,
        std::size_t const                               taggedSize,
        std::size_t const                               taggedId) noexcept
{
    std::size_t const offset = taggedId * taggedSize;
    auto taggedBits = lgrn::BitView(taggedInts.slice(offset, offset + taggedSize));

    for (uint32_t const tag : tagsIn)
    {
        taggedBits.set(tag);
    }
}

/**
 * @brief A convenient interface for setting up TaskTags and required task data
 */
template <typename DATA_T>
class TaskBuilder
{
    using Tags_t = Corrade::Containers::ArrayView<TagId const>;

public:

    struct TaskRef
    {
        TaskRef& assign(Tags_t const tags) noexcept
        {
            set_tags(Corrade::Containers::arrayCast<uint32_t const>(tags),
                     m_rTasks.m_taskTags,
                     m_rTags.tag_ints_per_task(),
                     std::size_t(m_taskId));
            return *this;
        }

        TaskRef& assign(std::initializer_list<TagId> tags) noexcept
        {
            return assign(Corrade::Containers::arrayView(tags));
        }

        template <typename ... ARGS_T>
        TaskRef& data(ARGS_T&& ... args) noexcept
        {
            task_data(m_rData, m_taskId, std::forward<ARGS_T>(args) ...);
            return *this;
        }

        TaskId const    m_taskId;
        Tags            &m_rTags;
        Tasks           &m_rTasks;
        DATA_T          &m_rData;

    }; // struct TaskRefSpec

    struct TagRef
    {

        TagRef& depend_on(Tags_t tags) noexcept
        {
            std::size_t const offset = std::size_t(m_tagId) * m_rTags.m_tagDependsPerTag;
            auto depends = Corrade::Containers::arrayView(m_rTags.m_tagDepends)
                            .slice(offset, offset + m_rTags.m_tagDependsPerTag);
            auto const depBegin = std::begin(depends);
            auto const depEnd   = std::end(depends);

            for (TagId const tag : tags)
            {
                // Find first empty spot or already-added dependency
                auto dependIt = std::find_if(
                        depBegin, depEnd, [tag] (TagId const lhs)
                {
                    return (lhs == lgrn::id_null<TagId>()) || (lhs == tag);
                });

                // No space left for any dependencies
                assert(dependIt != depEnd);

                *dependIt = tag;
            }

            return *this;
        }

        TagRef& depend_on(std::initializer_list<TagId> tags) noexcept
        {
            return depend_on(Corrade::Containers::arrayView(tags));
        }

        TagRef& limit(unsigned int lim)
        {
            m_rTags.m_tagLimits[std::size_t(m_tagId)] = lim;
            return *this;
        }

        TagRef& set_external(bool value)
        {
            auto bitView = lgrn::bit_view(m_rTags.m_tagExtern);
            if (value)
            {
                bitView.set(std::size_t(m_tagId));
            }
            else
            {
                bitView.reset(std::size_t(m_tagId));
            }
            return *this;
        }

        TagRef& enqueues(TagId enqueueTag)
        {
            m_rTags.m_tagEnqueues[std::size_t(m_tagId)] = enqueueTag;
            return *this;
        }

        TagId const m_tagId;
        Tags        &m_rTags;

    }; // struct TagRef

    constexpr TaskBuilder(Tags& rTags, Tasks& rTasks, DATA_T& rData)
     : m_rTags{rTags}
     , m_rTasks{rTasks}
     , m_rData{rData}
    { }

    template <std::size_t N>
    std::array<TagId, N> create_tags()
    {
        std::array<TagId, N> out;

        [[maybe_unused]] auto const it
                = m_rTags.m_tags.create(std::begin(out), std::end(out));

        assert(it == std::end(out)); // Auto-resizing tags not (yet?) supported

        return out;
    }

    TagRef tag(TagId const tagId)
    {
        return {
            .m_tagId = tagId,
            .m_rTags = m_rTags
        };
    }

    TaskRef task()
    {
        TaskId const taskId = m_rTasks.m_tasks.create();

        std::size_t const capacity = m_rTasks.m_tasks.capacity();

        m_rTasks.m_taskTags.resize(capacity * m_rTags.tag_ints_per_task());

        return task(taskId);
    };

    constexpr TaskRef task(TaskId const taskId) noexcept
    {
        return {
            .m_taskId       = taskId,
            .m_rTags        = m_rTags,
            .m_rTasks       = m_rTasks,
            .m_rData        = m_rData
        };
    }

private:
    Tags    &m_rTags;
    Tasks   &m_rTasks;
    DATA_T  &m_rData;

}; // class TaskBuilder

} // namespace osp
