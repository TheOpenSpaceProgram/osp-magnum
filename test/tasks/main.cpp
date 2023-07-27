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
#include <numeric>
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
void randomized_singlethreaded_execute(Tasks const& tasks, TaskGraph const& graph, ExecContext& rExec, std::mt19937 &rRand, int maxRuns, RUN_TASK_T && runTask)
{
    for (int i = 0; i < maxRuns; ++i)
    {
        auto const runTasksLeft     = rExec.tasksQueuedRun.size();
        auto const blockedTasksLeft = rExec.tasksQueuedBlocked.size();

        if (runTasksLeft+blockedTasksLeft == 0)
        {
            break;
        }

        if (runTasksLeft != 0)
        {
            TaskId const        randomTask  = rExec.tasksQueuedRun.at(rRand() % runTasksLeft);
            TaskActions const   status      = runTask(randomTask);
            complete_task(tasks, graph, rExec, randomTask, status);
        }

        enqueue_dirty(tasks, graph, rExec);
    }
}

//-----------------------------------------------------------------------------

namespace test_a
{

enum class Stages { Fill, Use, Clear };

struct Pipelines
{
    osp::PipelineDef<Stages> vec;
};

} // namespace test_a

// Test pipeline consisting of parallel tasks
TEST(Tasks, BasicSingleThreadedParallelTasks)
{
    using namespace test_a;
    using enum Stages;

    // NOTE
    // If this was multithreaded, then multiple threads writing to a single container is a bad
    // idea. The proper way to do this is to make a vector per-thread. Targets are still
    // well-suited for this problem, as these per-thread vectors can all be represented with the
    // same TargetId.

    using BasicTraits_t     = BasicBuilderTraits<TaskActions(*)(int const, std::vector<int>&, int&)>;
    using Builder_t         = BasicTraits_t::Builder;
    using TaskFuncVec_t     = BasicTraits_t::FuncVec_t;

    constexpr int sc_repetitions     = 32;
    constexpr int sc_pusherTaskCount = 24;
    constexpr int sc_totalTaskCount  = sc_pusherTaskCount + 2;
    std::mt19937 randGen(69);

    // Step 1: Create tasks

    Tasks           tasks;
    TaskEdges       edges;
    TaskFuncVec_t   functions;
    Builder_t       builder{tasks, edges, functions};
    auto pl = builder.create_pipelines<Pipelines>();

    // Multiple tasks push to the vector
    for (int i = 0; i < sc_pusherTaskCount; ++i)
    {
        builder.task()
            .run_on  (pl.vec(Fill))
            .func( [] (int const in, std::vector<int>& rOut, int &rChecksRun) -> TaskActions
        {
            rOut.push_back(in);
            return {};
        });
    }

    // Use vector
    builder.task()
        .run_on(pl.vec(Use))
        .func( [] (int const in, std::vector<int>& rOut, int &rChecksRun) -> TaskActions
    {
        int const sum = std::accumulate(rOut.begin(), rOut.end(), 0);
        EXPECT_EQ(sum, in * sc_pusherTaskCount);
        ++rChecksRun;
        return {};
    });

    // Clear vector after use
    builder.task()
        .run_on({pl.vec(Clear)})
        .func( [] (int const in, std::vector<int>& rOut, int &rChecksRun) -> TaskActions
    {
        rOut.clear();
        return {};
    });

    // Step 2: Compile tasks into an execution graph

    TaskGraph const graph = make_exec_graph(tasks, {&edges});

    // Step 3: Run

    ExecContext exec;
    exec_resize(tasks, graph, exec);

    int                 checksRun = 0;
    int                 input     = 0;
    std::vector<int>    output;

    // Repeat with randomness to test many possible execution orders
    for (int i = 0; i < sc_repetitions; ++i)
    {
        input = 1 + randGen() % 30;

        pipeline_run(exec, pl.vec);
        enqueue_dirty(tasks, graph, exec);

        randomized_singlethreaded_execute(tasks, graph, exec, randGen, sc_totalTaskCount, [&functions, &input, &output, &checksRun] (TaskId const task) -> TaskActions
        {
            return functions[task](input, output, checksRun);
        });
    }

    ASSERT_EQ(checksRun, sc_repetitions);
}

//-----------------------------------------------------------------------------

namespace test_b
{

struct TestState
{
    int     checks              { 0 };
    bool    normalDone          { false };
    bool    expectOptionalDone  { false };
    bool    optionalDone        { false };
};

enum class Stages { Schedule, Write, Read, Clear };

struct Pipelines
{
    osp::PipelineDef<Stages> normal;
    osp::PipelineDef<Stages> optional;

    // Extra pipeline blocked by optional task to make the test case more difficult
    osp::PipelineDef<Stages> distraction;
};

} // namespace test_b

// Test that features a looping 'normal' pipeline and an 'optional' pipeline that has a 50% chance of running
TEST(Tasks, BasicSingleThreadedOptional)
{
    using namespace test_b;
    using enum Stages;

    using BasicTraits_t     = BasicBuilderTraits<TaskActions(*)(TestState&, std::mt19937&)>;
    using Builder_t         = BasicTraits_t::Builder;
    using TaskFuncVec_t     = BasicTraits_t::FuncVec_t;

    constexpr int sc_repetitions = 128;
    std::mt19937 randGen(69);

    Tasks           tasks;
    TaskEdges       edges;
    TaskFuncVec_t   functions;
    Builder_t       builder{tasks, edges, functions};

    auto const pl = builder.create_pipelines<Pipelines>();

    builder.pipeline(pl.optional)
            .parent(pl.normal);

    builder.pipeline(pl.distraction)
            .parent(pl.normal);

    builder.task()
        .run_on   ({pl.optional(Schedule)})
        .func( [] (TestState& rState, std::mt19937 &rRand) -> TaskActions
    {
        if (rRand() % 2 == 0)
        {
            rState.expectOptionalDone = true;
            return { };
        }
        else
        {
            return TaskAction::Cancel;
        }
    });

    builder.task()
        .run_on   ({pl.normal(Write)})
        .func( [] (TestState& rState, std::mt19937 &rRand) -> TaskActions
    {
        rState.normalDone = true;
        return {};
    });

    builder.task()
        .run_on   ({pl.optional(Write)})
        .sync_with({pl.distraction(Read)})
        .func( [] (TestState& rState, std::mt19937 &rRand) -> TaskActions
    {
        rState.optionalDone = true;
        return {};
    });

    builder.task()
        .run_on   ({pl.normal(Read)})
        .sync_with({pl.optional(Read)})
        .func( [] (TestState& rState, std::mt19937 &rRand) -> TaskActions
    {
        EXPECT_TRUE(rState.normalDone);
        EXPECT_EQ(rState.expectOptionalDone, rState.optionalDone);
        return {};
    });

    builder.task()
        .run_on   ({pl.normal(Clear)})
        .func( [] (TestState& rState, std::mt19937 &rRand) -> TaskActions
    {
        ++ rState.checks;
        rState.normalDone           = false;
        rState.expectOptionalDone   = false;
        rState.optionalDone         = false;
        return {};
    });

    builder.task()
        .run_on   ({pl.distraction(Write)})
        .func( [] (TestState&, std::mt19937&) -> TaskActions
    {
        return {};
    });

    builder.task()
        .run_on   ({pl.distraction(Read)})
        .func( [] (TestState&, std::mt19937&) -> TaskActions
    {
        return {};
    });


    TaskGraph const graph = make_exec_graph(tasks, {&edges});

    // Execute

    ExecContext exec;
    exec_resize(tasks, graph, exec);

    TestState world;

    for (int i = 0; i < sc_repetitions; ++i)
    {
        pipeline_run(exec, pl.normal);
        enqueue_dirty(tasks, graph, exec);

        randomized_singlethreaded_execute(
                tasks, graph, exec, randGen, 10,
                    [&functions, &world, &randGen] (TaskId const task) -> TaskActions
        {
            return functions[task](world, randGen);
        });
    }

    // Assure that the tasks above actually ran, and didn't just skip everything
    // Max of 5 tasks run each loop
    ASSERT_GT(world.checks, sc_repetitions / 5);
}

//-----------------------------------------------------------------------------

namespace test_c
{

struct TestState
{
    std::vector<int> in;
    std::vector<int> out;

    int processA{0};
    int processB{0};
};

enum class Stages { Schedule, Process, Done, Clear };

struct Pipelines
{
    osp::PipelineDef<Stages> main;
    osp::PipelineDef<Stages> loop;
    osp::PipelineDef<Stages> processA;
    osp::PipelineDef<Stages> processB;
};

} // namespace test_c

//
TEST(Tasks, BasicSingleThreadedLoop)
{
    using namespace test_c;
    using enum Stages;

    using BasicTraits_t     = BasicBuilderTraits<TaskActions(*)(TestState&, std::mt19937 &)>;
    using Builder_t         = BasicTraits_t::Builder;
    using TaskFuncVec_t     = BasicTraits_t::FuncVec_t;

    constexpr int sc_repetitions = 128;
    std::mt19937 randGen(69);

    Tasks           tasks;
    TaskEdges       edges;
    TaskFuncVec_t   functions;
    Builder_t       builder{tasks, edges, functions};

    auto const pl = builder.create_pipelines<Pipelines>();

    builder.pipeline(pl.loop)    .parent(pl.main).loops(true);
    builder.pipeline(pl.processA).parent(pl.loop);
    builder.pipeline(pl.processB).parent(pl.loop);

    builder.task()
        .run_on   ({pl.main(Schedule)})
        .func( [] (TestState& rState, std::mt19937 &rRand) -> TaskActions
    {
        return { };
    });

    builder.task()
        .run_on   ({pl.loop(Schedule)})
        .func( [] (TestState& rState, std::mt19937 &rRand) -> TaskActions
    {
        return { };
    });


    TaskGraph const graph = make_exec_graph(tasks, {&edges});

    // Execute

    ExecContext exec;
    exec_resize(tasks, graph, exec);

    TestState world;

    for (int i = 0; i < sc_repetitions; ++i)
    {
        pipeline_run(exec, pl.main);
        enqueue_dirty(tasks, graph, exec);

        randomized_singlethreaded_execute(
                tasks, graph, exec, randGen, 999999,
                    [&functions, &world, &randGen] (TaskId const task) -> TaskActions
        {
            return functions[task](world, randGen);
        });
    }
}

//-----------------------------------------------------------------------------

namespace test_gameworld
{

struct World
{
    int m_deltaTimeIn{1};
    int m_forces{0};
    int m_positions{0};
    std::set<std::string> m_canvas;
};

enum class StgSimple { Recalc, Use };
enum class StgRender { Render, Done };

struct Pipelines
{
    osp::PipelineDef<StgSimple> time;       /// External time input, manually set dirty when time 'changes', and the world needs to update
    osp::PipelineDef<StgSimple> forces;     /// Forces need to be calculated before physics
    osp::PipelineDef<StgSimple> positions;  /// Positions calculated by physics task
    osp::PipelineDef<StgRender> render;     /// External render request, manually set dirty when a new frame to render is required
};

} // namespace test_gameworld


// Single-threaded test against World with order-dependent tasks
TEST(Tasks, BasicSingleThreadedGameWorld)
{
    using namespace test_gameworld;
    using enum StgSimple;
    using enum StgRender;

    using BasicTraits_t     = BasicBuilderTraits<TaskActions(*)(World&)>;
    using Builder_t         = BasicTraits_t::Builder;
    using TaskFuncVec_t     = BasicTraits_t::FuncVec_t;

    constexpr int sc_repetitions = 128;
    std::mt19937 randGen(69);

    Tasks           tasks;
    TaskEdges       edges;
    TaskFuncVec_t   functions;
    Builder_t       builder{tasks, edges, functions};

    auto const pl = builder.create_pipelines<Pipelines>();

    // Start adding tasks. The order these are added does not matter.

    // Two tasks calculate forces needed by the physics update
    builder.task()
        .run_on   ({pl.time(Use)})
        .sync_with({pl.forces(Recalc)})
        .func( [] (World& rWorld) -> TaskActions
    {
        rWorld.m_forces += 42 * rWorld.m_deltaTimeIn;
        return {};
    });
    builder.task()
        .run_on   ({pl.time(Use)})
        .sync_with({pl.forces(Recalc)})
        .func([] (World& rWorld) -> TaskActions
    {
        rWorld.m_forces += 1337 * rWorld.m_deltaTimeIn;
        return {};
    });

    // Main Physics update
    builder.task()
        .run_on   ({pl.time(Use)})
        .sync_with({pl.forces(Use), pl.positions(Recalc)})
        .func([] (World& rWorld) -> TaskActions
    {
        EXPECT_EQ(rWorld.m_forces, 1337 + 42);
        rWorld.m_positions += rWorld.m_forces;
        rWorld.m_forces = 0;
        return {};
    });

    // Draw things moved by physics update. If 'updWorld' wasn't enqueued, then
    // this will still run, as no 'needPhysics' tasks are incomplete
    builder.task()
        .run_on   ({pl.render(Render)})
        .sync_with({pl.positions(Use)})
        .func([] (World& rWorld) -> TaskActions
    {
        EXPECT_EQ(rWorld.m_positions, 1337 + 42);
        rWorld.m_canvas.emplace("Physics Cube");
        return {};
    });

    // Draw things unrelated to physics. This is allowed to be the first task
    // to run
    builder.task()
        .run_on  ({pl.render(Render)})
        .func([] (World& rWorld) -> TaskActions
    {
        rWorld.m_canvas.emplace("Terrain");
        return {};
    });

    TaskGraph const graph = make_exec_graph(tasks, {&edges});

    // Execute

    ExecContext exec;
    exec_resize(tasks, graph, exec);

    World world;

    // Repeat (with randomness) to test many possible execution orders
    for (int i = 0; i < sc_repetitions; ++i)
    {
        world.m_deltaTimeIn = 1;
        world.m_positions = 0;
        world.m_canvas.clear();

        // Enqueue initial tasks
        // This roughly indicates "Time has changed" and "Render requested"
        pipeline_run(exec, pl.time);
        pipeline_run(exec, pl.forces);
        pipeline_run(exec, pl.positions);
        pipeline_run(exec, pl.render);
        enqueue_dirty(tasks, graph, exec);

        randomized_singlethreaded_execute(
                tasks, graph, exec, randGen, 5,
                    [&functions, &world] (TaskId const task) -> TaskActions
        {
            return functions[task](world);
        });

        ASSERT_TRUE(world.m_canvas.contains("Physics Cube"));
        ASSERT_TRUE(world.m_canvas.contains("Terrain"));
    }
}


// TODO: Multi-threaded test with limits. Actual multithreading isn't needed;
//       as long as task_start/finish are called at the right times
