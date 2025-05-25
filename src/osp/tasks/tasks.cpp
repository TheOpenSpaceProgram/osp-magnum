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

#include <array>

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

void check_task_order(Tasks const& tasks, TaskOrderReport& rOut, lgrn::IdSetStl<LoopBlockId> const& loopblks)
{

    // step 1: add all the tasks

    std::vector<StageLock> locks;

    // add some way to know which stages are connected to which tasks
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

    while (true)
    {
        // consider connected points

        auto const task_aligned = [&] (TaskId const taskId, PipelineId const pipelineId, StageId stageId)
        {
            LGRN_ASSERT(alignsRemaining[taskId] != 0);
            -- alignsRemaining[taskId];
            if (alignsRemaining[taskId] == 0)
            {
                std::cout << "task passed: " << taskId.value << "\n";
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

        bool nothingMoved = true;

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

        if (nothingMoved)
        {
            break;
        }

    }

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
}

} // namespace osp

