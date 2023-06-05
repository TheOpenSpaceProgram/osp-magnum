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

#include "../bitvector.h"

#include <Corrade/Containers/ArrayViewStl.h>
#include <Corrade/Containers/StridedArrayView.h>

#include <entt/entity/sparse_set.hpp>

#include <cassert>
#include <vector>

namespace osp
{

struct StageCounts
{
    uint16_t        m_waitingTasks  {0};
};

struct ExecPipeline
{
    StageBits_t     m_triggered         {0};
    int             m_tasksQueuedRun    {0};
    int             m_tasksQueuedBlocked{0};
    StageId         m_currentStage      {0};
    uint32_t        m_waitingTasksTotal {0};

    std::array<StageCounts, gc_maxStages> m_stageCounts;
};

struct TaskWaiting
{
    TaskId          m_recipient;
    PipelineId      m_waitFor;
    StageId         m_waitForStage;
};

/**
 * @brief The ExecContext class
 *
 * Pipelines are marked dirty (m_plDirty.set(pipeline int)) if...
 * * All of its running tasks just finished
 * * Not running but required by another pipeline
 */
struct ExecContext
{
    void resize(Tasks const& tasks)
    {
        std::size_t const maxTasks      = tasks.m_taskIds.capacity();
        std::size_t const maxPipeline   = tasks.m_pipelineIds.capacity();

        m_tasksQueuedRun    .reserve(maxTasks);
        m_tasksQueuedBlocked.reserve(maxTasks);
        bitvector_resize(m_tasksTryRun, maxTasks);

        m_plData.resize(maxPipeline);
        bitvector_resize(m_plDirty,     maxPipeline);

        m_plNextStage.resize(maxPipeline);
        bitvector_resize(m_plRequired,  maxPipeline);
    }

    KeyedVec<PipelineId, ExecPipeline>  m_plData;
    BitVector_t                         m_plDirty;

    entt::basic_sparse_set<TaskId>      m_tasksQueuedRun;
    entt::basic_sparse_set<TaskId>      m_tasksQueuedBlocked;
    BitVector_t                         m_tasksTryRun;

    // used for updating

    BitVector_t                         m_plRequired;
    KeyedVec<PipelineId, StageId>       m_plNextStage;

    // TODO: Consider multithreading. something something work stealing...
    //  * Allow multiple threads to search for and execute tasks. Atomic access
    //    for ExecContext? Might be messy to implement.
    //  * Only allow one thread to search for tasks, assign tasks to other
    //    threads if they're available before running own task. Another thread
    //    can take over once it completes its task. May be faster as only one
    //    thread is modifying ExecContext, and easier to implement.
    //  * Plug into an existing work queue library?

}; // struct ExecContext

//inline bool compare_syncs(SyncWaiting const& lhs, SyncWaiting const& rhs)
//{
//    return (lhs.m_recipient < rhs.m_recipient)
//        && (lhs.m_recipient < rhs.m_waitFor);
//};

template<typename STAGE_ENUM_T>
inline void set_dirty(ExecContext &rExec, PipelineDef<STAGE_ENUM_T> const pipeline, STAGE_ENUM_T const stage)
{
    rExec.m_plData[pipeline].m_triggered |= 1 << int(stage);
    rExec.m_plDirty.set(std::size_t(pipeline));
}

void enqueue_dirty(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec) noexcept;

void mark_completed_task(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec, TaskId const task, FulfillDirty_t dirty) noexcept;



} // namespace osp
