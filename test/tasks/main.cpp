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
#include <osp/tasks/tasks.h>
#include <osp/tasks/builder.h>
#include <osp/tasks/execute.h>

#include <gtest/gtest.h>

#include <functional>
#include <random>
#include <set>

using namespace osp;

template <typename RANGE_T, typename VALUE_T>
bool contains(RANGE_T const& range, VALUE_T const& value) noexcept
{
    for (auto const& element : range)
    {
        if (element == value)
        {
            return true;
        }
    }
    return false;
}

template<typename RUN_TASK_T>
void randomized_singlethreaded_execute(Tasks const& tasks, ExecGraph const& graph, ExecContext& rExec, std::mt19937 &rRand, int maxRuns, RUN_TASK_T && runTask)
{
    std::size_t tasksLeft = rExec.m_tasksQueued.count();

    for (int i = 0; i < maxRuns; ++i)
    {
        if (tasksLeft == 0)
        {
            break;
        }

        // This solution of "pick random '1' bit in a bit vector" is very inefficient
        auto const randomTask = TaskId(*std::next(rExec.m_tasksQueued.ones().begin(), rRand() % tasksLeft));

        FulfillDirty_t const status = runTask(randomTask);
        mark_completed_task(tasks, graph, rExec, randomTask, status);
        int const newTasks = enqueue_dirty(tasks, graph, rExec);

        tasksLeft = tasksLeft - 1 + newTasks;
    }
}

//-----------------------------------------------------------------------------

// Test multiple tasks fulfilling a single target
TEST(Tasks, BasicParallelSingleThreaded)
{
    // NOTE
    // If this was multithreaded, then multiple threads writing to a single container is a bad
    // idea. The proper way to do this is to make a vector per-thread. Targets are still
    // well-suited for this problem, as these per-thread vectors can all be represented with the
    // same TargetId.

    using BasicTraits_t     = BasicBuilderTraits<FulfillDirty_t(*)(int const, std::vector<int>&)>;
    using Builder_t         = BasicTraits_t::Builder;
    using TaskFuncVec_t     = BasicTraits_t::FuncVec_t;

    constexpr int sc_repetitions = 32;
    constexpr int sc_pusherTaskCount = 1337;
    constexpr int sc_totalTaskCount = sc_pusherTaskCount + 1;
    std::mt19937 randGen(69);

    Tasks           tasks;
    TaskEdges       edges;
    TaskFuncVec_t   functions;
    Builder_t       builder{tasks, edges, functions};
    auto const
    [
        inputIn,    /// Input int, manually set dirty when it changes
        vecClear,   /// Vector must be cleared before new stuff can be added to it
        vecReady    /// Vector is ready, all ints have been added to it

    ] = builder.create_targets<3>();

    // Clear vector before use
    builder.task()
            .depends_on({inputIn})
            .fulfills({vecClear})
            .func( [] (int const in, std::vector<int>& rOut) -> FulfillDirty_t
    {
        rOut.clear();
        return {{0b01}};
    });

    // Multiple tasks push to the vector
    for (int i = 0; i < sc_pusherTaskCount; ++i)
    {
        builder.task()
                .depends_on({vecClear})
                .fulfills({vecReady})
                .func( [] (int const in, std::vector<int>& rOut) -> FulfillDirty_t
        {
            rOut.push_back(in);
            return {{0b01}};
        });
    }

    ExecGraph const graph = make_exec_graph(tasks, {&edges});
    ExecContext    exec;

    exec.resize(tasks);

    int                 input = 0;
    std::vector<int>    output;

    // Repeat (with randomness) to test many possible execution orders
    for (int i = 0; i < sc_repetitions; ++i)
    {
        input = 1 + randGen() % 30;

        // Enqueue initial tasks
        // This roughly indicates "Time has changed" and "Render requested"
        exec.m_targetDirty.set(std::size_t(inputIn));
        enqueue_dirty(tasks, graph, exec);

        randomized_singlethreaded_execute(tasks, graph, exec, randGen, sc_totalTaskCount, [&functions, &input, &output] (TaskId const task) -> FulfillDirty_t
        {
            return functions[task](input, output);
        });

        int const sum = std::accumulate(output.begin(), output.end(), 0);

        ASSERT_EQ(sum, input * sc_pusherTaskCount);
    }
}

//-----------------------------------------------------------------------------

struct TestWorld
{
    int m_deltaTimeIn{1};
    int m_forces{0};
    int m_positions{0};
    std::set<std::string> m_canvas;
};

struct TestWorldTargets
{
    TargetId timeIn;            /// External time input, manually set dirty when time 'changes', and the world needs to update
    TargetId forces;            /// Forces need to be calculated before physics
    TargetId positions;         /// Positions calculated by physics task
    TargetId renderRequestIn;   /// External render request, manually set dirty when a new frame to render is required
    TargetId renderDoneOut;     /// Fired when all rendering is finished
};

// Single-threaded test against TestWorld with order-dependent tasks
TEST(Tasks, BasicSingleThreaded)
{
    using BasicTraits_t     = BasicBuilderTraits<FulfillDirty_t(*)(TestWorld&)>;
    using Builder_t         = BasicTraits_t::Builder;
    using TaskFuncVec_t     = BasicTraits_t::FuncVec_t;

    constexpr int sc_repetitions = 32;
    std::mt19937 randGen(69);

    Tasks           tasks;
    TaskEdges       edges;
    TaskFuncVec_t   functions;
    Builder_t       builder{tasks, edges, functions};

    auto const tgt  = builder.create_targets<TestWorldTargets>();

    // Start adding tasks. The order these are added does not matter.

    // Two tasks calculate forces needed by the physics update
    TaskId const taskA = builder.task()
            .depends_on({tgt.timeIn})
            .fulfills({tgt.forces})
            .func( [] (TestWorld& rWorld) -> FulfillDirty_t
    {
        rWorld.m_forces += 42 * rWorld.m_deltaTimeIn;
        return {{0b01}};
    });
    TaskId const taskB = builder.task()
            .depends_on({tgt.timeIn})
            .fulfills({tgt.forces})
            .func([] (TestWorld& rWorld) -> FulfillDirty_t
    {
        rWorld.m_forces += 1337 * rWorld.m_deltaTimeIn;
        return {{0b01}};
    });

    // Main Physics update
    TaskId const taskC = builder.task()
            .depends_on({tgt.timeIn, tgt.forces})
            .fulfills({tgt.positions})
            .func([] (TestWorld& rWorld) -> FulfillDirty_t
    {
        EXPECT_EQ(rWorld.m_forces, 1337 + 42);
        rWorld.m_positions += rWorld.m_forces;
        rWorld.m_forces = 0;
        return {{0b01}};
    });

    // Draw things moved by physics update. If 'updWorld' wasn't enqueued, then
    // this will still run, as no 'needPhysics' tasks are incomplete
    TaskId const taskD = builder.task()
            .depends_on({tgt.positions, tgt.renderRequestIn})
            .fulfills({tgt.renderDoneOut})
            .func([] (TestWorld& rWorld) -> FulfillDirty_t
    {
        EXPECT_EQ(rWorld.m_positions, 1337 + 42);
        rWorld.m_canvas.emplace("Physics Cube");
        return {{0b01}};
    });

    // Draw things unrelated to physics. This is allowed to be the first task
    // to run
    TaskId const taskE = builder.task()
        .depends_on({tgt.renderRequestIn})
        .fulfills({tgt.renderDoneOut})
        .func([] (TestWorld& rWorld) -> FulfillDirty_t
    {
        rWorld.m_canvas.emplace("Terrain");
        return {{0b01}};
    });

    ExecGraph const graph = make_exec_graph(tasks, {&edges});

    // Random checks to assure the graph structure is properly built
    ASSERT_EQ(graph.m_targetDependents[ uint32_t(tgt.timeIn)          ].size(), 3);
    ASSERT_EQ(graph.m_targetDependents[ uint32_t(tgt.forces)          ].size(), 1);
    ASSERT_EQ(graph.m_targetDependents[ uint32_t(tgt.positions)       ].size(), 1);
    ASSERT_EQ(graph.m_targetDependents[ uint32_t(tgt.renderRequestIn) ].size(), 2);
    ASSERT_EQ(graph.m_targetDependents[ uint32_t(tgt.renderDoneOut)   ].size(), 0);
    ASSERT_TRUE(  contains(graph.m_targetFulfilledBy[uint32_t(tgt.forces)],        taskB) );
    ASSERT_FALSE( contains(graph.m_targetFulfilledBy[uint32_t(tgt.renderDoneOut)], taskC) );
    ASSERT_TRUE(  contains(graph.m_taskDependOn[uint32_t(taskA)], tgt.timeIn)       );
    ASSERT_FALSE( contains(graph.m_taskDependOn[uint32_t(taskB)], tgt.positions)    );
    ASSERT_TRUE(  contains(graph.m_taskDependOn[uint32_t(taskC)], tgt.timeIn)       );
    ASSERT_TRUE(  contains(graph.m_taskDependOn[uint32_t(taskC)], tgt.forces)       );
    ASSERT_TRUE(  contains(graph.m_taskFulfill[uint32_t(taskD)], tgt.renderDoneOut) );
    ASSERT_FALSE( contains(graph.m_taskFulfill[uint32_t(taskE)], tgt.forces)        );

    // Execute

    TestWorld      world;
    ExecContext    exec;
    exec.resize(tasks);

    // Repeat (with randomness) to test many possible execution orders
    for (int i = 0; i < sc_repetitions; ++i)
    {
        world.m_deltaTimeIn = 1;
        world.m_positions = 0;
        world.m_canvas.clear();

        // Enqueue initial tasks
        // This roughly indicates "Time has changed" and "Render requested"
        exec.m_targetDirty.set(std::size_t(tgt.timeIn));
        exec.m_targetDirty.set(std::size_t(tgt.renderRequestIn));
        enqueue_dirty(tasks, graph, exec);

        randomized_singlethreaded_execute(
                tasks, graph, exec, randGen, 5,
                    [&functions, &world] (TaskId const task) -> FulfillDirty_t
        {
            return functions[task](world);
        });

        ASSERT_TRUE(world.m_canvas.contains("Physics Cube"));
        ASSERT_TRUE(world.m_canvas.contains("Terrain"));
    }
}


// TODO: Multi-threaded test with limits. Actual multithreading isn't needed;
//       as long as task_start/finish are called at the right times




