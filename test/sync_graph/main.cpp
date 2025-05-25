 /**
 * Open Space Program
 * Copyright © 2019-2025 Open Space Program Project
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

#include "graph_builder.h"

#include <gtest/gtest.h>

#include <osp/executor/sync_graph.h>
#include <osp/executor/singlethread_sync_graph.h>

#include <osp/core/keyed_vector.h>

#include <longeron/id_management/id_set_stl.hpp>

#include <vector>


using namespace test_graph;


testing::AssertionResult is_locked(std::initializer_list<SynchronizerId> locked, SyncGraphExecutor& rExec, std::vector<SynchronizerId> &rJustLocked, SyncGraph const& graph)
{
    for (SynchronizerId const syncId : locked)
    {
        if( ! rExec.is_locked(syncId, graph) )
        {
            return testing::AssertionFailure() << "SynchronizerId=" << int(syncId.value) << " debugName=\"" << graph.syncs[syncId].debugName << "\" is not locked";
        }

        if (std::find(rJustLocked.begin(), rJustLocked.end(), syncId) == rJustLocked.end())
        {
            return testing::AssertionFailure() << "justLocked vector does not contain SynchronizerId=" << syncId.value << " \"" << graph.syncs[syncId].debugName << "\"";
        }
    }
    if(rJustLocked.size() != locked.size())
    {
        return testing::AssertionFailure() << "Excess items in justLocked vector";
    }
    return testing::AssertionSuccess();
}

template <typename ... ID_T>
bool is_all_not_null(ID_T const& ... ids)
{
    return (ids.has_value() && ... );
}

using ESyncAction = SyncGraphExecutor::ESyncAction;

TEST(SyncExec, Basic)
{
    SyncGraph const graph = make_test_graph(
    {
        .types =
        {
            {
                .name = "4PointLoop",
                .points = {"A", "B", "C", "D"},
                .cycles =
                {
                    {
                        .name = "MainCycle",
                        .path = {"A", "B", "C", "D"}
                    }
                },
                .initialCycle = { .cycle = "MainCycle", .position = 0 }
            }
        },
        .subgraphs =
        {
            { .name = "Bulb", .type = "4PointLoop" },
            { .name = "Fish", .type = "4PointLoop" },
            { .name = "Rock", .type = "4PointLoop" }
        },
        .syncs =
        {
            {
                .name = "Sync_0",
                .connections =
                {
                    { .subgraph = "Bulb", .point = "A" },
                    { .subgraph = "Fish", .point = "A" },
                    { .subgraph = "Rock", .point = "B" }
                }
            },
            {
                .name = "Sync_1",
                .connections =
                {
                    { .subgraph = "Bulb", .point = "A" },
                    { .subgraph = "Fish", .point = "B" },
                    { .subgraph = "Rock", .point = "B" }
                }
            },
            {
                .name = "Sync_2",
                .debugGraphStraight = true,
                .connections =
                {
                    { .subgraph = "Bulb", .point = "B" },
                    { .subgraph = "Fish", .point = "B" },
                    { .subgraph = "Rock", .point = "B" }
                }
            },
            {
                .name = "Sync_3",
                .connections =
                {
                    { .subgraph = "Bulb", .point = "D" },
                    { .subgraph = "Fish", .point = "D" },
                    { .subgraph = "Rock", .point = "D" }
                }
            },
            {
                .name = "Sync_4",
                .debugGraphStraight = true,
                .connections =
                {
                    { .subgraph = "Bulb", .point = "D" },
                    { .subgraph = "Fish", .point = "D" },
                    { .subgraph = "Rock", .point = "D" }
                }
            },
        }
    });

    SynchronizerId const sync0Id = find_sync("Sync_0", graph);
    SynchronizerId const sync1Id = find_sync("Sync_1", graph);
    SynchronizerId const sync2Id = find_sync("Sync_2", graph);
    SynchronizerId const sync3Id = find_sync("Sync_3", graph);
    SynchronizerId const sync4Id = find_sync("Sync_4", graph);

    std::vector<SynchronizerId> justLocked;
    SyncGraphExecutor exec;
    exec.load(graph);
    exec.batch(ESyncAction::SetEnable, {sync0Id, sync1Id, sync2Id, sync3Id, sync4Id}, graph);

    while (exec.update(justLocked, graph)) { }

    // Sync 0 locks first
    ASSERT_TRUE(is_locked({sync0Id}, exec, justLocked, graph));
    exec.batch(ESyncAction::Unlock, {sync0Id}, graph);
    justLocked.clear();

    while (exec.update(justLocked, graph)) { }

    // Sync 1 locks
    ASSERT_TRUE(is_locked({sync1Id}, exec, justLocked, graph));
    exec.batch(ESyncAction::Unlock, {sync1Id}, graph);
    justLocked.clear();

    while (exec.update(justLocked, graph)) { }

    // Sync 2 locks
    ASSERT_TRUE(is_locked({sync2Id}, exec, justLocked, graph));
    exec.batch(ESyncAction::Unlock, {sync2Id}, graph);
    justLocked.clear();

    while (exec.update(justLocked, graph)) { }

    // Sync 3 and 4 locks simultaneously
    ASSERT_TRUE(is_locked({sync3Id, sync4Id}, exec, justLocked, graph));
    exec.batch(ESyncAction::Unlock, {sync3Id}, graph);
    exec.batch(ESyncAction::Unlock, {sync4Id}, graph);
    justLocked.clear();

    while (exec.update(justLocked, graph)) { }

    // Loop back to Sync 0
    ASSERT_TRUE(is_locked({sync0Id}, exec, justLocked, graph));
    exec.batch(ESyncAction::Unlock, {sync0Id}, graph);
    justLocked.clear();
}

TEST(SyncExec, ParallelSize1Loop)
{
    SyncGraph const graph = make_test_graph(
    {
        .types =
        {
            {
                .name = "SinglePoint",
                .points = {"TheOnlyPoint"},
                .cycles =
                {
                    {
                        .name = "MainCycle",
                        .path = {"TheOnlyPoint"}
                    }
                },
                .initialCycle = { .cycle = "MainCycle", .position = 0 }
            }
        },
        .subgraphs =
        {
            { .name = "Foo", .type = "SinglePoint" },
            { .name = "Bar", .type = "SinglePoint" },
        },
        .syncs =
        {
            {
                .name = "Sync_0",
                .connections =
                {
                    { .subgraph = "Foo", .point = "TheOnlyPoint" },
                    { .subgraph = "Bar", .point = "TheOnlyPoint" }
                }
            }
        }
    });

    SynchronizerId const syncId = find_sync("Sync_0", graph);
    ASSERT_TRUE(syncId.has_value());

    std::vector<SynchronizerId> justLocked;
    SyncGraphExecutor exec;
    exec.load(graph);
    exec.batch(ESyncAction::SetEnable, {syncId}, graph);

    for(int i = 0; i < 5; ++i)
    {
        // something 'should happen' after first run or after unlock()
        ASSERT_TRUE(exec.update(justLocked, graph));

        // update 'a couple more times' until there's nothing to do
        exec.update(justLocked, graph);
        exec.update(justLocked, graph);
        exec.update(justLocked, graph);

        // Sync_0 should be aligned and locked
        ASSERT_TRUE(is_locked({syncId}, exec, justLocked, graph));
        justLocked.clear();

        exec.batch(ESyncAction::Unlock, {syncId}, graph);
        ASSERT_FALSE(exec.is_locked(syncId, graph));
    }
}

TEST(SyncExec, BranchingPath)
{
    SyncGraph const graph = make_test_graph(
    {
        .types =
        {
            {
                .name = "BranchingPaths",
                .points = {"Common", "A", "B"},
                .cycles =
                {
                    {
                        .name = "Idle",
                        .path = {"Common"}
                    },
                    {
                        .name = "ViaA",
                        .path = {"Common", "A"}
                    },
                    {
                        .name = "ViaB",
                        .path = {"Common", "B"}
                    }
                },
                .initialCycle = { .cycle = "Idle", .position = 0 }
            },
            {
                .name = "3PointLoop",
                .points = {"X", "Y", "Z"},
                .cycles =
                {
                    {
                        .name = "MainCycle",
                        .path = {"X", "Y", "Z"}
                    }
                },
                .initialCycle = { .cycle = "MainCycle", .position = 0 }
            }
        },
        .subgraphs =
        {
            { .name = "BP", .type = "BranchingPaths" },
            { .name = "3PL", .type = "3PointLoop" },
        },
        .syncs =
        {
            {
                .name = "Schedule",
                .connections =
                {
                    { .subgraph = "BP", .point = "Common" },
                    { .subgraph = "3PL", .point = "X" }
                }
            },
            {
                .name = "End of 3PL",
                .connections =
                {
                    { .subgraph = "3PL", .point = "Z" }
                }
            },
            {
                .name = "With A",
                .connections =
                {
                    { .subgraph = "BP", .point = "A" },
                    { .subgraph = "3PL", .point = "Y" }
                }
            },
            {
                .name = "With B",
                .connections =
                {
                    { .subgraph = "BP", .point = "B" },
                    { .subgraph = "3PL", .point = "Y" }
                }
            }
        }
    });

    SubgraphTypeId const branching      = find_sgtype("BranchingPaths", graph);

    LocalCycleId   const branchingViaA  = find_cycle("ViaA", branching, graph);
    LocalCycleId   const branchingViaB  = find_cycle("ViaB", branching, graph);

    SubgraphId     const bp             = find_subgraph("BP", graph);

    SynchronizerId const schedule       = find_sync("Schedule", graph);
    SynchronizerId const eo3pl          = find_sync("End of 3PL", graph);
    SynchronizerId const withA          = find_sync("With A", graph);
    SynchronizerId const withB          = find_sync("With B", graph);

    ASSERT_TRUE(is_all_not_null(schedule, eo3pl, withA, withB, branching, branchingViaA, branchingViaB, bp));

    std::vector<SynchronizerId> justLocked;
    SyncGraphExecutor exec;
    exec.load(graph);

    exec.batch(ESyncAction::SetEnable, {schedule, eo3pl}, graph);

    // initial Idle cycle just repeatedly locks "Schedule" and "End of 3PL" sync
    for (int i = 0; i < 5; ++i)
    {
        while (exec.update(justLocked, graph)) { }

        ASSERT_TRUE(is_locked({schedule}, exec, justLocked, graph));
        exec.batch(ESyncAction::Unlock, {schedule}, graph);
        justLocked.clear();

        while (exec.update(justLocked, graph)) { }

        ASSERT_TRUE(is_locked({eo3pl}, exec, justLocked, graph));
        exec.batch(ESyncAction::Unlock, {eo3pl}, graph);
        justLocked.clear();
    }

    while (exec.update(justLocked, graph)) { }

    // Switch subgraph BranchingPaths's current cycle to ViaA
    ASSERT_TRUE(is_locked({schedule}, exec, justLocked, graph));
    exec.select_cycle(bp, branchingViaA, graph);
    exec.batch(ESyncAction::SetEnable, {withA}, graph);

    for (int i = 0; i < 5; ++i)
    {
        while (exec.update(justLocked, graph)) { }

        ASSERT_TRUE(is_locked({schedule}, exec, justLocked, graph));
        exec.batch(ESyncAction::Unlock, {schedule}, graph);
        justLocked.clear();

        while (exec.update(justLocked, graph)) { }

        ASSERT_TRUE(is_locked({withA}, exec, justLocked, graph));
        exec.batch(ESyncAction::Unlock, {withA}, graph);
        justLocked.clear();

        while (exec.update(justLocked, graph)) { }

        ASSERT_TRUE(is_locked({eo3pl}, exec, justLocked, graph));
        exec.batch(ESyncAction::Unlock, {eo3pl}, graph);
        justLocked.clear();
    }

    while (exec.update(justLocked, graph)) { }

    // Switch subgraph BranchingPaths's current cycle back to MainCycle
    ASSERT_TRUE(is_locked({schedule}, exec, justLocked, graph));
    exec.select_cycle(bp, branchingViaA, graph);
    exec.batch(ESyncAction::SetDisable, {withA}, graph);

    for (int i = 0; i < 5; ++i)
    {
        while (exec.update(justLocked, graph)) { }

        ASSERT_TRUE(is_locked({schedule}, exec, justLocked, graph));
        exec.batch(ESyncAction::Unlock, {schedule}, graph);
        justLocked.clear();

        while (exec.update(justLocked, graph)) { }

        ASSERT_TRUE(is_locked({eo3pl}, exec, justLocked, graph));
        exec.batch(ESyncAction::Unlock, {eo3pl}, graph);
        justLocked.clear();
    }

    while (exec.update(justLocked, graph)) { }

    // Switch subgraph BranchingPaths's current cycle to ViaB
    ASSERT_TRUE(is_locked({schedule}, exec, justLocked, graph));
    exec.select_cycle(bp, branchingViaB, graph);
    exec.batch(ESyncAction::SetEnable, {withB}, graph);

    for (int i = 0; i < 5; ++i)
    {
        while (exec.update(justLocked, graph)) { }

        ASSERT_TRUE(is_locked({schedule}, exec, justLocked, graph));
        exec.batch(ESyncAction::Unlock, {schedule}, graph);
        justLocked.clear();

        while (exec.update(justLocked, graph)) { }

        ASSERT_TRUE(is_locked({withB}, exec, justLocked, graph));
        exec.batch(ESyncAction::Unlock, {withB}, graph);
        justLocked.clear();

        while (exec.update(justLocked, graph)) { }

        ASSERT_TRUE(is_locked({eo3pl}, exec, justLocked, graph));
        exec.batch(ESyncAction::Unlock, {eo3pl}, graph);
        justLocked.clear();
    }
}

//
// Task O0 - Write <Requests>
//
// scheduler Task L0 - check if we need to loop, like `while(hasRequests)`
// {
//     Task L1 - Read <Request>, Write to <Process 0>
//     Task L2 - Read <Process 0>, Write to <Process 1>
//     Task L3 - Read <Process 1>, Write to <Results>
// }
//
// Task O1 - Clear <Requests>
// Task O2 - Read <Results>
// Task O3 - Clear <Results>
//
//
TEST(SyncExec, NestedLoop)
{
    SyncGraph const graph = make_test_graph(
    {
        .types =
        {
            {
                .name = "BlockController",
                .points = {"Start", "Schedule", "Running", "Finish"},
                .cycles = {
                    { .name = "Control",    .path = {"Start", "Schedule", "Finish"} },
                    { .name = "Running",    .path = {"Schedule", "Running"} },
                    { .name = "Canceled",   .path = {"Schedule"} } },
                .initialCycle = { .cycle = "Control", .position = 0 }
            },
            {
                .name = "OSP-Style Intermediate-Value Pipeline",
                .points = {"Start", "Schedule", "Read", "Clear", "Modify", "Finish"},
                .cycles = {
                    { .name = "Control",    .path = {"Start", "Schedule", "Finish"} },
                    { .name = "Running",    .path = {"Schedule", "Read", "Clear", "Modify"} },
                    { .name = "Canceled",   .path = {"Schedule"} } },
                .initialCycle = { .cycle = "Control", .position = 0 }
            }
        },
        .subgraphs =
        {
            { .name = "OuterBlkCtrl",      .type = "BlockController" },
            { .name = "Outer-Request",      .type = "OSP-Style Intermediate-Value Pipeline" },
            { .name = "Outer-Results",      .type = "OSP-Style Intermediate-Value Pipeline" },
            { .name = "InnerBlkCtrl",      .type = "BlockController" },
            { .name = "Inner-Process0",     .type = "OSP-Style Intermediate-Value Pipeline" },
            { .name = "Inner-Process1",     .type = "OSP-Style Intermediate-Value Pipeline" },
        },
        .syncs =
        {
            // Stops the outer loop from running until it's commanded to start externally
            { .name = "syOtrExtStart", .connections = {
                  { .subgraph = "OuterBlkCtrl", .point = "Start" } } },

            { .name = "syOtrSchedule", .connections = {
                  { .subgraph = "OuterBlkCtrl", .point = "Schedule" } } },


            // Sync Start and Finish of OuterBlkCtrl's childen to its Running point.
            // This assures children can only run while OuterBlkCtrl is in its Running state.
            // SchInit 'schedule init' assures that all children start (cycles set) at the same
            // time.
            { .name = "syOtrLCLeft", .debugGraphStraight = true, .connections = {
                    { .subgraph = "OuterBlkCtrl",       .point = "Running" },
                    { .subgraph = "Outer-Request",      .point = "Start" },
                    { .subgraph = "Outer-Results",      .point = "Start" },
                    { .subgraph = "InnerBlkCtrl",       .point = "Start" }      } },
            { .name = "syOtrLCRight", .connections = {
                    { .subgraph = "OuterBlkCtrl",       .point = "Running" },
                    { .subgraph = "Outer-Request",      .point = "Finish" },
                    { .subgraph = "Outer-Results",      .point = "Finish" },
                    { .subgraph = "InnerBlkCtrl",       .point = "Finish" }     } },
            { .name = "syOtrLCSchInit", .connections = {
                    { .subgraph = "Outer-Request",      .point = "Schedule" },
                    { .subgraph = "Outer-Results",      .point = "Schedule" },
                    { .subgraph = "InnerBlkCtrl",       .point = "Schedule" }      } },

            { .name = "syTaskP0S", .connections = {
                    { .subgraph = "Inner-Process0",     .point = "Schedule" }   } },
            { .name = "syTaskP1S", .connections = {
                    { .subgraph = "Inner-Process1",     .point = "Schedule" }   } },

            // Same as above, but for InnerBlkCtrl
            { .name = "syInrSchedule", .connections = {
                  { .subgraph = "InnerBlkCtrl", .point = "Schedule" } } },

            { .name = "syInrLCLeft", .debugGraphStraight = true, .connections = {
                    { .subgraph = "InnerBlkCtrl",       .point = "Running" },
                    { .subgraph = "Inner-Process0",     .point = "Start" },
                    { .subgraph = "Inner-Process1",     .point = "Start" }      } },
            { .name = "syInrLCRight", .connections = {
                    { .subgraph = "InnerBlkCtrl",       .point = "Running" },
                    { .subgraph = "Inner-Process0",     .point = "Finish" },
                    { .subgraph = "Inner-Process1",     .point = "Finish" }     } },

            { .name = "syInrLCSchInit", .connections = {
                    { .subgraph = "Inner-Process0",     .point = "Schedule" },
                    { .subgraph = "Inner-Process1",     .point = "Schedule" }   } },

            { .name = "syTaskO0", .connections = {
                    { .subgraph = "Outer-Request",      .point = "Modify" }     } },

            // no syTaskL0, since no dependencies to anything inside the loop
            //
            // two extra syncs needed.
            // external sync acts normal for the first iteration, but immediately disabled
            // sustainer sync keeps outer block's dependencies locked in place until the inner loop exits.
            { .name = "syTaskL0ext", .connections =  {
                    { .subgraph = "Outer-Request",      .point = "Read" }       } },

            { .name = "syTaskL0sus", .debugGraphLongAndUgly = true, .connections = {
                    { .subgraph = "Outer-Request",      .point = "Read" },
                    { .subgraph = "Inner-Process0",     .point = "Finish" }    } },


            { .name = "syTaskRequestS", .connections = {
                    { .subgraph = "Outer-Request",      .point = "Schedule" }   } },
            { .name = "syTaskResultsS", .connections = {
                    { .subgraph = "Outer-Results",      .point = "Schedule" }   } },

            { .name = "syTaskL1", .connections = {
                    { .subgraph = "Inner-Process0",     .point = "Modify" }     } },
            { .name = "syTaskL1ext", .debugGraphLongAndUgly = true, .connections = {
                    { .subgraph = "Outer-Request",      .point = "Read" },
                    { .subgraph = "Inner-Process0",     .point = "Modify" }     } },
            { .name = "syTaskL1sus", .debugGraphLongAndUgly = true, .connections = {
                    { .subgraph = "Outer-Request",      .point = "Read" },
                    { .subgraph = "Inner-Process0",     .point = "Finish" }     } },

            { .name = "syTaskL2", .connections = {
                    { .subgraph = "Inner-Process0",     .point = "Read" },
                    { .subgraph = "Inner-Process1",     .point = "Modify" }     } },
            { .name = "syTaskL2can", .connections = {
                    { .subgraph = "Inner-Process0",     .point = "Schedule" },
                    { .subgraph = "Inner-Process1",     .point = "Modify" }     } },

            { .name = "syTaskL3", .connections = {
                    { .subgraph = "Inner-Process1",     .point = "Read" }       } },
            { .name = "syTaskL3ext", .debugGraphLongAndUgly = true, .connections = {
                    { .subgraph = "Outer-Results",      .point = "Modify" }     } },
            { .name = "syTaskL3sus", .debugGraphLongAndUgly = true, .connections = {
                    { .subgraph = "Outer-Results",      .point = "Modify" },
                    { .subgraph = "Inner-Process1",     .point = "Finish" }     } },

            { .name = "syTaskO1", .connections = {
                    { .subgraph = "Outer-Request",      .point = "Clear" },     } },
            { .name = "syTaskO2", .connections = {
                    { .subgraph = "Outer-Results",      .point = "Read" }       } },
            { .name = "syTaskO3", .connections = {
                    { .subgraph = "Outer-Results",      .point = "Clear" }      } },

            { .name = "syInrLCCan0", .connections = {
                    { .subgraph = "InnerBlkCtrl",       .point = "Schedule" },
                    { .subgraph = "Outer-Results",      .point = "Clear" }      } },
            { .name = "syInrLCCan1", .connections = {
                    { .subgraph = "InnerBlkCtrl",       .point = "Schedule" },
                    { .subgraph = "Outer-Results",      .point = "Clear" }      } },

        }
    });

    SubgraphTypeId const blkCtrl            = find_sgtype("BlockController",                       graph);
    SubgraphTypeId const ospPipeline        = find_sgtype("OSP-Style Intermediate-Value Pipeline", graph);

    LocalCycleId   const blkCtrlControl     = find_cycle("Control",  ospPipeline, graph);
    LocalCycleId   const blkCtrlRunning     = find_cycle("Running",  ospPipeline, graph);
    LocalCycleId   const blkCtrlCancel      = find_cycle("Canceled", ospPipeline, graph);

    LocalCycleId   const ospPipelineControl = find_cycle("Control",  ospPipeline, graph);
    LocalCycleId   const ospPipelineRunning = find_cycle("Running",  ospPipeline, graph);
    LocalCycleId   const ospPipelineCancel  = find_cycle("Canceled", ospPipeline, graph);

    SubgraphId     const outerBlkCtrl       = find_subgraph("OuterBlkCtrl",   graph);
    SubgraphId     const outerRequests      = find_subgraph("Outer-Request",  graph);
    SubgraphId     const outerResults       = find_subgraph("Outer-Results",  graph);
    SubgraphId     const innerBlkCtrl       = find_subgraph("InnerBlkCtrl",   graph);
    SubgraphId     const innerProcess0      = find_subgraph("Inner-Process0", graph);
    SubgraphId     const innerProcess1      = find_subgraph("Inner-Process1", graph);

    SynchronizerId const syOtrExtStart      = find_sync("syOtrExtStart",    graph);
    SynchronizerId const syOtrSchedule      = find_sync("syOtrSchedule",    graph);
    SynchronizerId const syOtrLCLeft        = find_sync("syOtrLCLeft",      graph);
    SynchronizerId const syOtrLCRight       = find_sync("syOtrLCRight",     graph);
    SynchronizerId const syOtrLCSchInit     = find_sync("syOtrLCSchInit",   graph);
    SynchronizerId const syTaskRequestS     = find_sync("syTaskRequestS",   graph);
    SynchronizerId const syTaskResultsS     = find_sync("syTaskResultsS",   graph);

    SynchronizerId const syInrSchedule      = find_sync("syInrSchedule",    graph);
    SynchronizerId const syInrLCLeft        = find_sync("syInrLCLeft",      graph);
    SynchronizerId const syInrLCRight       = find_sync("syInrLCRight",     graph);
    SynchronizerId const syInrLCSchInit     = find_sync("syInrLCSchInit",   graph);
    SynchronizerId const syTaskO0           = find_sync("syTaskO0",         graph);
    SynchronizerId const syTaskP0S          = find_sync("syTaskP0S",        graph);
    SynchronizerId const syTaskP1S          = find_sync("syTaskP1S",        graph);

    SynchronizerId const syTaskL0ext        = find_sync("syTaskL0ext",      graph);
    SynchronizerId const syTaskL0sus        = find_sync("syTaskL0sus",      graph);
    SynchronizerId const syTaskL1           = find_sync("syTaskL1",         graph);
    SynchronizerId const syTaskL1ext        = find_sync("syTaskL1ext",      graph);
    SynchronizerId const syTaskL1sus        = find_sync("syTaskL1sus",      graph);
    SynchronizerId const syTaskL2           = find_sync("syTaskL2",         graph);
    SynchronizerId const syTaskL2can        = find_sync("syTaskL2can",      graph);
    SynchronizerId const syTaskL3           = find_sync("syTaskL3",         graph);
    SynchronizerId const syTaskL3ext        = find_sync("syTaskL3ext",      graph);
    SynchronizerId const syTaskL3sus        = find_sync("syTaskL3sus",      graph);
    SynchronizerId const syTaskO1           = find_sync("syTaskO1",         graph);
    SynchronizerId const syTaskO2           = find_sync("syTaskO2",         graph);
    SynchronizerId const syTaskO3           = find_sync("syTaskO3",         graph);

    SynchronizerId const syInrLCCan0        = find_sync("syInrLCCan0",      graph);
    SynchronizerId const syInrLCCan1        = find_sync("syInrLCCan1",      graph);

    ASSERT_TRUE(is_all_not_null(
            blkCtrl, ospPipeline, blkCtrlControl, blkCtrlRunning, blkCtrlCancel, ospPipelineControl,
            ospPipelineRunning, ospPipelineCancel, outerBlkCtrl, outerRequests, outerResults,
            innerBlkCtrl, innerProcess0, innerProcess1, syOtrExtStart, syOtrSchedule, syOtrLCLeft, syOtrLCRight,
            syOtrLCSchInit, syTaskRequestS, syTaskResultsS, syInrSchedule, syInrLCLeft, syInrLCRight,
            syInrLCSchInit, syTaskO0, syTaskP0S, syTaskP1S, syTaskL0ext, syTaskL0sus,
            syTaskL1, syTaskL1ext, syTaskL1sus, syTaskL2, syTaskL2can, syTaskL3, syTaskL3ext,
            syTaskL3sus, syTaskO1, syTaskO2, syTaskO3));

    std::vector<SynchronizerId> justLocked;
    SyncGraphExecutor exec;
    exec.load(graph);

    exec.batch(ESyncAction::SetEnable, {
            syOtrExtStart, syOtrSchedule,
            syOtrLCLeft, syOtrLCRight, syOtrLCSchInit,
            syInrSchedule, syInrLCLeft, syInrLCRight, syInrLCSchInit,
            syTaskRequestS, syTaskResultsS,
            syTaskP0S, syTaskP1S,
            syTaskO0,
            syTaskL0ext, syTaskL0sus, syTaskL1, syTaskL1ext, syTaskL1sus, syTaskL2can, syTaskL3ext, syTaskL3sus,
            syInrLCCan0, syInrLCCan1}, graph);

    while (exec.update(justLocked, graph)) { }

    ASSERT_TRUE(is_locked({syOtrExtStart}, exec, justLocked, graph));
    exec.batch(ESyncAction::Unlock, {syOtrExtStart}, graph);
    justLocked.clear();

    while (exec.update(justLocked, graph)) { }
    ASSERT_TRUE(is_locked({syOtrSchedule}, exec, justLocked, graph));

    // Schedule outer loop. set OuterBlkControl cycle Control->Running
    exec.select_cycle(outerBlkCtrl, blkCtrlRunning, graph);
    exec.batch(ESyncAction::Unlock, {syOtrSchedule}, graph);
    justLocked.clear();

    // Outer block starts. SYN_OuterBlkCtrl-Left
    while (exec.update(justLocked, graph)) { }
    ASSERT_TRUE(is_locked({syOtrLCLeft}, exec, justLocked, graph));

    exec.batch(ESyncAction::Unlock, {syOtrLCLeft}, graph);
    justLocked.clear();

    // 'schedule init' assures that all children start (cycles set) at the same time by
    // aligning all the schedule stages.
    // lots of schedules run at the same time
    while (exec.update(justLocked, graph)) { }
    ASSERT_TRUE(is_locked({syInrSchedule, syOtrLCSchInit, syTaskRequestS, syTaskResultsS}, exec, justLocked, graph));


    exec.select_cycle(innerBlkCtrl, blkCtrlRunning, graph);
    exec.select_cycle(outerRequests, ospPipelineRunning, graph);
    exec.select_cycle(outerResults,  ospPipelineRunning, graph);
    exec.batch(ESyncAction::SetDisable, {syOtrLCSchInit}, graph);
    exec.batch(ESyncAction::SetEnable, {syTaskO1}, graph);
    exec.batch(ESyncAction::Unlock, {syInrSchedule, syTaskRequestS, syTaskResultsS}, graph);
    justLocked.clear();

    while (exec.update(justLocked, graph)) { }
    ASSERT_TRUE(is_locked({syInrLCCan0, syInrLCCan1, syTaskL0ext}, exec, justLocked, graph));

    exec.batch(ESyncAction::Unlock, {syInrLCCan0, syInrLCCan1}, graph);
    justLocked.clear();
    // don't disable syTaskL0ext yet

    while (exec.update(justLocked, graph)) { }
    ASSERT_TRUE(is_locked({syInrLCLeft, syTaskL3ext}, exec, justLocked, graph));

    // don't unlock syInrLCLeft yet
    exec.batch(ESyncAction::SetDisable, {syTaskL3ext}, graph);
    exec.batch(ESyncAction::SetDisable, {syTaskL0ext}, graph);
    justLocked.clear();

    // nothing left to do except unlock the inner loop
    while (exec.update(justLocked, graph)) { }
    ASSERT_TRUE(is_locked({}, exec, justLocked, graph));

    // now start the inner loop. unlock syInrLCLeft, locked but never unlocked previously.
    exec.batch(ESyncAction::Unlock, {syInrLCLeft}, graph);
    // no justLocked.clear();

    // schedule init for inner loop
    while (exec.update(justLocked, graph)) { }
    ASSERT_TRUE(is_locked({syInrLCSchInit, syTaskP0S, syTaskP1S}, exec, justLocked, graph));

    exec.select_cycle(innerProcess0,  ospPipelineRunning, graph);
    exec.select_cycle(innerProcess1,  ospPipelineRunning, graph);
    exec.batch(ESyncAction::SetDisable, {syInrLCSchInit}, graph);
    exec.batch(ESyncAction::Unlock, {syTaskP0S, syTaskP1S}, graph);
    justLocked.clear();

    // First inner loop iteration requires enabling tasks and disabling ext syncs

    // Process1->Modify to sync L2can
    while (exec.update(justLocked, graph)) { }
    ASSERT_TRUE(is_locked({syTaskL2can}, exec, justLocked, graph));

    exec.batch(ESyncAction::Unlock, {syTaskL2can}, graph);
    justLocked.clear();

    // Process0->Modify to sync L1 and L1ext, Process1->Schedule to sync P1S
    while (exec.update(justLocked, graph)) { }
    ASSERT_TRUE(is_locked({syTaskL1, syTaskL1ext, syTaskP1S}, exec, justLocked, graph));

    // unlock L1, unlock P1S
    // external syncs (L1ext) must be disabled right away after locking
    exec.batch(ESyncAction::Unlock, {syTaskL1, syTaskP1S}, graph);
    exec.batch(ESyncAction::SetDisable, {syTaskL1ext}, graph);
    justLocked.clear();

    // Process0->Schedule to sync P0S, Process1->Modify to sync L2can
    while (exec.update(justLocked, graph)) { }
    ASSERT_TRUE(is_locked({syTaskP0S, syTaskL2can}, exec, justLocked, graph));

    // unlock P0S, unlock L2can
    // enable task L2, since we're on Process0 schedule
    exec.batch(ESyncAction::SetEnable, {syTaskL2}, graph);
    exec.batch(ESyncAction::Unlock, {syTaskP0S, syTaskL2can}, graph);
    justLocked.clear();

    // Process0->Read to sync with L2
    while (exec.update(justLocked, graph)) { }
    ASSERT_TRUE(is_locked({syTaskL2}, exec, justLocked, graph));

    // unlock L2
    exec.batch(ESyncAction::Unlock, {syTaskL2}, graph);
    justLocked.clear();

    // Process0->Modify to sync L1, P1->schedule to sync P1s
    while (exec.update(justLocked, graph)) { }
    ASSERT_TRUE(is_locked({syTaskP1S, syTaskL1}, exec, justLocked, graph));

    // enable L3
    exec.batch(ESyncAction::SetEnable, {syTaskL3}, graph);

    // Inner loop 2nd iteration and onwards
    for (int i = 0; i < 20; ++i)
    {
        // unlock L1, unlock P1S
        exec.batch(ESyncAction::Unlock, {syTaskL1, syTaskP1S}, graph);
        justLocked.clear();

        // P0->Schedule to sync P0S, P1->Read to sync L3
        while (exec.update(justLocked, graph)) { }
        ASSERT_TRUE(is_locked({syTaskP0S, syTaskL3}, exec, justLocked, graph));

        // unlock P0S, unlock L3
        exec.batch(ESyncAction::Unlock, {syTaskP0S, syTaskL3}, graph);
        justLocked.clear();

        // P1->Modify to sync L2can/L2
        while (exec.update(justLocked, graph)) { }
        ASSERT_TRUE(is_locked({syTaskL2can}, exec, justLocked, graph));

        // unlock L2can
        exec.batch(ESyncAction::Unlock, {syTaskL2can}, graph);
        justLocked.clear();

        // P0->Read to sync L2
        while (exec.update(justLocked, graph)) { }
        ASSERT_TRUE(is_locked({syTaskL2}, exec, justLocked, graph));

        // unlock L2
        exec.batch(ESyncAction::Unlock, {syTaskL2}, graph);
        justLocked.clear();

        // P0->Modify to sync L1, P1->schedule to sync P1s
        while (exec.update(justLocked, graph)) { }
        ASSERT_TRUE(is_locked({syTaskL1, syTaskP1S}, exec, justLocked, graph));
    }

    // don't unlock P1S yet
    exec.batch(ESyncAction::Unlock, {syTaskL1}, graph);
    justLocked.clear();

    while (exec.update(justLocked, graph)) { }
    ASSERT_TRUE(is_locked({syTaskP0S}, exec, justLocked, graph));

    // First step to exit loop, cancel all pipelines
    exec.select_cycle(innerProcess0, ospPipelineCancel, graph);
    exec.select_cycle(innerProcess1, ospPipelineCancel, graph);
    exec.batch(ESyncAction::SetDisable, {syTaskL2can, syTaskL3ext}, graph);
    exec.batch(ESyncAction::Unlock, {syTaskP0S, syTaskP1S}, graph);
    justLocked.clear();

    // both pipelines in the loop are canceled, and only run schedule tasks (P0S and P1S)
    for (int i = 0; i < 5; ++i)
    {
        while (exec.update(justLocked, graph)) { }
        ASSERT_TRUE(is_locked({syTaskP0S, syTaskP1S}, exec, justLocked, graph));
        exec.batch(ESyncAction::Unlock, {syTaskP0S, syTaskP1S}, graph);
        justLocked.clear();
    }

    // To finally exit loop, set all pipelines to control, so they can go to their "Finish" point
    exec.select_cycle(innerProcess0, ospPipelineControl, graph);
    exec.select_cycle(innerProcess1, ospPipelineControl, graph);
    exec.batch(ESyncAction::SetDisable, {syTaskL2can, syTaskL3ext}, graph);

    while (exec.update(justLocked, graph)) { }
    ASSERT_TRUE(is_locked({syTaskL0sus, syTaskL1sus, syTaskL3sus, syInrLCRight}, exec, justLocked, graph));
    exec.batch(ESyncAction::Unlock, {syTaskL0sus, syTaskL1sus, syTaskL3sus, syInrLCRight}, graph);
    justLocked.clear();

    while (exec.update(justLocked, graph)) { }
    ASSERT_TRUE(is_locked({syInrSchedule, syTaskResultsS, syTaskO1}, exec, justLocked, graph));

    // cancel inner loop block control.
    exec.select_cycle(innerBlkCtrl, blkCtrlCancel, graph);
    exec.batch(ESyncAction::SetDisable, {syInrLCLeft, syInrLCRight, syTaskP0S, syTaskP1S, syTaskL0sus, syTaskL0ext, syTaskL1, syTaskL1sus, syTaskL1ext, syTaskL2, syTaskL3, syTaskL3sus, syTaskL3ext}, graph);

    exec.batch(ESyncAction::Unlock, {syInrSchedule, syTaskResultsS, syTaskO1}, graph);
    // pretend that inner loop has written data into 'results'.
    // result schedule task should detect this, and enable syTaskO2, syTaskO3
    exec.batch(ESyncAction::SetEnable, {syTaskO2, syTaskO3}, graph);
    justLocked.clear();

    while (exec.update(justLocked, graph)) { }
    ASSERT_TRUE(is_locked({syTaskO0, syTaskO2}, exec, justLocked, graph));
    exec.batch(ESyncAction::Unlock, {syTaskO0, syTaskO2}, graph);

    justLocked.clear();

    while (exec.update(justLocked, graph)) { }
    ASSERT_TRUE(is_locked({syTaskRequestS, syTaskO3, syInrLCCan0, syInrLCCan1}, exec, justLocked, graph));
    exec.batch(ESyncAction::Unlock, {syTaskRequestS, syTaskO3, syInrLCCan0, syInrLCCan1}, graph);
    justLocked.clear();

    while (exec.update(justLocked, graph)) { }
    ASSERT_TRUE(is_locked({syTaskResultsS, syTaskO1, syInrSchedule}, exec, justLocked, graph));

    exec.batch(ESyncAction::SetDisable, {syTaskO2, syTaskO3}, graph);
    exec.batch(ESyncAction::Unlock, {syTaskO1, syInrSchedule}, graph);
    justLocked.clear();

    while (exec.update(justLocked, graph)) { }
    ASSERT_TRUE(is_locked({syTaskO0}, exec, justLocked, graph));

    exec.batch(ESyncAction::Unlock, {syTaskO0}, graph);
    justLocked.clear();

    while (exec.update(justLocked, graph)) { }
    ASSERT_TRUE(is_locked({syTaskRequestS}, exec, justLocked, graph));

    // we can exit the outer loop, but by now I'm too lazy to write more test code.
}

