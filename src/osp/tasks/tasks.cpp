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

#include <longeron/id_management/id_set_stl.hpp>

#include <sstream>

namespace osp
{

PipelineTypeIdReg& PipelineTypeIdReg::instance()
{
    static PipelineTypeIdReg instance;
    return instance;
}

struct StageLock
{
    TaskId      taskId;
    PipelineId  pipelineId;
};

void check_task_order(Tasks const& tasks, TaskOrderReport& rOut, lgrn::IdSetStl<LoopBlockId> loopblks)
{
    auto const &rPltypeinfo = PipelineTypeIdReg::instance();

    std::vector<StageLock> locks;

    osp::KeyedVec<PipelineId, std::uint8_t> lockCountOf;
    osp::KeyedVec<PipelineId, StageId>      prevStageOf;
    osp::KeyedVec<PipelineId, StageId>      currentStageOf;
    lgrn::IdSetStl<PipelineId>              stageJustChanged;
    osp::KeyedVec<TaskId, std::uint8_t>     remainingSyncPointsOf;
    lgrn::IdSetStl<TaskId>                  tasksRemaining;

    rOut.taskCountOf        .resize(tasks.pipelineIds.capacity(), 0);
    lockCountOf             .resize(tasks.pipelineIds.capacity(), 0);
    prevStageOf             .resize(tasks.pipelineIds.capacity(), {});
    currentStageOf          .resize(tasks.pipelineIds.capacity(), {});
    stageJustChanged        .resize(tasks.pipelineIds.capacity());
    rOut.syncCountOf        .resize(tasks.taskIds.capacity(), 0);
    remainingSyncPointsOf   .resize(tasks.taskIds.capacity(), 0);
    tasksRemaining          .resize(tasks.taskIds.capacity());

    int pipelinesNotFinished = 0;
    for (PipelineId const pipelineId : tasks.pipelineIds)
    {
        Pipeline const& pipeline = tasks.pipelineInst[pipelineId];
        if (loopblks.contains(pipeline.block))
        {
            ++pipelinesNotFinished;

            currentStageOf[pipelineId] = pipeline.initialStage;
            stageJustChanged.insert(pipelineId);
            if (pipeline.scheduleCondition.has_value())
            {
                // Schedule sync is not listed in tasks.syncs
                ++ remainingSyncPointsOf[pipeline.scheduleCondition];
                ++ rOut.taskCountOf[pipelineId];
                ++ rOut.syncCountOf[pipeline.scheduleCondition];
                tasksRemaining.insert(pipeline.scheduleCondition);
            }
        }
    }

    for (TaskSyncToPipeline const& syncTo : tasks.syncs)
    {
        if (loopblks.contains(tasks.pipelineInst[syncTo.pipeline].block))
        {
            ++ remainingSyncPointsOf[syncTo.task];
            ++ rOut.taskCountOf[syncTo.pipeline];
            ++ rOut.syncCountOf[syncTo.task];
            tasksRemaining.insert(syncTo.task);
        }
    }

    int time = 0;
    bool somethingHappened = true;
    while (somethingHappened)
    {
        somethingHappened = false;

        // Keep trying to advance each pipeline's stages until everything is locked and stops moving
        bool anyPipelineAdvanced = true;
        while (anyPipelineAdvanced)
        {
            anyPipelineAdvanced = false;

            // For stages that have just advanced, lock them if they hit any syncs
            for (TaskSyncToPipeline const& syncTo : tasks.syncs)
            {
                if (stageJustChanged.contains(syncTo.pipeline) && currentStageOf[syncTo.pipeline] == syncTo.stage)
                {
                    ++lockCountOf[syncTo.pipeline];
                    --remainingSyncPointsOf[syncTo.task];
                    locks.push_back({syncTo.task, syncTo.pipeline});
                }
            }

            // Schedule task is not part of tasks.syncs, handle it separately
            for (PipelineId const pipelineId : stageJustChanged)
            {
                Pipeline const &pipeline = tasks.pipelineInst[pipelineId];
                if (   pipeline.scheduleCondition.has_value()
                    && currentStageOf[pipelineId].has_value()
                    && rPltypeinfo.get(pipeline.type).stages[currentStageOf[pipelineId]].isSchedule)
                {
                    ++lockCountOf[pipelineId];
                    --remainingSyncPointsOf[pipeline.scheduleCondition];
                    locks.push_back({pipeline.scheduleCondition, pipelineId});
                }
            }

            stageJustChanged.clear();

            // Try to advance all pipeline's current stages forward
            for (PipelineId const pipelineId : tasks.pipelineIds)
            {
                Pipeline  const &pipeline       = tasks.pipelineInst[pipelineId];
                StageId         &rCurrentStage  = currentStageOf[pipelineId];

                if (   rCurrentStage.has_value()          // Is not finished yet?
                    && lockCountOf[pipelineId] == 0       // Is not locked?
                    && loopblks.contains(pipeline.block)) // Is part of selected loopblock?
                {
                    anyPipelineAdvanced = true;
                    somethingHappened   = true;
                    stageJustChanged.insert(pipelineId);

                    rCurrentStage.value++;
                    if (rCurrentStage.value == rPltypeinfo.get(pipeline.type).stages.size())
                    {
                        rCurrentStage.value = 0; // Loop around back to zero
                    }
                    if (rCurrentStage == pipeline.initialStage)
                    {
                        rCurrentStage = {};     // Finished. (Looped around back to initial stage)
                        --pipelinesNotFinished;
                    }
                }
            }
        }

        // Write changed stages to steps
        for (PipelineId const pipelineId : tasks.pipelineIds)
        {
            if (prevStageOf[pipelineId] != currentStageOf[pipelineId])
            {
                prevStageOf[pipelineId] = currentStageOf[pipelineId];
                rOut.steps.push_back({.pipelineId = pipelineId, .stageId = currentStageOf[pipelineId], .time = time});
            }
        }

        // Remove locks on pipelines for completed tasks.
        // Erase from 'locks' and add to rOut.steps at the same time.
        locks.erase(std::remove_if(locks.begin(), locks.end(),
                [&remainingSyncPointsOf, &lockCountOf, &tasksRemaining, &rOut, &somethingHappened, time] (StageLock const& lock) -> bool
        {
            if (remainingSyncPointsOf[lock.taskId] == 0) // task sync fully aligned?
            {
                somethingHappened = true;
                --lockCountOf[lock.pipelineId];

                if (tasksRemaining.contains(lock.taskId))
                {
                    tasksRemaining.erase(lock.taskId);
                    rOut.steps.push_back({.taskId = lock.taskId, .time = time});
                }
                return true; // remove lock
            }
            return false;
        }), locks.end());

        ++ time;
    }

    for (TaskId const taskId : tasks.taskIds)
    {
        if (remainingSyncPointsOf[taskId] != 0)
        {
            if (std::find_if(locks.begin(), locks.end(),
                             [taskId] (StageLock const& lock) { return lock.taskId == taskId; })
                != locks.end())
            {
                rOut.failedLocked.push_back(taskId);
            }
            else
            {
                rOut.failedNotAdded.push_back(taskId);
            }
        }
    }

    rOut.loopblks = std::move(loopblks);
}

std::string visualize_task_order(TaskOrderReport const& report, Tasks const& tasks)
{
    std::ostringstream os;
    auto        it    = report.steps.begin();
    auto const  &last = report.steps.end();
    int         time  = 0;

    // Print pipeline vertical line labels
    int count = 0;
    for(PipelineId const pipelineId : tasks.pipelineIds)
    {
        Pipeline const& pipeline = tasks.pipelineInst[pipelineId];
        if (report.loopblks.contains(pipeline.block))
        {
            for (int i = 0; i < count; ++i)
            {
                os << "\U00002502 ";
            }

            os << "\U0000250D" << pipeline.name << "\n";
            ++count;
        }
    }

    osp::KeyedVec<PipelineId, StageId>      stageChanges;
    osp::KeyedVec<PipelineId, unsigned int> taskIndexOf;
    osp::KeyedVec<TaskId, unsigned int>     syncIndexOf;

    taskIndexOf .resize (tasks.pipelineIds.capacity(), 0);
    //syncIndexOf .resize (tasks.taskIds.capacity(), 0);
    stageChanges.reserve(tasks.pipelineIds.capacity());

    unsigned int syncIndex = 0;

    auto const has_vertical_line = [&taskIndexOf, &report] (PipelineId pl) { return (0 < taskIndexOf[pl]) && (taskIndexOf[pl] < report.taskCountOf[pl]); };
    auto const has_horizontal_line = [&syncIndex, &report] (TaskId taskId) { return (0 < syncIndex) && (syncIndex < report.syncCountOf[taskId]); };

    while(it != last)
    {
        stageChanges.clear();
        stageChanges.resize(tasks.pipelineIds.capacity());
        while(it != last && it->pipelineId.has_value() && it->time == time)
        {
            stageChanges[it->pipelineId] = it->stageId;
            ++it;
        }

        for(PipelineId const pipelineId : tasks.pipelineIds)
        {
            if (report.loopblks.contains(tasks.pipelineInst[pipelineId].block))
            {
                StageId const stageChange = stageChanges[pipelineId];
                if (stageChange.has_value())
                {
                    os << int(stageChange.value) << " ";
                }
                else if (has_vertical_line(pipelineId))
                {
                    os << "\U00002502 ";
                }
                else
                {
                    os << "  ";
                }
            }
        }

        os << "\n";

        while(it != last && it->taskId.has_value() && it->time == time)
        {
            // Draw horizontal row with a task name on the right

            syncIndex = 0;
            for(PipelineId const pipelineId : tasks.pipelineIds)
            {
                if (report.loopblks.contains(tasks.pipelineInst[pipelineId].block))
                {
                    if (tasks.pipelineInst[pipelineId].scheduleCondition == it->taskId)
                    {
                        os << "\U000025C9"; // fisheye for schedule tasks
                        ++ taskIndexOf[pipelineId];
                        ++ syncIndex;
                    }
                    else if (std::find_if(
                                    tasks.syncs.begin(), tasks.syncs.end(),
                                    [taskId = it->taskId, pipelineId] (TaskSyncToPipeline const& sync)
                                    { return sync.task == taskId && sync.pipeline == pipelineId; })
                             != tasks.syncs.end())
                    {
                        os << "\U000025CF"; // black circle
                        ++ taskIndexOf[pipelineId];
                        ++ syncIndex;
                    }
                    else if (has_vertical_line(pipelineId))
                    {
                        // vertical line, or crossed horizontal+vertical line
                        os << (has_horizontal_line(it->taskId) ? "\U0000253C" : "\U00002502");
                    }
                    else
                    {
                        os << (has_horizontal_line(it->taskId) ? "\U00002500" : " ");
                    }

                    os << (has_horizontal_line(it->taskId) ? "\U00002500" : " ");
                }
            }
            os << "" <<  tasks.taskInst[it->taskId].debugName << "\n";
            ++it;
        }

        ++time;
    }

    bool noError = report.failedLocked.empty() && report.failedNotAdded.empty();

    if ( ! noError )
    {
        auto  const &pltypereg  = PipelineTypeIdReg::instance();

        os << "Deadlocked tasks:\n";
        for (TaskId const taskId : report.failedLocked)
        {
            for(PipelineId const pipelineId : tasks.pipelineIds)
            {
                if (report.loopblks.contains(tasks.pipelineInst[pipelineId].block))
                {
                    Pipeline const& pipeline = tasks.pipelineInst[pipelineId];
                    if (pipeline.scheduleCondition == taskId)
                    {
                        auto const &pltype = pltypereg.get(pipeline.type);
                        auto stageIndex = std::distance(
                                std::find_if(pltype.stages.begin(), pltype.stages.end(),
                                             [] (PipelineTypeInfo::StageInfo const &stage) { return stage.isSchedule; } ),
                                pltype.stages.end());
                        os << stageIndex << " ";
                    }
                    else if (auto const syncIt = std::find_if(
                                    tasks.syncs.begin(), tasks.syncs.end(),
                                    [taskId, pipelineId] (TaskSyncToPipeline const& sync)
                                    { return sync.task == taskId && sync.pipeline == pipelineId; });
                            syncIt != tasks.syncs.end())
                    {
                        StageId const lastStage = std::find_if(
                                report.steps.rbegin(), report.steps.rend(),
                                [pipelineId] (TaskOrderReport::Step const& step)
                                { return step.pipelineId == pipelineId; })->stageId;
                        if (lastStage == syncIt->stage)
                        {
                            os << "\U000025CF ";
                        }
                        else
                        {
                            os << int(syncIt->stage.value) << " ";
                        }
                    }
                    else
                    {
                        os << "  ";
                    }
                }
            }
            os << tasks.taskInst[taskId].debugName << "\n";
        }

        os << "Not reached:\n";

        for (TaskId const taskId : report.failedNotAdded)
        {
            for(PipelineId const pipelineId : tasks.pipelineIds)
            {
                if (report.loopblks.contains(tasks.pipelineInst[pipelineId].block))
                {
                    Pipeline const& pipeline = tasks.pipelineInst[pipelineId];
                    if (pipeline.scheduleCondition == taskId)
                    {
                        auto const &pltype = pltypereg.get(pipeline.type);
                        auto stageIndex = std::distance(
                                std::find_if(pltype.stages.begin(), pltype.stages.end(),
                                             [] (PipelineTypeInfo::StageInfo const &stage) { return stage.isSchedule; } ),
                                pltype.stages.end());
                        os << stageIndex << "\U00002500";
                    }
                    else if (auto const syncIt = std::find_if(
                                    tasks.syncs.begin(), tasks.syncs.end(),
                                    [taskId, pipelineId] (TaskSyncToPipeline const& sync)
                                    { return sync.task == taskId && sync.pipeline == pipelineId; });
                            syncIt != tasks.syncs.end())
                    {
                        os << int(syncIt->stage.value) << "\U00002500";
                    }
                    else
                    {
                        os << "\U00002534\U00002500";
                    }
                }
            }
            os << "" <<  tasks.taskInst[taskId].debugName << "\n";
        }

    }

    return os.str();
}

} // namespace osp

