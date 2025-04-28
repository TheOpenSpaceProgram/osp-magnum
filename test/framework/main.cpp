/**
 * Open Space Program
 * Copyright © 2019-2024 Open Space Program Project
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
/**
 * @file
 * @brief A tutorial for the OSP framework; this is not much of a unit test.
 *
 * The framework...
 * * stores arbitrary application data through DataIds.
 * * uses osp/tasks to organize Tasks and how they access data. Pipelines and Tasks are not
 *   well-utilized here, so check out the 'test_tasks' unit test for more info.
 * * is NOT a game engine. Framework does not force data structures into game objects or any of
 *   that BS. The programmer is free to represent the world through means that best fits the
 *   problem at hand (tic-tac-toe can be a 3x3 array).
 * * is mostly just plain data and requires a separate executor to run it.
 * * bundles Tasks and Data together into extremely composable 'Features'.
 * * does witchcraft C++ metaprogramming. It's complicated and may not be worth trying to
 *   understand, but all it really does is write to osp::Framework, which is a simple struct.
 *
 *
 * Question: Is this the right API?
 *
 * Task/Pipeline/Framework stuff is the result of many iterations of osp-magnum, being rewritten
 * and simplified over the span of years to to what it is today.
 *
 * It is designed to cleanly represent the control flow in a complex simulation of vehicles with
 * wiring and fuel flow moving across terrain in a conventional physics engine scene representing a
 * part of huge planet with a rotating coordinate space that is part of an orbital simulation with
 * everything intended to be moddable and extendable.
 *
 * This does it quite well so better be the correct API, or is at least close to the ideal solution.
 */
#include <osp/executor/singlethread_framework.h>
#include <osp/framework/builder.h>
#include <osp/util/logging.h>

#include <spdlog/sinks/stdout_color_sinks.h>

#include <gtest/gtest.h>

using namespace osp;
using namespace osp::fw;

namespace test_a
{


enum class EStgContinuous {
    Modify = 0,
    Schedule = 1,
    Read = 2
};
osp::PipelineTypeInfo const infoForEStgContinuous{
    .debugName = "Continuous",
    .stages = {{
        { .name = "Modify"                          },
        { .name = "Schedule",   .isSchedule = true  },
        { .name = "Read",                           } } }};

enum class EStgOptionalPath {
    Schedule = 0,
    Signal = 1,
    Run = 2,
    Done = 3
};
osp::PipelineTypeInfo const infoForEStgOptionalPath{
    .debugName = "OptionalPath",
    .stages = {{
        { .name = "Schedule",   .isSchedule = true  },
        { .name = "Signal",                         },
        { .name = "Run",        .useCancel = true   },
        { .name = "Done"                            } } }};



struct Aquarium
{
    bool runMainLoop = false;
    bool runAquariumUpdate = false;
    //int waterLevel = 10;
};

struct AquariumFish
{
    int fishCount = 10;
};

struct AquariumSharks
{
    int sharkCount = 2;
};


// Feature Interfaces

// Feature Interfaces provide a way to share Data and Pipelines between Features. Features can
// Implement an Interface, and another Feature can DependOn it. This acts as a ayer of indirection
// that prevents Features from needing to directly depend on each other, which had been messy and
// inflexible in previous revisions of OSP.
//
// Metaprogramming does some magic to turn these FI* structs into DependOn<FIAquarium> or the
// return value of get_interface, "something.pl.somethingElse".
//
// 'something.di' is a FI*::DataIds, and 'something.pl' is a FI*::Pipelines.
//
// A postfix DI and PL are used to better show where these variables are coming from.
//
// The FI* structs themselves are never actually constructed.

struct FIMainLoop {
    struct LoopBlockIds {
        LoopBlockId mainLoop;
    };
    struct DataIds { };
    struct Pipelines {
        //PipelineDef<EStgOptionalPath> mainLoopPL{"Main Loop"};
    };
};

struct FIAquarium {
    struct DataIds {
        DataId aquariumDI;
    };
    struct Pipelines {
        // Controls access to Aquarium
        //PipelineDef<EStgContinuous> aquariumPL{"aquariumPL"};

        /// Boolean decision on whether we want to update the aquarium or not
        PipelineDef<EStgOptionalPath> aquariumUpdatePL{"aquariumUpdatePL"};
    };
};

struct FIFish {
    struct DataIds {
        DataId fishDI;
    };
    struct Pipelines {
        /// Controls access to AquariumFish struct
        PipelineDef<EStgContinuous> fishPL{"fishPL"};
    };
};

struct FISharks {
    struct DataIds {
        DataId sharksDI;
    };
    struct Pipelines {
        /// Controls access to AquariumSharks struct
        PipelineDef<EStgContinuous> sharksPL{"sharksPL"};
    };
};


// Features

// feature_def(...) reads and iterates the function arguments of the given lambda and does stuff
// accordingly. Yes, this is possible to do in C++.
FeatureDef const ftrWorld = feature_def("World", [] (
        FeatureBuilder          &rFB,
        Implement<FIMainLoop>   mainLoop,
        Implement<FIAquarium>   aquarium)
{
    rFB.data_emplace<Aquarium>(aquarium.di.aquariumDI);

    // 'Signal' will stop the main loop pipeline from proceeding until exec.signal is called.
    // If wait_for_signal isn't added to the main loop pipeline, then it will just infinite loop
    // when exec.wait(fw) is called.

    rFB.pipeline(aquarium.pl.aquariumUpdatePL).parent(mainLoop.loopblks.mainLoop).wait_for_signal(EStgOptionalPath::Signal);

    // Allow controlling the main loop so it can exit cleanly, controlled with runMainLoop.
    rFB.task()
        .name       ("Schedule main loop")
        .schedules  ({mainLoop.loopblks.mainLoop})
        .args       ({        aquarium.di.aquariumDI })
        .func       ([] (Aquarium const &rAquarium)
    {
        return TaskActions{ .cancel = !rAquarium.runMainLoop };
    });

    // Running the aquarium update is optional and controlled with runAquariumUpdate.
    // sync_with also ties aquariumUpdatePL to the main loop
    rFB.task()
        .name       ("Schedule aquarium update")
        .schedules  ({aquarium.pl.aquariumUpdatePL})
        .args       ({        aquarium.di.aquariumDI })
        .func       ([] (Aquarium const &rAquarium)
    {
        return TaskActions{ .cancel = ! rAquarium.runAquariumUpdate };
    });
});

FeatureDef const ftrFish = feature_def("Fish", [] (
        Implement<FIFish>       fish,
        DependOn<FIMainLoop>    mainLoop,
        FeatureBuilder          &rFB, // For demonstration, argument order doesn't matter. This is usually the first argument
        DependOn<FIAquarium>    aquarium)
{
    rFB.data_emplace<AquariumFish>(fish.di.fishDI);

    rFB.pipeline(fish.pl.fishPL).parent(mainLoop.loopblks.mainLoop);
});

FeatureDef const ftrSharks = feature_def("Sharks", [] (
        FeatureBuilder          &rFB,
        Implement<FISharks>     sharks,
        DependOn<FIMainLoop>    mainLoop,
        DependOn<FIAquarium>    aquarium,
        DependOn<FIFish>        fish,
        entt::any               userData) // optional data can be passed in through add_feature
{
    ASSERT_TRUE(entt::any_cast<std::string>(userData) == "user data!");

    rFB.data_emplace<AquariumSharks>(sharks.di.sharksDI);

    rFB.pipeline(sharks.pl.sharksPL).parent(mainLoop.loopblks.mainLoop);

    // Runs every aquarium update
    rFB.task()
        .name       ("Each shark eats a fish")
        .sync_with  ({aquarium.pl.aquariumUpdatePL(EStgOptionalPath::Run), fish.pl.fishPL(EStgContinuous::Modify), sharks.pl.sharksPL(EStgContinuous::Read)})
        .args       ({          fish.di.fishDI,            sharks.di.sharksDI })
        .func       ([] (AquariumFish &rFish, AquariumSharks const& rSharks)
    {
        rFish.fishCount -= rSharks.sharkCount;
    });
});

} // namespace test_a


TEST(Tasks, Basics)
{
    using namespace test_a;

    auto &rPltypeReg = PipelineTypeIdReg::instance();
    rPltypeReg.assign_pltype_info<EStgContinuous>   (infoForEStgContinuous);
    rPltypeReg.assign_pltype_info<EStgOptionalPath> (infoForEStgOptionalPath);

    auto pSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    osp::Logger_t logger = std::make_shared<spdlog::logger>("executor", pSink);
    osp::set_thread_logger(logger);

    Framework fw;

    // Contexts adds a way to separate major sections of the Framework.
    // Feature Interfaces are added per-context. A context can't have two of the same
    // implementations of a Feature Interface. If we were to add two aquariums that are logically
    // separated and can run in parallel, we can use two contexts.
    ContextId const ctx = fw.m_contextIds.create();

    ContextBuilder cb{ctx, {}, fw};
    cb.add_feature(ftrWorld);
    cb.add_feature(ftrFish);
    cb.add_feature(ftrSharks, std::string{"user data!"});
    ContextBuilder::finalize(std::move(cb));

    auto const fish             = fw.get_interface<FIFish>(ctx);
    auto const mainLoop         = fw.get_interface<FIMainLoop>(ctx);
    auto const aquarium         = fw.get_interface<FIAquarium>(ctx);

    auto       &rAquarium       = fw.data_get<Aquarium>(aquarium.di.aquariumDI);
    auto       &rAquariumFish   = fw.data_get<AquariumFish>(fish.di.fishDI);

    osp::exec::SinglethreadFWExecutor exec;
    exec.load(fw);

    ASSERT_FALSE(exec.is_running(fw));
    EXPECT_EQ(rAquariumFish.fishCount, 10);

    // Signal main loop block to start, but doesn't run because "Schedule main loop" sees rAquarium.runMainLoop=false.
    exec.signal(fw, mainLoop.loopblks.mainLoop);
    exec.wait(fw);
    ASSERT_FALSE(exec.is_running(fw));

    // Start the main loop for real
    rAquarium.runMainLoop = true;
    exec.signal(fw, mainLoop.loopblks.mainLoop);
    exec.wait(fw);

    // execution waits at signal aquariumUpdatePL
    ASSERT_TRUE(exec.is_running(fw));
    ASSERT_EQ(rAquariumFish.fishCount, 10);

    exec.signal(fw, aquarium.pl.aquariumUpdatePL);
    exec.wait(fw);

    // just exits as rAquarium.runAquariumUpdate != true
    ASSERT_TRUE(exec.is_running(fw));
    ASSERT_EQ(rAquariumFish.fishCount, 10);

    rAquarium.runAquariumUpdate = true;
    exec.signal(fw, aquarium.pl.aquariumUpdatePL);
    exec.wait(fw);


    // Start updating the aquarium
    rAquarium.runAquariumUpdate = true;
    exec.signal(fw, mainLoop.loopblks.mainLoop);
    exec.wait(fw);

    ASSERT_TRUE(exec.is_running(fw));
    ASSERT_EQ(rAquariumFish.fishCount, 8);

    // Sharks have eaten 2 fish


    exec.signal(fw, mainLoop.loopblks.mainLoop);
    exec.wait(fw);

    // Sharks have eaten 2 more fish
    ASSERT_EQ(rAquariumFish.fishCount, 6);

    // Stop the main loop
    rAquarium.runMainLoop = false;
    exec.signal(fw, mainLoop.loopblks.mainLoop);
    exec.wait(fw);

    ASSERT_FALSE(exec.is_running(fw));
}

//-----------------------------------------------------------------------------

// Test metaprogramming used by framework

using Input_t = Stuple<int, float, char, std::string, double>;
using Output_t = filter_parameter_pack< Input_t, std::is_integral >::value;

static_assert(std::is_same_v<Output_t, Stuple<int, char>>);

// Test empty. Nothing is being tested, PRED_T can be anything.
template<typename T>
struct Useless{};
static_assert(std::is_same_v<filter_parameter_pack< Stuple<>, Useless >::value, Stuple<>>);


// Some janky technique to pass the parameter pack from stuple to a different type

template<typename ... T>
struct TargetType {};

// Create a template function with stuple<T...> as an argument, and call it with inferred template
// parameters to obtain the parameter pack. Return value can be used for the target type.
template<typename ... T>
constexpr TargetType<T...> why_cpp(Stuple<T...>) { };

using WhatHow_t = decltype(why_cpp(Output_t{}));

static_assert(std::is_same_v<WhatHow_t, TargetType<int, char>>);

using Lambda_t  = decltype([] (int a, float b) { return 'c'; });
using FuncPtr_t = char(*)(int, float);

static_assert(std::is_same_v< as_function_ptr_t<Lambda_t>, FuncPtr_t >);

static_assert(std::is_same_v< as_function_ptr_t<FuncPtr_t>, char(*)(int, float) >);

inline void notused()
{
    int asdf = 69;

    [[maybe_unused]] auto lambdaWithCapture = [asdf] (int a, float b) { return 'c'; };

    using LambdaWithCapture_t = decltype(lambdaWithCapture);

    static_assert( ! CStatelessLambda<LambdaWithCapture_t> );
}

