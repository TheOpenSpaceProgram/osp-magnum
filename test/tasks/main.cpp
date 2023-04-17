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
//#include <osp/tasks/execute_simple.h>

#include <gtest/gtest.h>

#include <functional>
#include <random>
#include <set>

using namespace osp;

template <typename RANGE_T, typename VALUE_T>
bool contains(RANGE_T const& range, VALUE_T const& value)
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

struct World
{
    int m_deltaTimeIn{0};
    int m_forces{0};
    int m_positions{0};
    std::set<std::string> m_canvas;
};

using TaskFunctions_t = KeyedVec<TaskId, std::function<void(World&)>>;

struct CustomTaskBuilder;

struct CustomTaskRef : public osp::TaskRefBase<CustomTaskBuilder, CustomTaskRef>
{
    CustomTaskRef& func(std::function<void(World&)> in);
};

struct CustomTaskBuilder : public osp::TaskBuilderBase<CustomTaskBuilder, CustomTaskRef>
{
    TaskFunctions_t & m_funcs;
};

CustomTaskRef& CustomTaskRef::func(std::function<void(World&)> in)
{
    m_rBuilder.m_funcs.resize(m_rBuilder.m_taskIds.capacity());
    m_rBuilder.m_funcs[m_taskId] = std::move(in);
    return *this;
}

struct WorldTargets
{
    TargetId timeIn;            /// Fire externally when time changes, and world needs to update
    TargetId forces;            /// Forces need to be calculated for physics
    TargetId physics;           /// Physics calculations
    TargetId renderRequestIn;   /// Fire externally when a new frame to render is required
    TargetId renderDoneOut;     /// Fired when all rendering is finished
};

// Single-threaded test against World with order-dependent tasks
TEST(Tasks, SingleThreaded)
{
    constexpr uint32_t const sc_seed = 69;
    constexpr int const sc_repetitions = 32;

    // Setup structs for storing tasks and tags

    KeyedVec<TaskId, std::function<void(World&)>> functions;

    // Create tags and set relationships between them

    auto builder = CustomTaskBuilder{.m_funcs = functions};

    auto const tgt = builder.create_targets<WorldTargets>();


    // Start adding tasks / systems. The order these are added does not matter.


    // Calculate forces needed by the physics update
    TaskId const taskA = builder.task()
            .depends_on({tgt.timeIn})
            .fulfills({tgt.forces})
            .func( [] (World& rWorld)
    {
        rWorld.m_forces += 42 * rWorld.m_deltaTimeIn;
    });

    TaskId const taskB = builder.task()
            .depends_on({tgt.timeIn})
            .fulfills({tgt.forces})
            .func([] (World& rWorld)
    {
        rWorld.m_forces += 1337 * rWorld.m_deltaTimeIn;
    });

    // Main Physics update
    TaskId const taskC = builder.task()
            .depends_on({tgt.timeIn, tgt.forces})
            .fulfills({tgt.physics})
            .func([] (World& rWorld)
    {
        EXPECT_EQ(rWorld.m_forces, 1337 + 42);
        rWorld.m_positions += rWorld.m_forces;
        rWorld.m_forces = 0;
    });

    // Draw things moved by physics update. If 'updWorld' wasn't enqueued, then
    // this will still run, as no 'needPhysics' tasks are incomplete
    TaskId const taskD = builder.task()
            .depends_on({tgt.physics, tgt.renderRequestIn})
            .fulfills({tgt.renderDoneOut})
            .func([] (World& rWorld)
    {
        EXPECT_EQ(rWorld.m_positions, 1337 + 42);
        rWorld.m_canvas.emplace("Physics Cube");
    });

    // Draw things unrelated to physics. This is allowed to be the first task
    // to run
    TaskId const taskE = builder.task()
        .depends_on({tgt.renderRequestIn})
        .fulfills({tgt.renderDoneOut})
        .func([] (World& rWorld)
    {
        rWorld.m_canvas.emplace("Terrain");
    });

    Tasks const tasks = finalize(std::move(builder));

    // Some random checks to assure the graph structure is properly built
    EXPECT_EQ(tasks.m_targetDependents[ uint32_t(tgt.timeIn)          ].size(), 3);
    EXPECT_EQ(tasks.m_targetDependents[ uint32_t(tgt.forces)          ].size(), 1);
    EXPECT_EQ(tasks.m_targetDependents[ uint32_t(tgt.physics)         ].size(), 1);
    EXPECT_EQ(tasks.m_targetDependents[ uint32_t(tgt.renderRequestIn) ].size(), 2);
    EXPECT_EQ(tasks.m_targetDependents[ uint32_t(tgt.renderDoneOut)   ].size(), 0);
    EXPECT_TRUE(  contains(tasks.m_targetFulfilledBy[uint32_t(tgt.forces)],        taskB) );
    EXPECT_FALSE( contains(tasks.m_targetFulfilledBy[uint32_t(tgt.renderDoneOut)], taskC) );
    EXPECT_TRUE(  contains(tasks.m_taskDependOn[uint32_t(taskA)], tgt.timeIn)       );
    EXPECT_FALSE( contains(tasks.m_taskDependOn[uint32_t(taskB)], tgt.physics)      );
    EXPECT_TRUE(  contains(tasks.m_taskDependOn[uint32_t(taskC)], tgt.timeIn)       );
    EXPECT_TRUE(  contains(tasks.m_taskDependOn[uint32_t(taskC)], tgt.forces)       );
    EXPECT_TRUE(  contains(tasks.m_taskFulfill[uint32_t(taskD)], tgt.renderDoneOut) );
    EXPECT_FALSE( contains(tasks.m_taskFulfill[uint32_t(taskE)], tgt.forces)        );


#if 0

    // Start execution

    ExecutionContext exec;
    exec.m_tagIncompleteCounts  .resize(tags.m_tags.capacity(), 0);
    exec.m_tagRunningCounts     .resize(tags.m_tags.capacity(), 0);
    exec.m_taskQueuedCounts     .resize(tasks.m_tasks.capacity(), 0);

    std::mt19937 gen(sc_seed);

    std::vector<uint64_t> tagsToRun(tags.m_tags.vec().size());
    to_bitspan({updWorld, updRender}, tagsToRun);

    World world;

    // Repeat (with randomness) to test many possible execution orders
    for (int i = 0; i < sc_repetitions; ++i)
    {
        // Enqueue all tasks tagged with updWorld and updRender
        task_enqueue(tags, tasks, exec, tagsToRun);

        // Its best to pass all external information (such as delta time) into
        // the systems instead of leaving them to figure it out.
        // Enqueuing should be similar to setting a dirty flag.
        world.m_deltaTimeIn = 1;
        world.m_positions = 0;
        world.m_canvas.clear();

        // Run until there's no tasks left to run
        while (true)
        {
            std::vector<uint64_t> tasksToRun(tasks.m_tasks.vec().size());
            task_list_available(tags, tasks, exec, tasksToRun);
            auto const tasksToRunBits = lgrn::bit_view(tasksToRun);
            unsigned int const availableCount = tasksToRunBits.count();

            if (availableCount == 0)
            {
                break;
            }

            // Choose a random available task
            auto const choice = std::uniform_int_distribution<unsigned int>{0, availableCount - 1}(gen);
            auto const task = TaskId(*std::next(tasksToRunBits.ones().begin(), choice));

            task_start(tags, tasks, exec, task);
            functions.m_taskData[std::size_t(task)](world);
            task_finish(tags, tasks, exec, task);
        }

        ASSERT_TRUE(world.m_canvas.contains("Physics Cube"));
        ASSERT_TRUE(world.m_canvas.contains("Terrain"));
    }
#endif
}


// TODO: Multi-threaded test with limits. Actual multithreading isn't needed;
//       as long as task_start/finish are called at the right times
