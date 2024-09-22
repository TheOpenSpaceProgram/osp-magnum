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

#include "execute.h"

#include <Corrade/Containers/ArrayViewStl.h>
#include <iterator>

namespace osp
{

static void exec_log(ExecContext &rExec, ExecContext::LogMsg_t msg) noexcept;

static void pipeline_run_root(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec, PipelineId pipeline) noexcept;

static int pipeline_run(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec, bool rerunLoop, PipelineId pipeline, PipelineTreePos_t treePos, uint32_t descendents, bool isLoopScope, bool insideLoopScope);

static void pipeline_advance_stage(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec, PipelineId pipeline) noexcept;

static void pipeline_advance_reqs(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec, PipelineId pipeline) noexcept;

static void pipeline_advance_run(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec, PipelineId pipeline) noexcept;

static constexpr bool pipeline_can_advance(ExecPipeline &rExecPl) noexcept;

static void pipeline_try_advance(ExecContext &rExec, ExecPipeline &rExecPl, PipelineId pipeline) noexcept;

static void pipeline_cancel(Tasks const& tasks, TaskGraph const& graph, ExecContext& rExec, ExecPipeline& rExecPl, PipelineId pipeline) noexcept;

struct ArgsForIsPipelineInLoop
{
    PipelineId viewedFrom;
    PipelineId insideLoop;
};

static bool is_pipeline_in_loop(Tasks const& tasks, TaskGraph const& graph, ExecContext const& exec, ArgsForIsPipelineInLoop args) noexcept;

struct ArgsForSubtreeForEach
{
    PipelineTreePos_t   root;
    bool                includeRoot;
};

template <typename FUNC_T>
static void subtree_for_each(ArgsForSubtreeForEach args, TaskGraph const& graph, ExecContext const& rExec, FUNC_T&& func);

//-----------------------------------------------------------------------------

// Main entry-point functions

void exec_update(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec) noexcept
{
    exec_log(rExec, ExecContext::UpdateStart{});

    if (rExec.hasRequestRun)
    {
        for (ExecPipeline const& rExecPl : rExec.plData)
        {
            LGRN_ASSERTM( ! rExecPl.running, "Running new pipelines while already running is not yet supported ROFL! "
                                             "Make sure all pipelines are finished running.");
        }

        for (PipelineId const plId : rExec.plRequestRun)
        {
            pipeline_run_root(tasks, graph, rExec, plId);
        }
        rExec.plRequestRun.clear();
        rExec.hasRequestRun = false;
    }


    while (rExec.hasPlAdvanceOrLoop)
    {
        exec_log(rExec, ExecContext::UpdateCycle{});

        rExec.hasPlAdvanceOrLoop = false;

        for (auto const [pipeline, treePos] : rExec.requestLoop)
        {
            ExecPipeline &rExecPl = rExec.plData[pipeline];
            LGRN_ASSERT(rExecPl.loopChildrenLeft == 0);
        }

        rExec.requestLoop.clear();

        for (PipelineId const plId : rExec.plAdvance)
        {
            pipeline_advance_stage(tasks, graph, rExec, plId);
        }

        for (PipelineId const plId : rExec.plAdvance)
        {
            pipeline_advance_reqs(tasks, graph, rExec, plId);
        }

        for (PipelineId const plId : rExec.plAdvance)
        {
            pipeline_advance_run(tasks, graph, rExec, plId);
        }
        rExec.plAdvance.clear();
        
        rExec.plAdvance.insert(rExec.plAdvanceNext.begin(), rExec.plAdvanceNext.end());
        rExec.plAdvanceNext.clear();
    }

    exec_log(rExec, ExecContext::UpdateEnd{});
}

void complete_task(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec, TaskId const task, TaskActions actions) noexcept
{
    LGRN_ASSERT(rExec.tasksQueuedRun.contains(task));
    rExec.tasksQueuedRun.erase(task);

    exec_log(rExec, ExecContext::CompleteTask{task});

    auto const [pipeline, stage] = tasks.m_taskRunOn[task];
    ExecPipeline &rExecPl = rExec.plData[pipeline];

    -- rExecPl.tasksQueuedRun;

    pipeline_try_advance(rExec, rExecPl, pipeline);

    if (actions & TaskAction::Cancel)
    {
        LGRN_ASSERTMV(rExecPl.tasksQueuedRun == 0 && rExecPl.tasksQueuedBlocked == 0,
                      "Tasks that cancel the pipeline should be the only task running. Too lazy to enforce this elsewhere LOL",
                      rExecPl.tasksQueuedRun, rExecPl.tasksQueuedBlocked);
        pipeline_cancel(tasks, graph, rExec, rExecPl, pipeline);
    }

    // Handle stages requiring this task
    for (AnyStageId const& reqTaskAnystg : fanout_view(graph.taskToFirstRevStgreqtask, graph.revStgreqtaskToStage, task))
    {
        PipelineId const    reqPl       = graph.anystgToPipeline[reqTaskAnystg];
        StageId const       reqStg      = stage_from(graph, reqPl, reqTaskAnystg);
        ExecPipeline        &rReqExecPl = rExec.plData[reqPl];

        if (rReqExecPl.stage == reqStg)
        {
            // True if 'reqPl' sees that 'pipeline' is inside a loop, and may run more times
            bool const loopsRelative = rExecPl.loop && is_pipeline_in_loop(tasks, graph, rExec, {.viewedFrom = reqPl, .insideLoop = pipeline});

            if ( ( ! loopsRelative ) || rExecPl.canceled )
            {
                LGRN_ASSERT(rReqExecPl.ownStageReqTasksLeft != 0);
                -- rReqExecPl.ownStageReqTasksLeft;
                pipeline_try_advance(rExec, rReqExecPl, reqPl);
            }
        }
        else
        {
            LGRN_ASSERTMV(int(rReqExecPl.stage) < int(reqStg) && rReqExecPl.stage != lgrn::id_null<StageId>(),
                          "Stage-requires-Task means that rReqExecPl.stage cannot advance any further than reqStg until task completes.",
                          int(task), int(rReqExecPl.stage), int(reqStg));
        }
    }

    // Handle this task requiring stages from other pipelines
    for (TaskRequiresStage const& req : fanout_view(graph.taskToFirstTaskreqstg, graph.taskreqstgData, task))
    {
        ExecPipeline &rReqExecPl = rExec.plData[req.reqPipeline];

        LGRN_ASSERTMV(rReqExecPl.stage == req.reqStage,
                      "Task-requires-Stage means this task should have not run unless the stage is selected",
                      int(task), int(rReqExecPl.stage), int(req.reqStage));

        bool const loopsRelative = rExecPl.loop && is_pipeline_in_loop(tasks, graph, rExec, {.viewedFrom = req.reqPipeline, .insideLoop = pipeline});

        if ( ( ! loopsRelative ) || rExecPl.canceled )
        {
            LGRN_ASSERT(rReqExecPl.tasksReqOwnStageLeft != 0);
            -- rReqExecPl.tasksReqOwnStageLeft;
            pipeline_try_advance(rExec, rReqExecPl, req.reqPipeline);
        }
        // else: This pipeline is enclosed in a loop relative to dependency. This task may run
        //       more times.
    }
}

void exec_signal(ExecContext &rExec, PipelineId pipeline) noexcept
{
    ExecPipeline &rExecPl = rExec.plData[pipeline];
    exec_log(rExec, ExecLog::ExternalSignal{pipeline, ! rExecPl.waitSignaled});

    if ( ! rExecPl.waitSignaled )
    {
        rExecPl.waitSignaled = true;
        pipeline_try_advance(rExec, rExecPl, pipeline);
    }
}

//-----------------------------------------------------------------------------

// Major steps

static void pipeline_run_root(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec, PipelineId const pipeline) noexcept
{
    exec_log(rExec, ExecLog::ExternalRunRequest{pipeline});

    PipelineTreePos_t const treePos = graph.pipelineToPltree[pipeline];

    bool const isLoopScope = tasks.m_pipelineControl[pipeline].isLoopScope;

    uint32_t const descendents = (treePos != lgrn::id_null<PipelineTreePos_t>())
                               ? graph.pltreeDescendantCounts[treePos]
                               : 0;

    pipeline_run(tasks, graph, rExec, false, pipeline, treePos, descendents, isLoopScope, false);
}

static int pipeline_run(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec, bool const rerunLoop, PipelineId const pipeline, PipelineTreePos_t const treePos, uint32_t const descendents, bool const isLoopScope, bool const insideLoopScope)
{
    auto const stageCount = fanout_size(graph.pipelineToFirstAnystg, pipeline);
    ExecPipeline &rExecPl = rExec.plData[pipeline];

    if (rExecPl.stage != lgrn::id_null<StageId>())
    {
        return 0;
    }

    if (stageCount != 0)
    {
        if ( ! rExecPl.running )
        {
            rExecPl.running  = true;
            rExecPl.loop     = isLoopScope || insideLoopScope;

            ++ rExec.pipelinesRunning;

            rExec.plAdvance.insert(pipeline);
            exec_log(rExec, ExecContext::PipelineRun{pipeline});
        }

        if (rerunLoop)
        {
            rExecPl.canceled = false;

            rExec.plAdvanceNext.insert(pipeline);
            exec_log(rExec, ExecContext::PipelineLoop{pipeline});
        }

        rExec.hasPlAdvanceOrLoop = true;
    }

    if (descendents == 0)
    {
        return 0;
    }

    PipelineTreePos_t const childPosLast = treePos + 1 + descendents;
    PipelineTreePos_t       childPos     = treePos + 1;

    int scopeChildCount = 0;

    while (childPos != childPosLast)
    {
        uint32_t const      childDescendents    = graph.pltreeDescendantCounts[childPos];
        PipelineId const    childPl             = graph.pltreeToPipeline[childPos];
        bool const          childIsLoopScope    = tasks.m_pipelineControl[childPl].isLoopScope;

        int const childChildCount = pipeline_run(tasks, graph, rExec, rerunLoop, childPl, childPos, childDescendents, childIsLoopScope, isLoopScope || insideLoopScope);

        scopeChildCount += childChildCount + 1; // + 1 for child itself

        childPos += 1 + childDescendents;
    }

    if (isLoopScope)
    {
        LGRN_ASSERT(rExecPl.loopChildrenLeft == 0);
        rExecPl.loopChildrenLeft = scopeChildCount;

        return 0;
    }
    else
    {
        return scopeChildCount;
    }
}


static void loop_scope_done(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec, ExecPipeline &rExecPl, PipelineId const pipeline, PipelineTreePos_t const treePos)
{
    if ( ! rExecPl.canceled )
    {
        // Loop more

        uint32_t const descendents = graph.pltreeDescendantCounts[treePos];
        pipeline_run(tasks, graph, rExec, true, pipeline, treePos, descendents, true,  /* dont-care */ false);
    }
    else
    {
        // Loop finished

        LGRN_ASSERT(rExecPl.loopChildrenLeft == 0);

        subtree_for_each(
            {.root = treePos, .includeRoot = true}, graph, rExec,
            [&tasks, &graph, &rExec]
            (PipelineTreePos_t const loopPos, PipelineId const loopPipeline, uint32_t const descendants)
        {
            ExecPipeline &rLoopExecPl = rExec.plData[loopPipeline];

            LGRN_ASSERT(rLoopExecPl.stage == lgrn::id_null<StageId>());

            if (rLoopExecPl.running)
            {
                LGRN_ASSERT(rExec.pipelinesRunning != 0);
                -- rExec.pipelinesRunning;
            }

            rLoopExecPl.running  = false;
            rLoopExecPl.canceled = false;
            rLoopExecPl.loop     = false;


            exec_log(rExec, ExecContext::PipelineLoopFinish{loopPipeline});
        });

        // If this is a nested loop, decrement parent loop scope's loopChildrenLeft

        PipelineId const parentPl = tasks.m_pipelineParents[pipeline];
        if (parentPl == lgrn::id_null<PipelineId>())
        {
            return; // Loop is in the root
        }

        PipelineTreePos_t const parentScopeTreePos = graph.pipelineToLoopScope[parentPl];
        if (parentScopeTreePos == lgrn::id_null<PipelineTreePos_t>())
        {
            return; // Parent does not loop
        }

        PipelineId const parentScopePl = graph.pltreeToPipeline[parentScopeTreePos];
        ExecPipeline &rParentScopeExecPl = rExec.plData[parentScopePl];

        LGRN_ASSERT(rParentScopeExecPl.loopChildrenLeft != 0);
        -- rParentScopeExecPl.loopChildrenLeft;

        if (rParentScopeExecPl.loopChildrenLeft == 0)
        {
            loop_scope_done(tasks, graph, rExec, rParentScopeExecPl, parentScopePl, parentScopeTreePos);
        }
    }
}

static void loop_done(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec, ExecPipeline &rExecPl, PipelineId const pipeline)
{
    rExecPl.stage = lgrn::id_null<StageId>();

    PipelineTreePos_t scopeTreePos  = graph.pipelineToLoopScope[pipeline];
    PipelineId        scopePl       = graph.pltreeToPipeline[scopeTreePos];
    ExecPipeline      &rScopeExecPl = rExec.plData[scopePl];

    if (scopePl != pipeline) // if not a loopscope itself
    {
        LGRN_ASSERTM(rScopeExecPl.loopChildrenLeft != 0,
                     "A pipeline called 'loop done' more than once. I'm not sure why, but if you "
                     "hit this assert, then its likely that you're forgetting to fire all "
                     "required pipeline signals.");
        -- rScopeExecPl.loopChildrenLeft;
    }

    if (rScopeExecPl.loopChildrenLeft == 0 && rScopeExecPl.stage == lgrn::id_null<StageId>())
    {
        loop_scope_done(tasks, graph, rExec, rScopeExecPl, scopePl, scopeTreePos);
    }
}

static void pipeline_advance_stage(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec, PipelineId const pipeline) noexcept
{
    ExecPipeline &rExecPl = rExec.plData[pipeline];

    LGRN_ASSERT(pipeline_can_advance(rExecPl));
    // * rExecPl.ownStageReqTasksLeft == 0;
    // * rExecPl.tasksReqOwnStageLeft == 0;

    auto const stageCount = fanout_size(graph.pipelineToFirstAnystg, pipeline);
    LGRN_ASSERTM(stageCount != 0, "Pipelines with 0 stages shouldn't be running");

    bool const justStarting = rExecPl.stage == lgrn::id_null<StageId>();

    auto const nextStage = StageId( justStarting ? 0 : (int(rExecPl.stage)+1) );

    rExecPl.tasksQueueDone = false;

    if (rExecPl.stage == rExecPl.waitStage)
    {
        rExecPl.waitSignaled = false;
    }

    if (nextStage != StageId(stageCount))
    {
        exec_log(rExec, ExecContext::StageChange{pipeline, rExecPl.stage, nextStage});

        // Proceed to next stage since its valid
        rExecPl.stage = nextStage;
    }
    else if ( rExecPl.loop )
    {
        loop_done(tasks, graph, rExec, rExecPl, pipeline);
    }
    else
    {
        // Finished running
        rExecPl.stage    = lgrn::id_null<StageId>();
        rExecPl.running  = false;
        rExecPl.canceled = false;

        LGRN_ASSERT(rExec.pipelinesRunning != 0);
        -- rExec.pipelinesRunning;

        exec_log(rExec, ExecContext::PipelineFinish{pipeline});
    }
}

static void pipeline_advance_reqs(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec, PipelineId const pipeline) noexcept
{
    ExecPipeline &rExecPl = rExec.plData[pipeline];

    if (rExecPl.stage == lgrn::id_null<StageId>())
    {
        return;
    }

    AnyStageId const anystg = anystg_from(graph, pipeline, rExecPl.stage);

    // Evaluate Task-requires-Stages
    // These are tasks from other pipelines that require nextStage

    auto const revTaskReqStageView = ArrayView<TaskId const>(fanout_view(graph.anystgToFirstRevTaskreqstg, graph.revTaskreqstgToTask, anystg));

    // Number of tasks that require this stage. This is decremented when required tasks finish
    rExecPl.tasksReqOwnStageLeft = int(revTaskReqStageView.size());

    for (TaskId const task : revTaskReqStageView)
    {
        if (rExec.tasksQueuedBlocked.contains(task))
        {
            // Unblock tasks that are alredy queued
            BlockedTask &rBlocked = rExec.tasksQueuedBlocked.get(task);
            -- rBlocked.reqStagesLeft;
            if (rBlocked.reqStagesLeft == 0)
            {
                exec_log(rExec, ExecContext::UnblockTask{task});
                ExecPipeline &rTaskPlExec = rExec.plData[rBlocked.pipeline];
                -- rTaskPlExec.tasksQueuedBlocked;
                ++ rTaskPlExec.tasksQueuedRun;
                rExec.tasksQueuedRun.push(task);
                rExec.tasksQueuedBlocked.erase(task);
            }
        }
        else
        {
            auto const [reqPipeline, reqStage] = tasks.m_taskRunOn[task];
            ExecPipeline &rReqExecPl = rExec.plData[reqPipeline];
            if ( ! rReqExecPl.running || rReqExecPl.canceled)
            {
                // Task is done or cancelled
                -- rExecPl.tasksReqOwnStageLeft;
            }
        }
    }

    // Evaluate Stage-requires-Tasks
    // To be allowed to advance to the next-stage, these tasks must be complete.

    auto const stgreqtaskView = ArrayView<StageRequiresTask const>(fanout_view(graph.anystgToFirstStgreqtask, graph.stgreqtaskData, anystg));

    rExecPl.ownStageReqTasksLeft = int(stgreqtaskView.size());

    // Decrement ownStageReqTasksLeft, as some of these tasks might already be complete
    for (StageRequiresTask const& stgreqtask : stgreqtaskView)
    {
        ExecPipeline &rReqTaskExecPl = rExec.plData[stgreqtask.reqPipeline];

        // NOLINTBEGIN(bugprone-branch-clone)
        bool const reqTaskDone = [&tasks, &rReqTaskExecPl, &stgreqtask, &rExec] () noexcept -> bool
        {
            if ( ! rReqTaskExecPl.running )
            {
                return true; // Not running, which means the whole pipeline finished already
            }
            else if (rReqTaskExecPl.canceled)
            {
                return true; // Stage cancelled. Required task is considered finish and will never run
            }
            else if (int(rReqTaskExecPl.stage) < int(stgreqtask.reqStage))
            {
                return false; // Not yet reached required stage. Required task didn't run yet
            }
            else if (int(rReqTaskExecPl.stage) > int(stgreqtask.reqStage))
            {
                return true; // Passed required stage. Required task finished
            }
            else if ( ! rReqTaskExecPl.tasksQueueDone )
            {
                return false; // Required tasks not queued yet
            }
            else if (   rExec.tasksQueuedBlocked.contains(stgreqtask.reqTask)
                     || rExec.tasksQueuedRun    .contains(stgreqtask.reqTask))
            {
                return false; // Required task is queued and not yet finished running
            }
            else
            {
                return true; // On the right stage and task not running. This means it's done
            }
        }();
        // NOLINTEND(bugprone-branch-clone)

        if (reqTaskDone)
        {
            -- rExecPl.ownStageReqTasksLeft;
        }
    }
}

static void pipeline_advance_run(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec, PipelineId const pipeline) noexcept
{
    ExecPipeline &rExecPl = rExec.plData[pipeline];

    if (rExecPl.stage == lgrn::id_null<StageId>())
    {
        return;
    }

    bool noTasksRun = true;

    if ( ! rExecPl.canceled )
    {
        auto const anystg   = anystg_from(graph, pipeline, rExecPl.stage);
        auto const runTasks = ArrayView<TaskId const>{fanout_view(graph.anystgToFirstRuntask, graph.runtaskToTask, anystg)};

        noTasksRun = runTasks.size() == 0;

        for (TaskId task : runTasks)
        {
            LGRN_ASSERTM( ! rExec.tasksQueuedBlocked.contains(task), "Impossible to queue a task that's already queued");
            LGRN_ASSERTM( ! rExec.tasksQueuedRun    .contains(task), "Impossible to queue a task that's already queued");

            // Evaluate Task-requires-Stages
            // Some requirements may already be satisfied
            auto const taskreqstageView = ArrayView<const TaskRequiresStage>(fanout_view(graph.taskToFirstTaskreqstg, graph.taskreqstgData, task));
            auto reqStagesLeft = int(taskreqstageView.size());

            for (TaskRequiresStage const& req : taskreqstageView)
            {
                ExecPipeline const &rReqPlData = rExec.plData[req.reqPipeline];

                LGRN_ASSERTMV(rReqPlData.running, "Required pipelines must be running", TaskInt(task), PipelineInt(req.reqPipeline));

                if (rReqPlData.stage == req.reqStage)
                {
                    reqStagesLeft --;
                }
            }

            bool const blocked = reqStagesLeft != 0;

            if (blocked)
            {
                rExec.tasksQueuedBlocked.emplace(task, BlockedTask{reqStagesLeft, pipeline});
                ++ rExecPl.tasksQueuedBlocked;
            }
            else
            {
                rExec.tasksQueuedRun.push(task);
                ++ rExecPl.tasksQueuedRun;
            }

            exec_log(rExec, ExecContext::EnqueueTask{pipeline, rExecPl.stage, task, blocked});
            if (rExec.doLogging)
            {
                for (TaskRequiresStage const& req : taskreqstageView)
                {
                    ExecPipeline const &rReqPlData = rExec.plData[req.reqPipeline];

                    exec_log(rExec, ExecContext::EnqueueTaskReq{req.reqPipeline, req.reqStage, rReqPlData.stage == req.reqStage});
                }
            }
        }
    }

    rExecPl.tasksQueueDone = true;

    if (noTasksRun && pipeline_can_advance(rExecPl))
    {
        // No tasks to run. RunTasks are responsible for setting this pipeline dirty once they're
        // all done. If there is none, then this pipeline may get stuck if nothing sets it dirty,
        // so set dirty right away.
        rExec.plAdvanceNext.insert(pipeline);
        rExec.hasPlAdvanceOrLoop = true;
    }
}

//-----------------------------------------------------------------------------

// Pipeline utility


static void pipeline_cancel(Tasks const& tasks, TaskGraph const& graph, ExecContext& rExec, ExecPipeline& rExecPl, PipelineId pipeline) noexcept
{
    PipelineTreePos_t const treePos = graph.pipelineToPltree[pipeline];

    auto const cancel_stage_ahead = [&tasks, &graph, &rExec, &rExecPl, pipeline] (AnyStageId const anystg)
    {
        for (TaskId task : fanout_view(graph.anystgToFirstRuntask, graph.runtaskToTask, anystg))
        {
            // Stages depend on this RunTask (reverse Stage-requires-Task)
            for (AnyStageId const& reqTaskAnystg : fanout_view(graph.taskToFirstRevStgreqtask, graph.revStgreqtaskToStage, task))
            {
                PipelineId const    reqPl       = graph.anystgToPipeline[reqTaskAnystg];
                StageId const       reqStg      = stage_from(graph, reqPl, reqTaskAnystg);
                ExecPipeline        &rReqExecPl = rExec.plData[reqPl];

                if (rReqExecPl.stage == reqStg)
                {
                    // True if 'reqPl' sees that 'pipeline' is inside a loop, and may run more times
                    bool const loopsRelative = rExecPl.loop && is_pipeline_in_loop(tasks, graph, rExec, {.viewedFrom = reqPl, .insideLoop = pipeline});

                    if ( ! loopsRelative )
                    {
                        LGRN_ASSERT(rReqExecPl.ownStageReqTasksLeft != 0);
                        -- rReqExecPl.ownStageReqTasksLeft;
                        pipeline_try_advance(rExec, rReqExecPl, reqPl);
                    }
                }
            }

            // RunTask depends on stages (Task-requires-Stage)
            for (TaskRequiresStage const& req : fanout_view(graph.taskToFirstTaskreqstg, graph.taskreqstgData, task))
            {
                ExecPipeline &rReqExecPl = rExec.plData[req.reqPipeline];

                if (rReqExecPl.stage == req.reqStage)
                {
                    bool const loopsRelative = rExecPl.loop && is_pipeline_in_loop(tasks, graph, rExec, {.viewedFrom = req.reqPipeline, .insideLoop = pipeline});

                    if ( ! loopsRelative )
                    {
                        LGRN_ASSERT(rReqExecPl.tasksReqOwnStageLeft != 0);
                        -- rReqExecPl.tasksReqOwnStageLeft;
                        pipeline_try_advance(rExec, rReqExecPl, req.reqPipeline);
                    }
                }
            }
        }
    };

    auto const cancel_stage_blocked = [&tasks, &graph, &rExec, &rExecPl, pipeline] (AnyStageId const anystg, [[maybe_unused]] int blockedExpected)
    {
        for (TaskId task : fanout_view(graph.anystgToFirstRuntask, graph.runtaskToTask, anystg))
        {
            if ( ! rExec.tasksQueuedBlocked.contains(task) )
            {
                continue;
            }

            -- blockedExpected;
            rExec.tasksQueuedBlocked.erase(task);

            // Stages depend on this RunTask (reverse Stage-requires-Task)
            for (AnyStageId const& reqTaskAnystg : fanout_view(graph.taskToFirstRevStgreqtask, graph.revStgreqtaskToStage, task))
            {
                PipelineId const    reqPl       = graph.anystgToPipeline[reqTaskAnystg];
                StageId const       reqStg      = stage_from(graph, reqPl, reqTaskAnystg);
                ExecPipeline        &rReqExecPl = rExec.plData[reqPl];

                if (rReqExecPl.stage == reqStg)
                {
                    // True if 'reqPl' sees that 'pipeline' is inside a loop, and may run more times
                    bool const loopsRelative = rExecPl.loop && is_pipeline_in_loop(tasks, graph, rExec, {.viewedFrom = reqPl, .insideLoop = pipeline});

                    if ( ! loopsRelative )
                    {
                        LGRN_ASSERT(rReqExecPl.ownStageReqTasksLeft != 0);
                        -- rReqExecPl.ownStageReqTasksLeft;
                        pipeline_try_advance(rExec, rReqExecPl, reqPl);
                    }
                }
            }

            // RunTask depends on stages (Task-requires-Stage)
            for (TaskRequiresStage const& req : fanout_view(graph.taskToFirstTaskreqstg, graph.taskreqstgData, task))
            {
                ExecPipeline &rReqExecPl = rExec.plData[req.reqPipeline];

                if (rReqExecPl.stage == req.reqStage)
                {
                    bool const loopsRelative = rExecPl.loop && is_pipeline_in_loop(tasks, graph, rExec, {.viewedFrom = req.reqPipeline, .insideLoop = pipeline});

                    if ( ! loopsRelative )
                    {
                        LGRN_ASSERT(rReqExecPl.tasksReqOwnStageLeft != 0);
                        -- rReqExecPl.tasksReqOwnStageLeft;
                        pipeline_try_advance(rExec, rReqExecPl, req.reqPipeline);
                    }
                }
            }
        }

        LGRN_ASSERT(blockedExpected == 0);
    };

    subtree_for_each(
            {.root = treePos, .includeRoot = true}, graph, rExec,
            [&cancel_stage_ahead, &cancel_stage_blocked, &tasks, &graph, &rExec, &rExecPl, pipeline]
            (PipelineTreePos_t const cancelPos, PipelineId const cancelPl, uint32_t const descendants)
    {
        ExecPipeline &rCancelExecPl = rExec.plData[cancelPl];

        if ( ! rCancelExecPl.canceled )
        {
            rCancelExecPl.canceled = true;

            if (rCancelExecPl.tasksQueuedBlocked != 0)
            {
                cancel_stage_blocked(anystg_from(graph, cancelPl, StageId(rCancelExecPl.stage)), rCancelExecPl.tasksQueuedBlocked);
                rCancelExecPl.tasksQueuedBlocked = 0;
            }

            auto const stageCount  = int(fanout_size(graph.pipelineToFirstAnystg, cancelPl));
            auto const stagesAhead = int(rCancelExecPl.stage) + 1;

            // TODO: As of now, only stages ahead that have not yet run yet are being canceled.
            //       This is a problem for loops, where stages behind that have already run still
            //       have dependencies, since they may run again. Eventually deal with these.

            auto anystgInt = uint32_t(anystg_from(graph, cancelPl, StageId(stagesAhead)));
            for (auto stgInt = stagesAhead; stgInt < stageCount; ++stgInt, ++anystgInt)
            {
                cancel_stage_ahead(AnyStageId(anystgInt));
            }

            // In this case, "stage == null" means the pipeline is done; advancing it would
            // accidentally restart the pipeline.
            if (rCancelExecPl.stage != lgrn::id_null<StageId>())
            {
                pipeline_try_advance(rExec, rCancelExecPl, cancelPl);
            }

            exec_log(rExec, ExecContext::PipelineCancel{cancelPl, rCancelExecPl.stage});
        }
    });
}

static void pipeline_try_advance(ExecContext &rExec, ExecPipeline &rExecPl, PipelineId const pipeline) noexcept
{
    if (pipeline_can_advance(rExecPl))
    {
        rExec.plAdvance.insert(pipeline);
        rExec.hasPlAdvanceOrLoop = true;
    }
};

//-----------------------------------------------------------------------------

// Read-only checks

static constexpr bool pipeline_can_advance(ExecPipeline &rExecPl) noexcept
{
    // Pipeline can advance if...
    return
           rExecPl.running

        // Tasks required by stage are done
        && rExecPl.ownStageReqTasksLeft == 0

        // Not required by any tasks
        && rExecPl.tasksReqOwnStageLeft == 0

        // Tasks done
        && ( rExecPl.tasksQueuedBlocked + rExecPl.tasksQueuedRun ) == 0

        // Trigger checks out. only if...
        && (
            // Wait is disabled
                ( rExecPl.waitStage == lgrn::id_null<StageId>() )

            // Or not on specified wait stage
             || ( rExecPl.waitStage != rExecPl.stage )

            // Or wait is externally signaled to go
             || rExecPl.waitSignaled
           );

}

/**
 * @brief Check if a pipeline 'sees' another pipeline is enclosed within a non-cancelled loop
 */
static bool is_pipeline_in_loop(Tasks const& tasks, TaskGraph const& graph, ExecContext const& exec, ArgsForIsPipelineInLoop const args) noexcept
{
    PipelineTreePos_t const insideLoopScopePos = graph.pipelineToLoopScope[args.insideLoop];

    if (insideLoopScopePos == lgrn::id_null<PipelineTreePos_t>())
    {
        return false; // insideLoop is in the root, not inside a loop lol
    }

    PipelineTreePos_t const viewedFromScopePos = graph.pipelineToLoopScope[args.viewedFrom];

    if (insideLoopScopePos == viewedFromScopePos)
    {
        return false; // Both pipelines are in the same loop scope
    }
    else if (viewedFromScopePos == lgrn::id_null<PipelineTreePos_t>())
    {
        // viewedFrom is in the root, and insideLoop is enclosed in a loop.

        if (exec.plData[args.insideLoop].canceled)
        {
            // NOLINTBEGIN(readability-simplify-boolean-expr)
            if (exec.plData[graph.pltreeToPipeline[insideLoopScopePos]].canceled)
            {
                return false; // insideLoop is canceled and will not loop again
            }
            else
            {
                return true; // insideLoop itself is canceled, but is parented to a non-canceled
                             // loop scope. It will loop more times.
            }
            // NOLINTEND(readability-simplify-boolean-expr)
        }
        else // insideLoop is canceled
        {
            return true; // insideLoop is not canceled and will loop more
        }
    }
    else
    {
        // Check tree if insideLoopScopePos is a descendent of viewedFromScopePos
        PipelineTreePos_t const ancestor            = viewedFromScopePos;
        PipelineTreePos_t const ancestorLastChild   = viewedFromScopePos + 1 + graph.pltreeDescendantCounts[viewedFromScopePos];
        PipelineTreePos_t const descendent          = insideLoopScopePos;

        bool const isDescendent = (ancestor < descendent) && (descendent < ancestorLastChild);

        if (isDescendent)
        {
            if (exec.plData[args.insideLoop].canceled)
            {
                // NOLINTBEGIN(readability-simplify-boolean-expr)
                if (exec.plData[graph.pltreeToPipeline[insideLoopScopePos]].canceled)
                {
                    return false; // insideLoop is canceled and will not loop again
                }
                else
                {
                    return true; // insideLoop is in a nested loop and is canceled, but is parented
                                 // to a non-canceled loop scope. It will loop more times.
                }
                // NOLINTEND(readability-simplify-boolean-expr)
            }
            else
            {
                return true; // insideLoop is in a nested loop and is not canceled
            }
        }
        else
        {
            return false; // viewedFrom and insideLoop are unrelated
        }
    }
}

//-----------------------------------------------------------------------------

// Minor utility

void exec_conform(Tasks const& tasks, ExecContext &rOut)
{
    std::size_t const maxTasks      = tasks.m_taskIds.capacity();
    std::size_t const maxPipeline   = tasks.m_pipelineIds.capacity();

    rOut.tasksQueuedRun    .reserve(maxTasks);
    rOut.tasksQueuedBlocked.reserve(maxTasks);
    rOut.plData.resize(maxPipeline);
    rOut.plAdvance.resize(maxPipeline);
    rOut.plAdvanceNext.resize(maxPipeline);
    rOut.plRequestRun.resize(maxPipeline);

    for (PipelineId const pipeline : tasks.m_pipelineIds)
    {
        rOut.plData[pipeline].waitStage = tasks.m_pipelineControl[pipeline].waitStage;
    }
}

static void exec_log(ExecContext &rExec, ExecContext::LogMsg_t msg) noexcept
{
    if (rExec.doLogging)
    {
        rExec.logMsg.push_back(msg);
    }
}

template <typename FUNC_T>
static void subtree_for_each(ArgsForSubtreeForEach args, TaskGraph const& graph, ExecContext const& rExec, FUNC_T&& func)
{
    uint32_t const          descendants = graph.pltreeDescendantCounts[args.root];
    PipelineTreePos_t const lastPos     = args.root + 1 + descendants;
    PipelineTreePos_t const firstPos    = args.includeRoot ? args.root : (args.root+1);

    for (PipelineTreePos_t pos = firstPos; pos < lastPos; ++pos)
    {
        std::invoke(func, pos, graph.pltreeToPipeline[pos], descendants);
    }
}

} // namespace osp
