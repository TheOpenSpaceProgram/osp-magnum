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
#include <osp/tasks/execute_simple.h>

#include <gtest/gtest.h>

#include <functional>
#include <random>
#include <set>

using namespace osp;

struct World
{
    int m_deltaTimeIn{0};
    int m_forces{0};
    int m_positions{0};
    std::set<std::string> m_canvas;
};

// Single-threaded test against World with order-dependent tasks
TEST(Tasks, SingleThreaded)
{
    constexpr uint32_t const sc_seed = 69;
    constexpr int const sc_repetitions = 32;

    // Setup structs for storing tasks and tags

    TaskTags tags;
    tags.m_tags.reserve(128); // Max 128 tags, aka: just two 64-bit integers
    tags.m_tagDepends.resize(tags.m_tags.capacity() * tags.m_tagDependsPerTag,
                             lgrn::id_null<TaskTags::Tag>());
    tags.m_tagLimits.resize(tags.m_tags.capacity());

    TaskFunctions<std::function<void(World&)>> functions;

    // Create tags and set relationships between them

    auto builder = TaskBuilder{tags, functions};

    // Tags are simply enum class integers
    auto const [updWorld, updRender, forces, physics, needPhysics, draw]
            = builder.create_tags<6>();

    // Limit sets how many tasks using a certain tag can run simultaneously, but
    // are unused in this test.
    // Dependency tags restricts a task from running until all tasks containing
    // tags it depends on are compelete.
    builder.tag(forces).limit(1);
    builder.tag(physics).depend_on({forces});
    builder.tag(needPhysics).depend_on({physics});
    builder.tag(draw).limit(1);

    // Start adding tasks / systems. The order these are added does not matter.

    // Calculate forces needed by the physics update
    builder.task()
        .assign({updWorld, forces})
        .function([] (World& rWorld)
    {
        rWorld.m_forces += 42 * rWorld.m_deltaTimeIn;
    });
    builder.task()
        .assign({updWorld, forces})
        .function([] (World& rWorld)
    {
        rWorld.m_forces += 1337 * rWorld.m_deltaTimeIn;
    });

    // Main Physics update
    builder.task()
        .assign({updWorld, physics})
        .function([] (World& rWorld)
    {
        EXPECT_EQ(rWorld.m_forces, 1337 + 42);
        rWorld.m_positions += rWorld.m_forces;
        rWorld.m_forces = 0;
    });
    // Physics update can be split into many smaller tasks. Tasks tagged with
    // 'needPhysics' will run once ALL tasks tagged with 'physics' are done.
    builder.task()
            .assign({updWorld, physics})
            .function([] (World& rWorld)
    {
        rWorld.m_deltaTimeIn = 0;
    });

    // Draw things moved by physics update. If 'updWorld' wasn't enqueued, then
    // this will still run, as no 'needPhysics' tasks are incomplete
    builder.task()
        .assign({updRender, needPhysics, draw})
        .function([] (World& rWorld)
    {
        EXPECT_EQ(rWorld.m_positions, 1337 + 42);
        rWorld.m_canvas.emplace("Physics Cube");
    });

    // Draw things unrelated to physics. This is allowed to be the first task
    // to run
    builder.task()
        .assign({updRender, draw})
        .function([] (World& rWorld)
    {
        rWorld.m_canvas.emplace("Terrain");
    });

    // Start execution

    ExecutionContext exec;
    exec.m_tagIncompleteCounts  .resize(tags.m_tags.capacity(), 0);
    exec.m_tagRunningCounts     .resize(tags.m_tags.capacity(), 0);
    exec.m_taskQueuedCounts     .resize(tags.m_tasks.capacity(), 0);

    std::mt19937 gen(sc_seed);

    std::vector<uint64_t> tagsToRun(tags.m_tags.vec().size());
    to_bitspan({updWorld, updRender}, tagsToRun);

    World world;

    // Repeat (with randomness) to test many possible execution orders
    for (int i = 0; i < sc_repetitions; ++i)
    {
        // Enqueue all tasks tagged with updWorld and updRender
        task_enqueue(tags, exec, tagsToRun);

        // Its best to pass all external information (such as delta time) into
        // the systems instead of leaving them to figure it out.
        // Enqueuing should be similar to setting a dirty flag.
        world.m_deltaTimeIn = 1;
        world.m_positions = 0;
        world.m_canvas.clear();

        // Run until there's no tasks left to run
        while (true)
        {
            std::vector<uint64_t> tasksToRun(tags.m_tasks.vec().size());
            task_list_available(tags, exec, tasksToRun);
            auto const tasksToRunBits = lgrn::bit_view(tasksToRun);
            unsigned int const availableCount = tasksToRunBits.count();

            if (availableCount == 0)
            {
                break;
            }

            // Choose a random available task
            auto const choice = std::uniform_int_distribution<unsigned int>{0, availableCount - 1}(gen);
            auto const task = TaskTags::Task(*std::next(tasksToRunBits.ones().begin(), choice));

            task_start(tags, exec, task);
            functions.m_taskFunctions[std::size_t(task)](world);
            task_finish(tags, exec, task);
        }

        ASSERT_TRUE(world.m_canvas.contains("Physics Cube"));
        ASSERT_TRUE(world.m_canvas.contains("Terrain"));
    }
}


// TODO: Multi-threaded test with limits. Actual multithreading isn't needed;
//       as long as task_start/finish are called at the right times
