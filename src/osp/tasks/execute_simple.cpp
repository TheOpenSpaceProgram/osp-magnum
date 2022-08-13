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

#include "execute_simple.h"

#include <algorithm>
#include <cstdint>

using Corrade::Containers::ArrayView;
using Corrade::Containers::StridedArrayView2D;
using Corrade::Containers::arrayView;

namespace osp
{

bool any_bits_match(BitSpanConst_t const lhs, BitSpanConst_t const rhs)
{
    return ! std::equal(
            std::cbegin(lhs), std::cend(lhs),
            std::cbegin(rhs), std::cend(rhs),
            [] (uint64_t const lhsInt, uint64_t const rhsInt)
    {
        return (lhsInt & rhsInt) == 0;
    });
}

void task_enqueue(Tags const& tags, Tasks const& tasks, ExecutionContext &rExec, BitSpanConst_t const query)
{
    assert(query.size() == tags.tag_ints_per_task());

    task_for_each(tags, tasks, [&rExec, &query]
            (uint32_t const currTask, ArrayView<uint64_t const> const currTags)
    {
        unsigned int &rQueuedCount = rExec.m_taskQueuedCounts[currTask];

        if ( (rQueuedCount != 0) || ( ! any_bits_match(query, currTags) ) )
        {
            return; // Ignore already-queued tasks, or if tags do not match query
        }

        rQueuedCount = 1; // All good, now queue the task

        // Add the task's tags to the incomplete counts
        auto const view = lgrn::bit_view(currTags);
        for (uint32_t const tag : view.ones())
        {
            rExec.m_tagIncompleteCounts[tag] ++;
        }
    });
}

void task_extern_set(ExecutionContext &rExec, TagId const tag, bool value)
{
    auto bitView = lgrn::bit_view(rExec.m_tagExternTriggers);
    if (value)
    {
        bitView.set(std::size_t(tag));
    }
    else
    {
        bitView.reset(std::size_t(tag));
    }
}


static bool tags_present(BitSpanConst_t const mask, BitSpanConst_t const taskTags) noexcept
{
    auto taskTagIntIt = std::begin(taskTags);
    for (uint64_t const maskInt : mask)
    {
        uint64_t const taskTagInt = *taskTagIntIt;

        // No match if a 1 bit in taskTagInt corresponds with a 0 in maskInt
        if ((maskInt & taskTagInt) != taskTagInt)
        {
            return false;
        }
        std::advance(taskTagIntIt, 1);
    }
    return true;
}

void task_list_available(Tags const& tags, Tasks const& tasks, ExecutionContext const& exec, BitSpan_t tasksOut)
{
    assert(tasksOut.size() == tasks.m_tasks.vec().size());

    // Bitmask makes it easy to compare the tags of a task
    // 1 = allowed (default), 0 = not allowed
    // All tags of a task must be allowed for the entire task to run.
    // aka: All ones in a task's bits must corrolate to a one in the mask
    std::size_t const maskSize = tags.m_tags.vec().size();
    std::vector<uint64_t> mask(maskSize, ~uint64_t(0));
    auto maskBits = lgrn::bit_view(mask);

    auto tagDepends2d = tag_depends_2d(tags);

    // Check dependencies of each tag
    // Set them 0 (disallow) in the mask if the tag has incomplete dependencies
    for (uint32_t const currTag : tags.m_tags.bitview().zeros())
    {
        bool satisfied = true;
        auto const currTagDepends = tagDepends2d[currTag].asContiguous();

        for (TagId const dependTag : currTagDepends)
        {
            if (dependTag == lgrn::id_null<TagId>())
            {
                break;
            }

            if (exec.m_tagIncompleteCounts[std::size_t(dependTag)] != 0)
            {
                satisfied = false;
                break;
            }
        }

        if ( ! satisfied)
        {
            maskBits.reset(currTag);
        }
    }

    // Check external dependencies
    std::size_t const externSize = std::min({tags.m_tagExtern.size(), exec.m_tagExternTriggers.size(), maskSize});
    for (std::size_t i = 0; i < externSize; ++i)
    {
        mask[i] &= ~(tags.m_tagExtern[i] & exec.m_tagExternTriggers[i]);
    }

    // TODO: Check Limits with exec.m_tagRunningCounts

    auto tasksOutBits = lgrn::bit_view(tasksOut);

    std::size_t const tagIntSize = tags.tag_ints_per_task();
    BitSpanConst_t const taskTagInts = Corrade::Containers::arrayView(tasks.m_taskTags);

    // Iterate all tasks and use mask to match which ones can run
    for (uint32_t const currTask : tasks.m_tasks.bitview().zeros())
    {
        if (exec.m_taskQueuedCounts[currTask] == 0)
        {
            continue; // Task not queued to run
        }

        std::size_t const offset = currTask * tagIntSize;
        auto const currTaskTagInts = taskTagInts.slice(offset, offset + tagIntSize);

        if (tags_present(mask, currTaskTagInts))
        {
            tasksOutBits.set(currTask);
        }
    }
}

void task_start(Tags const& tags, Tasks const& tasks, ExecutionContext &rExec, TaskId const task)
{
    auto currTaskTagInts = task_tags_2d(tags, tasks)[std::size_t(task)].asContiguous();

    auto const view = lgrn::bit_view(currTaskTagInts);
    for (uint32_t const tag : view.ones())
    {
        rExec.m_tagRunningCounts[tag] ++;
    }
}

void task_finish(Tags const& tags, Tasks const& tasks, ExecutionContext &rExec, TaskId const task, BitSpan_t tmpEnqueue)
{
    auto currTaskTagInts = task_tags_2d(tags, tasks)[std::size_t(task)].asContiguous();

    auto const externBits = lgrn::bit_view(tags.m_tagExtern);

    rExec.m_taskQueuedCounts[std::size_t(task)] --;

    auto enqueueBits = lgrn::bit_view(tmpEnqueue);
    bool somethingEnqueued = false;

    auto const view = lgrn::bit_view(currTaskTagInts);
    for (uint32_t const tag : view.ones())
    {
        rExec.m_tagRunningCounts[tag] --;
        rExec.m_tagIncompleteCounts[tag] --;

        // If last task to finish with this tag
        if (rExec.m_tagIncompleteCounts[tag] == 0)
        {
            // Reset external dependency bit
            if (externBits.test(tag))
            {
                task_extern_set(rExec, TagId(tag), false);
            }

            // Handle enqueue tags
            if ( ! tmpEnqueue.isEmpty() )
            {
                TagId const enqueue = tags.m_tagEnqueues[tag];
                if ((enqueue != lgrn::id_null<TagId>()) )
                {
                    somethingEnqueued = true;
                    enqueueBits.set(std::size_t(enqueue));
                }
            }
        }
    }

    if (somethingEnqueued)
    {
        task_enqueue(tags, tasks, rExec, tmpEnqueue);
        enqueueBits.reset();
    }
}

} // namespace osp
