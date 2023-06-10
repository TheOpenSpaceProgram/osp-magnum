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

#include "Corrade/Containers/ArrayViewStl.h"

#include <array>

namespace osp
{

struct TaskCounts
{
    uint8_t  runOn              {0};
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
    uint8_t     stages          {0};
    std::array<StageCounts, gc_maxStages> stageCounts;
};



template <typename KEY_T, typename VALUE_T, typename GETSIZE_T, typename CLAIM_T>
static void fill_many(KeyedVec<KEY_T, VALUE_T>& rVec, GETSIZE_T&& get_size, CLAIM_T&& claim)
{
    using key_int_t     = lgrn::underlying_int_type_t<KEY_T>;
    using value_int_t   = lgrn::underlying_int_type_t<VALUE_T>;

    value_int_t currentId = 0;
    for (value_int_t i = 0; i < rVec.size(); ++i)
    {
        value_int_t const size = get_size(KEY_T(i));

        rVec[KEY_T(i)] = VALUE_T(currentId);

        if (size != 0)
        {
            value_int_t const nextId = currentId + size;

            for (value_int_t j = currentId; j < nextId; ++j)
            {
                claim(KEY_T(i), VALUE_T(j));
            }

            currentId = nextId;
        }
    }
}

template <typename KEY_T, typename VALUE_T>
static VALUE_T id_from_count(KeyedVec<KEY_T, VALUE_T> const& vec, KEY_T const key, lgrn::underlying_int_type_t<VALUE_T> const count)
{
    return VALUE_T( lgrn::underlying_int_type_t<VALUE_T>(vec[KEY_T(lgrn::underlying_int_type_t<KEY_T>(key) + 1)]) - count );
}


TaskGraph make_exec_graph(Tasks const& tasks, ArrayView<TaskEdges const* const> const data)
{
    TaskGraph out;

    std::size_t const maxPipelines  = tasks.m_pipelineIds.capacity();
    std::size_t const maxTasks      = tasks.m_taskIds.capacity();

    // counts

    KeyedVec<PipelineId, PipelineCounts>    plCounts;
    KeyedVec<TaskId, TaskCounts>            taskCounts;
    out.pipelineToFirstAnystg .resize(maxPipelines);
    plCounts        .resize(maxPipelines+1);
    taskCounts      .resize(maxTasks+1);

    std::size_t totalTasksReqStage = 0;
    std::size_t totalStageReqTasks = 0;
    std::size_t totalRunTasks      = 0;
    std::size_t totalStages        = 0;

    // Count up which pipeline/stages each task runs on

    auto const count_stage = [&plCounts] (PipelineId const pipeline, StageId const stage)
    {
        uint8_t &rStageCount = plCounts[pipeline].stages;
        rStageCount = std::max(rStageCount, uint8_t(uint8_t(stage) + 1));
    };

    for (TaskEdges const* pEdges : data)
    {
        totalRunTasks += pEdges->m_runOn.size();

        for (auto const [task, pipeline, stage] : pEdges->m_runOn)
        {
            plCounts[pipeline].stageCounts[std::size_t(stage)].runTasks ++;
            taskCounts[task]    .runOn      ++;
            count_stage(pipeline, stage);
        }

        for (auto const [task, pipeline, stage] : pEdges->m_syncWith)
        {
            count_stage(pipeline, stage);
        }

        for (auto const [task, pipeline, stage] : pEdges->m_triggers)
        {
            count_stage(pipeline, stage);
        }
    }

    // Count total stages

    for (PipelineCounts const& plCount : plCounts)
    {
        totalStages += plCount.stages;
    }

    // Count TaskRequiresStages and StageRequiresTasks


    auto const count_stagereqtask = [&plCounts, &taskCounts, &totalStageReqTasks]
                                    (PipelineId const pl, StageId const stg, TaskId const task)
    {
        StageCounts &rStageCounts = plCounts[pl].stageCounts[std::size_t(stg)];
        TaskCounts  &rTaskCounts  = taskCounts[task];

        rStageCounts.requiresTasks    ++;
        rTaskCounts .requiredByStages ++;
        totalStageReqTasks            ++;
    };


    auto const count_taskreqstage = [&plCounts, &taskCounts, &totalTasksReqStage]
                                    (TaskId const task, PipelineId const pl, StageId const stg)
    {
        StageCounts &rStageCounts = plCounts[pl].stageCounts[std::size_t(stg)];
        TaskCounts  &rTaskCounts  = taskCounts[task];

        rTaskCounts .requiresStages  ++;
        rStageCounts.requiredByTasks ++;
        totalTasksReqStage           ++;
    };

    for (TaskEdges const* pEdges : data)
    {
        // Each sync-with adds...
        // * TaskRequiresStage makes task require pipeline to be on stage
        // * StageRequiresTask for stage to wait for task to complete
        for (auto const [task, pipeline, stage] : pEdges->m_syncWith)
        {
            count_stagereqtask(pipeline, stage, task);
            count_taskreqstage(task, pipeline, stage);
        }

        // Each triggers adds...
        // * StageRequiresTask on previous stage to wait for task to complete
        for (auto const [task, pipeline, stage] : pEdges->m_triggers)
        {
            count_stagereqtask(pipeline, stage_prev(stage, plCounts[pipeline].stages), task);
        }
    }

    // Allocate

    out.pipelineToFirstAnystg       .resize(maxPipelines+1,     lgrn::id_null<AnyStageId>());
    out.anystgToPipeline            .resize(totalStages+1,      lgrn::id_null<PipelineId>());
    out.anystgToFirstRuntask        .resize(totalStages+1,      lgrn::id_null<RunTaskId>());
    out.runtaskToTask               .resize(totalRunTasks,      lgrn::id_null<TaskId>());
    out.anystgToFirstStgreqtask     .resize(totalStages+1,      lgrn::id_null<StageReqTaskId>());
    out.stgreqtaskData              .resize(totalStageReqTasks, {});
    out.taskToFirstRevStgreqtask    .resize(maxTasks+1,         lgrn::id_null<ReverseStageReqTaskId>());
    out.revStgreqtaskToStage        .resize(totalStageReqTasks, lgrn::id_null<AnyStageId>());
    out.taskToFirstTaskreqstg       .resize(maxTasks+1,         lgrn::id_null<TaskReqStageId>());
    out.taskreqstgData              .resize(totalTasksReqStage, {});
    out.stageToFirstRevTaskreqstg   .resize(totalStages,        lgrn::id_null<ReverseTaskReqStageId>());
    out.revTaskreqstgToTask         .resize(totalTasksReqStage, lgrn::id_null<TaskId>());

    // Put spaces

    fill_many(
        out.pipelineToFirstAnystg,
        [&plCounts] (PipelineId pl)                { return plCounts[pl].stages; },
        [&out] (PipelineId pl, AnyStageId claimed) { out.anystgToPipeline[claimed] = pl; });

    fill_many(
        out.anystgToFirstRuntask,
        [&plCounts, &out] (AnyStageId stg)
        {
            PipelineId const    pl          = out.anystgToPipeline[stg];
            if (pl == lgrn::id_null<PipelineId>())
            {
                return uint16_t(0);
            }
            StageId const       stgLocal    = stage_from(out, pl, stg);
            return plCounts[pl].stageCounts[std::size_t(stgLocal)].runTasks;
        },
        [] (AnyStageId, RunTaskId) { });

    // for StageReqTaskId
    fill_many(
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
    fill_many(
        out.taskToFirstRevStgreqtask,
        [&taskCounts] (TaskId task)                         { return taskCounts[task].requiredByStages; },
        [&out] (TaskId, ReverseStageReqTaskId) { });

    // for TaskReqStage
    fill_many(
        out.taskToFirstTaskreqstg,
        [&taskCounts] (TaskId task)                     { return taskCounts[task].requiresStages; },
        [&out] (TaskId task, TaskReqStageId claimed)    { out.taskreqstgData[claimed].ownTask = task; });
    fill_many(
        out.stageToFirstRevTaskreqstg,
        [&plCounts, &out] (AnyStageId stg)
        {
            PipelineId const    pl          = out.anystgToPipeline[stg];
            StageId const       stgLocal    = stage_from(out, pl, stg);
            return plCounts[pl].stageCounts[std::size_t(stgLocal)].requiredByTasks;
        },
        [&out] (AnyStageId, ReverseTaskReqStageId) { });

    // Push

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
        ReverseTaskReqStageId const revTaskReqStgId = id_from_count(out.stageToFirstRevTaskreqstg, anystg, rStageCounts.requiredByTasks);

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
        for (auto const [task, pipeline, stage] : pEdges->m_runOn)
        {
            AnyStageId const anystg         = anystg_from(out, pipeline, stage);
            StageCounts      &rStageCounts  = plCounts[pipeline].stageCounts[std::size_t(stage)];
            TaskCounts       &rTaskCounts   = taskCounts[task];

            RunTaskId const runTask = id_from_count(out.anystgToFirstRuntask, anystg, rStageCounts.runTasks);
            out.runtaskToTask[runTask] = task;

            -- rStageCounts.runTasks;
            -- rTaskCounts.runOn;
            -- totalRunTasks;
        }

        for (auto const [task, pipeline, stage] : pEdges->m_syncWith)
        {
            add_stagereqtask(pipeline, stage, task);
            add_taskreqstage(task, pipeline, stage);
        }

        for (auto const [task, pipeline, stage] : pEdges->m_triggers)
        {
            add_stagereqtask(pipeline, stage_prev(stage, plCounts[pipeline].stages), task);
        }
    }

    [[maybe_unused]] auto const all_counts_zero = [&plCounts, &taskCounts] ()
    {
        for (PipelineCounts const& plCount : plCounts)
        {
            for (StageCounts const& stgCount : plCount.stageCounts)
            {
                if (   stgCount.requiredByTasks != 0
                    || stgCount.requiresTasks   != 0
                    || stgCount.runTasks        != 0)
                {
                    return false;
                }
            }
        }
        for (TaskCounts const& taskCount : taskCounts)
        {
            if (   taskCount.requiredByStages != 0
                || taskCount.requiredByStages != 0
                || taskCount.runOn            != 0)
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

