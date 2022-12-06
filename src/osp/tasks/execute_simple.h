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

#include <Corrade/Containers/ArrayViewStl.h>
#include <Corrade/Containers/StridedArrayView.h>

#include <cassert>
#include <vector>

namespace osp
{

using BitSpan_t = Corrade::Containers::ArrayView<bit_int_t>;
using BitSpanConst_t = Corrade::Containers::ArrayView<bit_int_t const>;

/**
 * @brief Convert a range of ints or enums to bit positions
 *
 * {0, 1, 7, 4} -> 0x1010011
 *
 * @param range     [in] Range of ints or enum classes
 * @param bitsOut   [out] Span of ints large enough to fit all bits in range
 *
 * @return bitsOut
 */
template <typename RANGE_T>
BitSpan_t to_bitspan(RANGE_T const& range, BitSpan_t bitsOut) noexcept
{
    auto outBits = lgrn::bit_view(bitsOut);
    outBits.reset();
    for (auto const pos : range)
    {
        outBits.set(std::size_t(pos));
    }
    return bitsOut;
}

template <typename T>
BitSpan_t to_bitspan(std::initializer_list<T> tags, BitSpan_t out) noexcept
{
    return to_bitspan(Corrade::Containers::arrayView(tags), out);
}

template <typename FUNC_T>
constexpr void task_for_each(Tags const& tags, Tasks const& tasks, FUNC_T&& func)
{
    auto const taskTags2d = task_tags_2d(tags, tasks);

    for (uint32_t const currTask : tasks.m_tasks.bitview().zeros())
    {
        func(currTask, taskTags2d[currTask].asContiguous());
    }
}

bool any_bits_match(BitSpanConst_t lhs, BitSpanConst_t rhs);

/**
 * @brief Enqueue all tasks that contain any specified tag
 *
 * @param tags      [in] Tags
 * @param tasks     [in] Tasks
 * @param rExec     [ref] ExecutionContext to record queued tasks into
 * @param query     [in] Bit positions of tags used to select tasks
 */
void task_enqueue(Tags const& tags, Tasks const& tasks, ExecutionContext &rExec, BitSpanConst_t query);

void task_extern_set(ExecutionContext &rExec, TagId tag, bool value);

/**
 * @brief List all available tasks that are currently allowed to run
 *
 * @param tags      [in] Tags
 * @param tasks     [in] Tasks
 * @param rExec     [in] ExecutionContext indicating tasks available
 * @param tasksOut  [out] Available tasks as bit positions
 */
void task_list_available(Tags const& tags, Tasks const& tasks, ExecutionContext const& exec, BitSpan_t tasksOut);

/**
 * @brief Mark a task as running in an ExecutionContext
 *
 * @param tags      [in] Tags
 * @param tasks     [in] Tasks
 * @param rExec     [ref] ExecutionContext to record running tasks
 * @param task      [in] Id of task to start running
 */
void task_start(Tags const& tags, Tasks const& tasks, ExecutionContext &rExec, TaskId task);

/**
 * @brief Mark a task as finished in an ExecutionContext
 *
 * @param tags          [in] Tags
 * @param tasks         [in] Tasks
 * @param rExec         [ref] ExecutionContext to record running tasks
 * @param task          [in] Id of task to finish
 */
void task_finish(Tags const& tags, Tasks const& tasks, ExecutionContext &rExec, TaskId task);

} // namespace osp
