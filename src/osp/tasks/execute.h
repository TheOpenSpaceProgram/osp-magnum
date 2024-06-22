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
#include "worker.h"

#include <longeron/id_management/id_set_stl.hpp>


#include <entt/entity/storage.hpp>

#include <cassert>
#include <variant>
#include <vector>

namespace osp
{

/**
 * @brief Per-Pipeline state needed for execution
 */
struct ExecPipeline
{

    /// Number of Tasks with satisfied dependencies that are ready to run
    int             tasksQueuedRun          { 0 };

    ///< Number of Tasks waiting for their TaskReqStage dependencies to be satisfied
    int             tasksQueuedBlocked      { 0 };

    /**
     * @brief Number of remaining TaskReqStage dependencies.
     *
     * Tasks from other running Pipelines require this Pipeline's current stage to be selected
     * in order to run. This Pipeline has to wait for these tasks to complete before proceeding to
     * the next stage.
     */
    int             tasksReqOwnStageLeft    { 0 };

    /**
     * @brief Number of remaining StageReqTask dependencies.
     *
     * This Pipeline's current stage require Tasks from other Pipelines to complete in order to
     * proceed to the next stage.
     */
    int             ownStageReqTasksLeft    { 0 };

    int             loopChildrenLeft        { 0 };

    StageId         stage                   { lgrn::id_null<StageId>() };

    StageId         waitStage               { lgrn::id_null<StageId>() };
    bool            waitSignaled            { false };

    bool            tasksQueueDone          { false };
    bool            loop                    { false };
    bool            running                 { false };
    bool            canceled                { false };
};

struct BlockedTask
{
    int             reqStagesLeft;
    PipelineId      pipeline;
};

struct LoopRequestRun
{
    PipelineId          pipeline;
    PipelineTreePos_t   treePos;
};


/**
 * @brief Fast plain-old-data log for ExecContext state changes
 */
struct ExecLog
{
    struct UpdateStart { };
    struct UpdateCycle { };
    struct UpdateEnd { };

    struct PipelineRun
    {
        PipelineId  pipeline;
    };

    struct PipelineFinish
    {
        PipelineId  pipeline;
    };

    struct PipelineCancel
    {
        PipelineId  pipeline;
        StageId     stage;
    };

    struct PipelineLoop
    {
        PipelineId  pipeline;
    };

    struct PipelineLoopFinish
    {
        PipelineId  pipeline;
    };

    struct StageChange
    {
        PipelineId  pipeline;
        StageId     stageOld;
        StageId     stageNew;
    };

    struct EnqueueTask
    {
        PipelineId  pipeline;
        StageId     stage;
        TaskId      task;
        bool        blocked;
    };

    struct EnqueueTaskReq
    {
        PipelineId  pipeline;
        StageId     stage;
        bool        satisfied;
    };

    struct UnblockTask
    {
        TaskId      task;
    };

    struct CompleteTask
    {
        TaskId      task;
    };

    struct ExternalRunRequest
    {
        PipelineId  pipeline;
    };

    struct ExternalSignal
    {
        PipelineId  pipeline;
        bool        ignored;
    };

    using LogMsg_t = std::variant<
            UpdateStart,
            UpdateCycle,
            UpdateEnd,
            PipelineRun,
            PipelineFinish,
            PipelineCancel,
            PipelineLoop,
            PipelineLoopFinish,
            StageChange,
            EnqueueTask,
            EnqueueTaskReq,
            UnblockTask,
            CompleteTask,
            ExternalRunRequest,
            ExternalSignal>;

    std::vector<LogMsg_t>           logMsg;
    bool                            doLogging{true};
}; // struct ExecLog

/**
 * @brief State for executing Tasks and TaskGraph
 */
struct ExecContext : public ExecLog
{
    KeyedVec<PipelineId, ExecPipeline>  plData;

    entt::basic_sparse_set<TaskId>              tasksQueuedRun;
    entt::basic_storage<BlockedTask, TaskId>    tasksQueuedBlocked;

    lgrn::IdSetStl<PipelineId>          plAdvance;
    lgrn::IdSetStl<PipelineId>          plAdvanceNext;
    bool                                hasPlAdvanceOrLoop  {false};

    lgrn::IdSetStl<PipelineId>          plRequestRun;
    std::vector<LoopRequestRun>         requestLoop;
    bool                                hasRequestRun {false};

    int                                 pipelinesRunning {0};

    // TODO: Consider multithreading. something something work stealing...
    //  * Allow multiple threads to search for and execute tasks. Atomic access
    //    for ExecContext? Might be messy to implement.
    //  * Only allow one thread to search for tasks, assign tasks to other
    //    threads if they're available before running own task. Another thread
    //    can take over once it completes its task. May be faster as only one
    //    thread is modifying ExecContext, and easier to implement.
    //  * Plug into an existing work queue library?

}; // struct ExecContext

void exec_conform(Tasks const& tasks, ExecContext &rOut);

inline void exec_request_run(ExecContext &rExec, PipelineId pipeline) noexcept
{
    rExec.plRequestRun.insert(pipeline);
    rExec.hasRequestRun = true;
}

void exec_signal(ExecContext &rExec, PipelineId pipeline) noexcept;

void exec_update(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec) noexcept;

void complete_task(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec, TaskId task, TaskActions actions) noexcept;



} // namespace osp
