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

inline void set_tags(
        Corrade::Containers::ArrayView<uint32_t const>  tags,
        Corrade::Containers::ArrayView<uint64_t>        taskTagInts,
        std::size_t const                               intsPerTask,
        TaskTags::Task const                            task) noexcept
{
    std::size_t const offset = std::size_t(task) * intsPerTask;
    auto tagBits = lgrn::BitView(taskTagInts.slice(offset, offset + intsPerTask));

    for (uint32_t const tag : tags)
    {
        tagBits.set(tag);
    }
}

template <typename ... ARGS_T>
class TaskBuilder
{
    using Functions_t = TaskFunctions<ARGS_T...>;
    using Func_t = typename Functions_t::Func_t;
public:

    struct TaskRefSpec
    {
        using FulfillTags_t = Corrade::Containers::ArrayView<TaskTags::Fulfill const>;
        using LimitedTags_t = Corrade::Containers::ArrayView<TaskTags::Limited const>;

        TaskRefSpec& fulfills(FulfillTags_t const tags) noexcept
        {
            set_tags(Corrade::Containers::arrayCast<uint32_t const>(tags),
                     m_rTags.m_taskFulfill,
                     m_rTags.fulfill_int_size(),
                     m_id);
            return *this;
        }

        TaskRefSpec& fulfills(std::initializer_list<TaskTags::Fulfill> tags) noexcept
        {
            return fulfills(Corrade::Containers::arrayView(tags));
        }

        TaskRefSpec& depends(FulfillTags_t const tags) noexcept
        {
            set_tags(Corrade::Containers::arrayCast<uint32_t const>(tags),
                     m_rTags.m_taskDepends,
                     m_rTags.fulfill_int_size(),
                     m_id);
            return *this;
        }

        TaskRefSpec& depends(std::initializer_list<TaskTags::Fulfill> tags) noexcept
        {
            return depends(Corrade::Containers::arrayView(tags));
        }

        TaskRefSpec& limited(FulfillTags_t const tags) noexcept
        {
            set_tags(Corrade::Containers::arrayCast<uint32_t const>(tags),
                     m_rTags.m_taskLimited,
                     m_rTags.limited_int_size(),
                     m_id);
            return *this;
        }

        TaskRefSpec& limited(std::initializer_list<TaskTags::Fulfill> tags) noexcept
        {
            return limited(Corrade::Containers::arrayView(tags));
        }

        TaskRefSpec& function(Func_t&& func) noexcept
        {
            m_rFunctions.m_taskFunctions[std::size_t(m_id)] = std::move(func);
            return *this;
        }

        TaskTags::Task const m_id;
        TaskTags &m_rTags;
        Functions_t &m_rFunctions;

    }; // struct TaskRefSpec

    constexpr TaskBuilder(TaskTags& rTags, Functions_t& rFunctions)
     : m_rTags{rTags}
     , m_rFunctions{rFunctions}
    { }

    template <std::size_t N>
    std::array<TaskTags::Fulfill, N> fulfill_tags()
    {
        std::array<TaskTags::Fulfill, N> out;

        [[maybe_unused]] auto const it
                = m_rTags.m_fulfillTags.create(std::begin(out), std::end(out));

        assert(it == std::end(out)); // Auto-resizing tags not (yet?) supported

        return out;
    }

    template <std::size_t N>
    std::array<TaskTags::Limited, N> limit_tags()
    {
        std::array<TaskTags::Limited, N> out;

        [[maybe_unused]] auto const it
                = m_rTags.m_limitedTags.create(std::begin(out), std::end(out));

        assert(it == std::end(out)); // Auto-resizing tags not (yet?) supported

        return out;
    }

    TaskRefSpec task()
    {
        TaskTags::Task const task = m_rTags.m_tasks.create();

        std::size_t const capacity = m_rTags.m_tasks.capacity();
        std::size_t const fulfillCapacity = capacity * m_rTags.fulfill_int_size();
        std::size_t const limitedCapacity = capacity * m_rTags.limited_int_size();

        m_rFunctions.m_taskFunctions.resize(capacity);

        m_rTags.m_taskFulfill.resize(fulfillCapacity);
        m_rTags.m_taskDepends.resize(fulfillCapacity);
        m_rTags.m_taskLimited.resize(limitedCapacity);

        return {
            .m_id           = task,
            .m_rTags        = m_rTags,
            .m_rFunctions   = m_rFunctions
        };

    };

private:
    TaskTags &m_rTags;
    Functions_t &m_rFunctions;

}; // class TaskBuilder

} // namespace osp
