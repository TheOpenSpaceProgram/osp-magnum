/**
 * Open Space Program
 * Copyright © 2019-2023 Open Space Program Project
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
#include "tasks.h"

#include <array>

namespace osp
{

struct TaskCounts
{
    uint16_t requiresStages     {0};
    uint16_t requiredByStages   {0};
};

struct StageCounts
{
    uint16_t runTasks           {0};
    uint16_t requiresTasks      {0};
    uint16_t requiredByTasks    {0};
};

struct PipelineCounts
{
    std::array<StageCounts, gc_maxStages> stageCounts;

    uint16_t publishTo          {0};
    uint8_t  stages             {0};
};


TaskGraph make_exec_graph(Tasks const& tasks, ArrayView<TaskEdges const* const> const data)
{
    TaskGraph out;

    std::size_t const maxPipelines  = tasks.m_pipelineIds.capacity();
    std::size_t const maxTasks      = tasks.m_taskIds.capacity();

    KeyedVec<PipelineId, PipelineCounts>    plCounts;
    KeyedVec<TaskId, TaskCounts>            taskCounts;
    out.pipelineToFirstAnystg .resize(maxPipelines);
    plCounts        .resize(maxPipelines+1);
    taskCounts      .resize(maxTasks+1);

    std::size_t totalTasksReqStage  = 0;
    std::size_t totalStageReqTasks  = 0;
    std::size_t totalRunTasks       = 0;
    std::size_t totalSubscriptions  = 0;
    std::size_t totalStages         = 0;

    // 1. Count total number of stages

    auto const count_stage = [&plCounts] (PipelineId const pipeline, StageId const stage)
    {
        uint8_t &rStageCount = plCounts[pipeline].stages;
        rStageCount = std::max(rStageCount, uint8_t(uint8_t(stage) + 1));
    };

    // Count stages from task run-ons
    for (TaskInt const taskInt : tasks.m_taskIds.bitview().zeros())
    {
        auto const task                     = TaskId(taskInt);
        auto const [runPipeline, runStage]  = tasks.m_taskRunOn[task];

        count_stage(runPipeline, runStage);

        ++ plCounts[runPipeline].stageCounts[std::size_t(runStage)].runTasks;
        ++ totalRunTasks;
    }

    // Count stages from syncs
    for (TaskEdges const* pEdges : data)
    {
        for (auto const [task, pipeline, stage] : pEdges->m_syncWith)
        {
            count_stage(pipeline, stage);
        }
    }

    for (PipelineCounts const& plCount : plCounts)
    {
        totalStages += plCount.stages;
    }

    // 2. Count TaskRequiresStages and StageRequiresTasks and Subscriptions

    for (TaskEdges const* pEdges : data)
    {
        for (auto const [task, pipeline, stage] : pEdges->m_syncWith)
        {
            StageCounts &rStageCounts = plCounts[pipeline].stageCounts[std::size_t(stage)];
            TaskCounts  &rTaskCounts  = taskCounts[task];

            // TaskRequiresStage makes task require pipeline to be on stage to be allowed to run
            ++ rTaskCounts .requiresStages;
            ++ rStageCounts.requiredByTasks;


            // StageRequiresTask for stage to wait for task to complete
            ++ rStageCounts.requiresTasks;
            ++ rTaskCounts .requiredByStages;

        }
        totalTasksReqStage += pEdges->m_syncWith.size();
        totalStageReqTasks += pEdges->m_syncWith.size();

        for (auto const [publisher, subscriber] : pEdges->m_subscriptions)
        {
            ++ plCounts[publisher].publishTo;
        }
        totalSubscriptions += pEdges->m_subscriptions.size();
    }


    // 3. Allocate

    // The +1 is needed for 1-to-many connections to store the total number of other elements they
    // index. This also simplifies logic in fanout_view(...)

    out.pipelineToFirstAnystg       .resize(maxPipelines+1,     lgrn::id_null<AnyStageId>());
    out.anystgToPipeline            .resize(totalStages+1,      lgrn::id_null<PipelineId>());
    out.anystgToFirstRuntask        .resize(totalStages+1,      lgrn::id_null<RunTaskId>());
    out.runtaskToTask               .resize(totalRunTasks,      lgrn::id_null<TaskId>());
    out.pipelineToFirstSub          .resize(maxPipelines+1,     lgrn::id_null<PipelineSubId>());
    out.subToPipeline               .resize(totalSubscriptions, lgrn::id_null<PipelineId>());
    out.anystgToFirstStgreqtask     .resize(totalStages+1,      lgrn::id_null<StageReqTaskId>());
    out.stgreqtaskData              .resize(totalStageReqTasks, {});
    out.taskToFirstRevStgreqtask    .resize(maxTasks+1,         lgrn::id_null<ReverseStageReqTaskId>());
    out.revStgreqtaskToStage        .resize(totalStageReqTasks, lgrn::id_null<AnyStageId>());
    out.taskToFirstTaskreqstg       .resize(maxTasks+1,         lgrn::id_null<TaskReqStageId>());
    out.taskreqstgData              .resize(totalTasksReqStage, {});
    out.anystgToFirstRevTaskreqstg  .resize(totalStages+1,      lgrn::id_null<ReverseTaskReqStageId>());
    out.revTaskreqstgToTask         .resize(totalTasksReqStage, lgrn::id_null<TaskId>());

    // 4. Calculate one-to-many partitions

    fanout_partition(
        out.pipelineToFirstAnystg,
        [&plCounts] (PipelineId pl)                { return plCounts[pl].stages; },
        [&out] (PipelineId pl, AnyStageId claimed) { out.anystgToPipeline[claimed] = pl; });

    fanout_partition(
        out.anystgToFirstRuntask,
        [&plCounts, &out] (AnyStageId stg)
        {
            PipelineId const pl = out.anystgToPipeline[stg];
            if (pl == lgrn::id_null<PipelineId>())
            {
                return uint16_t(0);
            }
            StageId const stgLocal = stage_from(out, pl, stg);
            return plCounts[pl].stageCounts[std::size_t(stgLocal)].runTasks;
        },
        [] (AnyStageId, RunTaskId) { });

    fanout_partition(
        out.pipelineToFirstSub,
        [&plCounts] (PipelineId pl) { return plCounts[pl].publishTo; },
        [&out] (PipelineId, PipelineSubId) { });

    fanout_partition(
        out.anystgToFirstStgreqtask,
        [&plCounts, &out] (AnyStageId stg)
        {
            PipelineId const    pl          = out.anystgToPipeline[stg];
            if (pl == lgrn::id_null<PipelineId>())
            {
                return uint16_t(0);
            }
            StageId const       stgLocal    = stage_from(out, pl, stg);
            return plCounts[pl].stageCounts[std::size_t(stgLocal)].requiresTasks;
        },
        [&out] (AnyStageId stg, StageReqTaskId claimed) { out.stgreqtaskData[claimed].ownStage = stg; });
    fanout_partition(
        out.taskToFirstRevStgreqtask,
        [&taskCounts] (TaskId task)                         { return taskCounts[task].requiredByStages; },
        [&out] (TaskId, ReverseStageReqTaskId) { });

    fanout_partition(
        out.taskToFirstTaskreqstg,
        [&taskCounts] (TaskId task)                     { return taskCounts[task].requiresStages; },
        [&out] (TaskId task, TaskReqStageId claimed)    { out.taskreqstgData[claimed].ownTask = task; });
    fanout_partition(
        out.anystgToFirstRevTaskreqstg,
        [&plCounts, &out] (AnyStageId stg)
        {
            PipelineId const    pl          = out.anystgToPipeline[stg];
            if (pl == lgrn::id_null<PipelineId>())
            {
                return uint16_t(0);
            }
            StageId const       stgLocal    = stage_from(out, pl, stg);
            return plCounts[pl].stageCounts[std::size_t(stgLocal)].requiredByTasks;
        },
        [&out] (AnyStageId, ReverseTaskReqStageId) { });

    // 5. Push

    for (TaskInt const taskInt : tasks.m_taskIds.bitview().zeros())
    {
        auto const      task            = TaskId(taskInt);
        auto const      run             = tasks.m_taskRunOn[task];
        auto const      anystg          = anystg_from(out, run.pipeline, run.stage);
        StageCounts     &rStageCounts   = plCounts[run.pipeline].stageCounts[std::size_t(run.stage)];
        RunTaskId const runtask         = id_from_count(out.anystgToFirstRuntask, anystg, rStageCounts.runTasks);

        out.runtaskToTask[runtask] = task;

        -- rStageCounts.runTasks;
    }

    auto add_stagereqtask = [&plCounts, &taskCounts, &totalStageReqTasks, &out]
                            (PipelineId const pl, StageId const stg, TaskId const task)
    {
        AnyStageId const            anystg          = anystg_from(out, pl, stg);
        StageCounts                 &rStageCounts   = plCounts[pl].stageCounts[std::size_t(stg)];
        TaskCounts                  &rTaskCounts    = taskCounts[task];

        StageReqTaskId const        stgReqTaskId    = id_from_count(out.anystgToFirstStgreqtask, anystg, rStageCounts.requiresTasks);
        ReverseStageReqTaskId const revStgReqTaskId = id_from_count(out.taskToFirstRevStgreqtask, task, rTaskCounts.requiredByStages);

        StageRequiresTask &rStgReqTask = out.stgreqtaskData[stgReqTaskId];
        rStgReqTask.reqTask     = task;
        rStgReqTask.reqPipeline = pl;
        rStgReqTask.reqStage    = stg;
        out.revStgreqtaskToStage[revStgReqTaskId] = anystg;

        -- rStageCounts.requiresTasks;
        -- rTaskCounts.requiredByStages;
        -- totalStageReqTasks;
    };

    auto add_taskreqstage = [&plCounts, &taskCounts, &totalTasksReqStage, &out] (TaskId const task, PipelineId const pl, StageId const stg)
    {
        AnyStageId const            anystg          = anystg_from(out, pl, stg);
        StageCounts                 &rStageCounts   = plCounts[pl].stageCounts[std::size_t(stg)];
        TaskCounts                  &rTaskCounts    = taskCounts[task];

        TaskReqStageId const        taskReqStgId    = id_from_count(out.taskToFirstTaskreqstg, task, rTaskCounts.requiresStages);
        ReverseTaskReqStageId const revTaskReqStgId = id_from_count(out.anystgToFirstRevTaskreqstg, anystg, rStageCounts.requiredByTasks);

        TaskRequiresStage &rTaskReqStage = out.taskreqstgData[taskReqStgId];
        rTaskReqStage.reqStage      = stg;
        rTaskReqStage.reqPipeline   = pl;
        out.revTaskreqstgToTask[revTaskReqStgId] = task;

        -- rTaskCounts.requiresStages;
        -- rStageCounts.requiredByTasks;
        -- totalTasksReqStage;
    };

    for (TaskEdges const* pEdges : data)
    {

        for (auto const [task, pipeline, stage] : pEdges->m_syncWith)
        {
            add_stagereqtask(pipeline, stage, task);
            add_taskreqstage(task, pipeline, stage);
        }

    }

    [[maybe_unused]] auto const all_counts_zero = [&] ()
    {
        if (   totalStageReqTasks   != 0
            || totalTasksReqStage   != 0 )
        {
            return false;
        }
        for (PipelineCounts const& plCount : plCounts)
        {
            for (StageCounts const& stgCount : plCount.stageCounts)
            {
                if (   stgCount.requiredByTasks != 0
                    || stgCount.requiresTasks   != 0 )
                {
                    return false;
                }
            }
        }
        for (TaskCounts const& taskCount : taskCounts)
        {
            if (   taskCount.requiredByStages != 0
                || taskCount.requiredByStages != 0 )
            {
                return false;
            }
        }

        return true;
    };

    LGRN_ASSERTM(all_counts_zero(), "Counts repurposed as items remaining, and must all be zero by the end here");

    return out;
}

} // namespace osp

