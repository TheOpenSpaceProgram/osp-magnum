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

#include <longeron/id_management/registry_stl.hpp>

#include <cstdint>
#include <functional>
#include <vector>

namespace osp
{


struct TaskTags
{
    enum class Task     : uint32_t {};
    enum class Tag      : uint32_t {};

    lgrn::IdRegistryStl<Task>       m_tasks;
    lgrn::IdRegistryStl<Tag, true>  m_tags;

    std::vector<unsigned int>       m_tagLimits;
    std::vector<Tag>                m_tagDepends;
    std::size_t                     m_tagDependsPerTag{8};

    std::vector<uint64_t>           m_taskTags;

    std::size_t tag_ints_per_task() const noexcept { return m_tags.vec().capacity(); }
};

struct WorkerContext
{
    struct LimitSlot
    {
        TaskTags::Tag m_tag;
        int m_slot;
    };

    std::vector<LimitSlot> m_limitSlots;
};

template <typename ... ARGS_T>
struct TaskFunctions
{
    using Func_t = std::function<void(WorkerContext, ARGS_T ...)>;

    std::vector<Func_t> m_taskFunctions;
};


struct ExecutionContext
{
    //std::vector<unsigned int>
};

} // namespace osp
