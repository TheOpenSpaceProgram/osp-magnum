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
    StageId     stageId;
};

void check_task_order(Tasks const& tasks, TaskOrderReport& rOut, lgrn::IdSetStl<LoopBlockId> loopblks)
{
    std::vector<StageLock> locks;

    osp::KeyedVec<PipelineId, StageId> currentStages;
    currentStages.resize(tasks.pipelineIds.capacity());
    lgrn::IdSetStl<PipelineId> stageJustChanged;
    stageJustChanged.resize(tasks.pipelineIds.capacity());

    osp::KeyedVec<TaskId, std::uint8_t> alignsRemaining;
    alignsRemaining.resize(tasks.taskIds.capacity());

    int pipelinesNotFinished = 0;
    for (PipelineId const pipelineId : tasks.pipelineIds)
    {
        Pipeline const& pipeline = tasks.pipelineInst[pipelineId];
        if (loopblks.contains(pipeline.block))
        {
            ++pipelinesNotFinished;

            currentStages[pipelineId] = pipeline.initialStage;
            stageJustChanged.insert(pipelineId);
            rOut.steps.push_back({.pipelineId = pipelineId, .stageId = pipeline.initialStage, .time = 0});
            if (pipeline.scheduleCondition.has_value())
            {
                ++ alignsRemaining[pipeline.scheduleCondition];
            }
        }
    }


    for (TaskSyncToPipeline const& syncTo : tasks.syncs)
    {
        if (loopblks.contains(tasks.pipelineInst[syncTo.pipeline].block))
        {
            ++ alignsRemaining[syncTo.task];
        }
    }

    int time = 0;

    auto &rPltypeinfo = PipelineTypeIdReg::instance();

    auto const task_aligned = [&] (TaskId const taskId, PipelineId const pipelineId, StageId stageId)
    {
        LGRN_ASSERT(alignsRemaining[taskId] != 0);
        -- alignsRemaining[taskId];
        if (alignsRemaining[taskId] == 0)
        {
            rOut.steps.push_back({.taskId = taskId, .time = time});

            // remove all locks assiciated with this task
            for (StageLock &rLock : locks)
            {
                if (rLock.taskId == taskId)
                {
                    rLock = {};
                }
            }
        }
        else
        {
            locks.push_back({taskId, pipelineId, stageId});
        }
    };

    bool nothingMoved = true;

    do
    {
        // any points connected to current stages?
        for (TaskSyncToPipeline const& syncTo : tasks.syncs)
        {
            if (stageJustChanged.contains(syncTo.pipeline) && currentStages[syncTo.pipeline] == syncTo.stage)
            {
                task_aligned(syncTo.task, syncTo.pipeline, syncTo.stage);
            }
        }
        for (PipelineId const pipelineId : tasks.pipelineIds)
        {
            if (stageJustChanged.contains(pipelineId))
            {
                Pipeline const &inst = tasks.pipelineInst[pipelineId];
                if (inst.scheduleCondition.has_value() && rPltypeinfo.get(inst.type).stages[currentStages[pipelineId]].isSchedule)
                {
                    task_aligned(inst.scheduleCondition, pipelineId, currentStages[pipelineId]);
                }
            }
        }

        if (pipelinesNotFinished == 0) { break; }

        ++ time;

        auto const &locksFirst = locks.begin();
        auto const &locksLast  = locks.end();

        stageJustChanged.clear();

        // move points
        for (PipelineId const pipelineId : tasks.pipelineIds)
        {
            Pipeline const& pipeline = tasks.pipelineInst[pipelineId];

            if ( ! loopblks.contains(pipeline.block) ) { continue; }

            StageId &rCurrentStage = currentStages[pipelineId];

            // stages set to null means it's done
            if ( ! rCurrentStage.has_value() ) { continue; }

            // don't advance stages when a lock exists on it
            auto const findLock = [rCurrentStage, pipelineId] (StageLock const& lock)
            {
                return (lock.pipelineId == pipelineId) && (lock.stageId == rCurrentStage);
            };
            if (std::find_if(locksFirst, locksLast, findLock) != locksLast) { continue; }

            nothingMoved = false;
            rCurrentStage.value++;
            if (rCurrentStage.value == rPltypeinfo.get(pipeline.type).stages.size())
            {
                rCurrentStage.value = 0; // loop around back to zero
            }
            if (rCurrentStage == pipeline.initialStage)
            {
                // looped around
                rCurrentStage = {};
                --pipelinesNotFinished;
            }
            else
            {
                rOut.steps.push_back({.pipelineId = pipelineId, .stageId = rCurrentStage, .time = time});
                stageJustChanged.insert(pipelineId);
            }
        }

    } while (!nothingMoved);

    for (TaskId const taskId : tasks.taskIds)
    {
        if (alignsRemaining[taskId] != 0)
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

    osp::KeyedVec<PipelineId, StageId> stageChanges;

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
                else
                {
                    os << "\U00002502 ";
                }
            }

        }

        os << "\n";

        while(it != last && it->taskId.has_value() && it->time == time)
        {
            for(PipelineId const pipelineId : tasks.pipelineIds)
            {
                if (report.loopblks.contains(tasks.pipelineInst[pipelineId].block))
                {
                    if (tasks.pipelineInst[pipelineId].scheduleCondition == it->taskId)
                    {
                        os << "\U000025C9\U00002500";
                    }
                    else if (std::find_if(
                                    tasks.syncs.begin(), tasks.syncs.end(),
                                    [taskId = it->taskId, pipelineId] (TaskSyncToPipeline const& sync)
                                    { return sync.task == taskId && sync.pipeline == pipelineId; })
                             != tasks.syncs.end())
                    {
                        os << "\U000025CF\U00002500";
                    }
                    else
                    {
                        os << "\U0000253C\U00002500";
                    }
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
                        os << stageIndex << "\U00002500";
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
                            os << "\U000025CF\U00002500";
                        }
                        else
                        {
                            os << int(syncIt->stage.value) << "\U00002500";
                        }
                    }
                    else
                    {
                        os << "\U00002534\U00002500";
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

