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

void register_pltype_info();

// Test 1: Basic functionality of LoopBlocks, Pipeline, Tasks, Feature, Feature Interface, etc...

enum class EStgContinuous {
    Modify      = 0,
    Schedule    = 1,
    Read        = 2
};
osp::PipelineTypeInfo const gc_infoForEStgContinuous{
    .debugName = "Continuous",
    .stages = {{
        { .name = "Modify"                          },
        { .name = "Schedule",   .isSchedule = true  },
        { .name = "Read",                           }
    }},
    .initialStage = osp::StageId{0}
};

enum class EStgOptionalPath {
    Schedule    = 0,
    Run         = 1,
    Done        = 2
};
osp::PipelineTypeInfo const gc_infoForEStgOptionalPath{
    .debugName = "OptionalPath",
    .stages = {{
        { .name = "Schedule",   .isSchedule = true  },
        { .name = "Run",        .useCancel  = true  },
        { .name = "Done"                            }
    }},
    .initialStage = osp::StageId{0}
};

struct Aquarium
{
    int dummy = 0;
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
    struct Pipelines { };
    struct TaskIds {
        TaskId schedule;
    };
};

struct FIAquarium {
    struct DataIds {
        DataId aquariumDI;
    };
    struct Pipelines {
        /// Boolean decision on whether we want to update the aquarium or not
        PipelineDef<EStgOptionalPath> aquariumUpdatePL{"aquariumUpdatePL"};
    };
    struct TaskIds {
        TaskId schedule;
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

    rFB.pipeline(aquarium.pl.aquariumUpdatePL).parent(mainLoop.loopblks.mainLoop);

    // Allow controlling the main loop so it can exit cleanly, controlled with runMainLoop.
    rFB.task(mainLoop.tasks.schedule)
        .name       ("Schedule main loop")
        .ext_finish (true)
        .schedules  ({mainLoop.loopblks.mainLoop});

    // Running the aquarium update is optional and controlled with runAquariumUpdate.
    // sync_with also ties aquariumUpdatePL to the main loop
    rFB.task(aquarium.tasks.schedule)
        .name       ("Schedule aquarium update")
        .ext_finish (true)
        .schedules  ({aquarium.pl.aquariumUpdatePL});
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
    // Since this syncs to "aquariumUpdatePL(EStgOptionalPath::Run)",
    rFB.task()
        .name       ("Each shark eats a fish")
        .sync_with  ({aquarium.pl.aquariumUpdatePL(EStgOptionalPath::Run), fish.pl.fishPL(EStgContinuous::Modify), sharks.pl.sharksPL(EStgContinuous::Read)})
        .args       ({          fish.di.fishDI,            sharks.di.sharksDI })
        .func       ([] (AquariumFish &rFish, AquariumSharks const& rSharks)
    {
        rFish.fishCount -= rSharks.sharkCount;
    });
});

TEST(Framework, Basics)
{
    register_pltype_info();

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
    exec.m_log = logger;
    exec.load(fw);

    exec.wait(fw);
    ASSERT_FALSE(exec.is_running(fw, mainLoop.loopblks.mainLoop));
    EXPECT_EQ(rAquariumFish.fishCount, 10);

    // Run main loop block but canceled so it doesn't do anything.
    exec.task_finish(fw, mainLoop.tasks.schedule, true, {.cancel = true});
    exec.wait(fw);
    exec.task_finish(fw, mainLoop.tasks.schedule, true, {.cancel = true});
    exec.wait(fw);
    ASSERT_FALSE(exec.is_running(fw, mainLoop.loopblks.mainLoop));
    ASSERT_EQ(rAquariumFish.fishCount, 10);

    // Run main loop but not canceled, this starts running
    exec.task_finish(fw, mainLoop.tasks.schedule, true, {.cancel = false});
    exec.wait(fw);
    ASSERT_TRUE(exec.is_running(fw, mainLoop.loopblks.mainLoop));

    // by here, scheduleAquariumUpdate should be locked
    exec.task_finish(fw, aquarium.tasks.schedule, true, {.cancel = false});
    exec.wait(fw);
    ASSERT_TRUE(exec.is_running(fw, mainLoop.loopblks.mainLoop));
    ASSERT_EQ(rAquariumFish.fishCount, 8); // sharks ate 2 fish

    // repeat
    exec.task_finish(fw, aquarium.tasks.schedule, true, {.cancel = false});
    exec.wait(fw);
    ASSERT_TRUE(exec.is_running(fw, mainLoop.loopblks.mainLoop));
    ASSERT_EQ(rAquariumFish.fishCount, 6); // sharks ate 2 more fish

    // repeat
    exec.task_finish(fw, aquarium.tasks.schedule, true, {.cancel = false});
    exec.wait(fw);
    ASSERT_TRUE(exec.is_running(fw, mainLoop.loopblks.mainLoop));
    ASSERT_EQ(rAquariumFish.fishCount, 4);

    // exit
    exec.task_finish(fw, aquarium.tasks.schedule, true, {.cancel = true});
    exec.wait(fw);
    ASSERT_EQ(rAquariumFish.fishCount, 4);
    ASSERT_FALSE(exec.is_running(fw, mainLoop.loopblks.mainLoop));
}

//-----------------------------------------------------------------------------

// Test 2: Task order, cases such as "all tasks that write to this container must run BEFORE all
//         tasks that read it."

enum class EStgIntermediate {
    Modify      = 0,
    Schedule    = 1,
    Read        = 2,
    Clear       = 3
};
osp::PipelineTypeInfo const gc_infoForEStgIntermediate{
    .debugName = "Intermediate container",
    .stages = {{
        { .name = "Modify",                         },
        { .name = "Schedule",   .isSchedule = true  },
        { .name = "Read",       .useCancel  = true  },
        { .name = "Clear",      .useCancel  = true  }
    }},
    .initialStage = osp::StageId{0}
};

struct FIMultiStepProcess {
    struct LoopBlockIds {
        LoopBlockId mainLoop;
    };
    struct DataIds {
        DataId vecA;
        DataId vecB;
        DataId vecC;
        DataId vecD;
    };
    struct Pipelines {
        PipelineDef<EStgIntermediate> processA{"processA"};
        PipelineDef<EStgIntermediate> processB{"processB"};
        PipelineDef<EStgIntermediate> processC{"processC"};
        PipelineDef<EStgIntermediate> processD{"processD"};
    };
    struct TaskIds {
        TaskId blockSchedule;
        TaskId last;
    };
};

FeatureDef const ftrProcess = feature_def("Process", [] (
        FeatureBuilder          &rFB,
        Implement<FIMultiStepProcess>   process)
{
    rFB.data_emplace< std::vector<int> >(process.di.vecA);
    rFB.data_emplace< std::vector<int> >(process.di.vecB);
    rFB.data_emplace< std::vector<int> >(process.di.vecC);
    rFB.data_emplace< std::vector<int> >(process.di.vecD);

    rFB.pipeline(process.pl.processA).parent(process.loopblks.mainLoop);
    rFB.pipeline(process.pl.processB).parent(process.loopblks.mainLoop);
    rFB.pipeline(process.pl.processC).parent(process.loopblks.mainLoop);
    rFB.pipeline(process.pl.processD).parent(process.loopblks.mainLoop);

    rFB.task(process.tasks.blockSchedule)
        .name       ("Schedule main loop")
        .schedules  ({process.loopblks.mainLoop})
        .ext_finish (true);

    auto const scheduleFunc = [] (std::vector<int> const &vec) noexcept -> TaskActions
    {
        return {.cancel = vec.empty()};
    };

    auto const copyFunc = [] (std::vector<int> const &rFrom, std::vector<int> &rTo) noexcept
    {
        rTo.assign(rFrom.begin(), rFrom.end());
    };

    auto const clearFunc = [] (std::vector<int> &rVec) noexcept
    {
        rVec.clear();
    };

    rFB.task().name("Schedule A").schedules(process.pl.processA).args({process.di.vecA}).func(scheduleFunc);
    rFB.task().name("Schedule B").schedules(process.pl.processB).args({process.di.vecB}).func(scheduleFunc);
    rFB.task().name("Schedule C").schedules(process.pl.processC).args({process.di.vecC}).func(scheduleFunc);
    rFB.task().name("Schedule D").schedules(process.pl.processD).args({process.di.vecD}).func(scheduleFunc);

    rFB.task()
        .name       ("Copy A to B")
        .sync_with  ({process.pl.processA(EStgIntermediate::Read), process.pl.processB(EStgIntermediate::Modify)})
        .args       ({process.di.vecA, process.di.vecB})
        .func       (copyFunc);

    rFB.task()
        .name       ("Copy B to C")
        .sync_with  ({process.pl.processB(EStgIntermediate::Read), process.pl.processC(EStgIntermediate::Modify)})
        .args       ({process.di.vecB, process.di.vecC})
        .func       (copyFunc);

    rFB.task()
        .name       ("Copy C to D")
        .sync_with  ({process.pl.processC(EStgIntermediate::Read), process.pl.processD(EStgIntermediate::Modify)})
        .args       ({process.di.vecC, process.di.vecD})
        .func       (copyFunc);

    rFB.task().name("Clear A").sync_with({process.pl.processA(EStgIntermediate::Clear)}).args({process.di.vecA}).func(clearFunc);
    rFB.task().name("Clear B").sync_with({process.pl.processB(EStgIntermediate::Clear)}).args({process.di.vecB}).func(clearFunc);
    rFB.task().name("Clear C").sync_with({process.pl.processC(EStgIntermediate::Clear)}).args({process.di.vecC}).func(clearFunc);

    // Last task will stop
    rFB.task().name("Check D").sync_with({process.pl.processD(EStgIntermediate::Clear)}).args({process.di.vecC}).ext_finish(true);
});


TEST(Framework, Process)
{
    register_pltype_info();

    auto pSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    osp::Logger_t logger = std::make_shared<spdlog::logger>("executor", pSink);
    osp::set_thread_logger(logger);

    Framework fw;

    ContextId const ctx = fw.m_contextIds.create();

    ContextBuilder cb{ctx, {}, fw};
    cb.add_feature(ftrProcess);
    ContextBuilder::finalize(std::move(cb));

    auto const process  = fw.get_interface<FIMultiStepProcess>(ctx);

    auto       &rVecA   = fw.data_get<std::vector<int>>(process.di.vecA);
    auto       &rVecD   = fw.data_get<std::vector<int>>(process.di.vecD);

    osp::exec::SinglethreadFWExecutor exec;
    exec.m_log = logger;
    exec.load(fw);

    exec.wait(fw);
    ASSERT_FALSE(exec.is_running(fw, process.loopblks.mainLoop));
    exec.task_finish(fw, process.tasks.blockSchedule, true, {.cancel = false});
    exec.wait(fw);
    ASSERT_FALSE(exec.is_running(fw, process.loopblks.mainLoop));

    rVecA = {1, 2, 3};

    exec.task_finish(fw, process.tasks.blockSchedule, true, {.cancel = false});
    exec.wait(fw);

    ASSERT_TRUE(exec.is_running(fw, process.loopblks.mainLoop));
}

//-----------------------------------------------------------------------------

// Test 3: Nested loop. Keep running an inner loop until a condition is met; in this example, a
//         process that manipulates a value so it matches a setpoint, like a control system.
//         Tasks in the inner loop must be able to sync with pipelines in the outer loop.

struct NestedLoopData
{
    int setpoint    {0};

    int value       {0};
    int error       {0};
    int command     {0}; // possible values: -1, 0, 1
};

struct FINestedLoop {
    struct LoopBlockIds {
        LoopBlockId outer;
        LoopBlockId inner;
    };
    struct DataIds {
        DataId data;
    };
    struct Pipelines {
        PipelineDef<EStgContinuous>     value       {"value"};
        PipelineDef<EStgContinuous>     setpoint    {"setpoint"};

        PipelineDef<EStgContinuous>     valueInner  {"valueInner"};
        PipelineDef<EStgContinuous>     error       {"error"};
        PipelineDef<EStgIntermediate>   command     {"command"};
    };
    struct TaskIds {
        TaskId outerSchedule;
        TaskId innerSchedule;
        TaskId last;
    };
};

FeatureDef const ftrNestedLoop = feature_def("NestedLoop", [] (
        FeatureBuilder          &rFB,
        Implement<FINestedLoop>   nestedLoop)
{
    rFB.data_emplace<NestedLoopData>(nestedLoop.di.data);

    rFB.loopblk(nestedLoop.loopblks.inner).parent(nestedLoop.loopblks.outer);

    rFB.pipeline(nestedLoop.pl.value)       .parent(nestedLoop.loopblks.outer);
    rFB.pipeline(nestedLoop.pl.setpoint)    .parent(nestedLoop.loopblks.outer);
    rFB.pipeline(nestedLoop.pl.valueInner)  .parent(nestedLoop.loopblks.inner).initial_stage(EStgContinuous::Read);
    rFB.pipeline(nestedLoop.pl.error)       .parent(nestedLoop.loopblks.inner);
    rFB.pipeline(nestedLoop.pl.command)     .parent(nestedLoop.loopblks.inner);

    rFB.task(nestedLoop.tasks.outerSchedule)
        .name       ("Schedule main loop")
        .schedules  ({nestedLoop.loopblks.outer})
        .ext_finish (true);


    rFB.task(nestedLoop.tasks.innerSchedule)
        .name       ("Schedule inner loop")
        .schedules  ({nestedLoop.loopblks.inner})
        .args       ({      nestedLoop.di.data})
        .func       ([] (NestedLoopData& rData) -> TaskActions
    {
        return {.cancel = rData.setpoint == rData.value};
    });

    rFB.task()
        .name       ("Calculate error")
        .sync_with  ({nestedLoop.pl.value(EStgContinuous::Modify), nestedLoop.pl.valueInner(EStgContinuous::Read), nestedLoop.pl.setpoint(EStgContinuous::Read), nestedLoop.pl.error(EStgContinuous::Modify)})
        .args       ({      nestedLoop.di.data})
        .func       ([] (NestedLoopData& rData)
    {
        rData.error = rData.setpoint - rData.value;
    });

    rFB.task()
        .name       ("Calculate command")
        .sync_with  ({nestedLoop.pl.error(EStgContinuous::Read), nestedLoop.pl.command(EStgIntermediate::Modify)})
        .args       ({      nestedLoop.di.data})
        .func       ([] (NestedLoopData& rData)
    {
        rData.command = (0 < rData.error) - (rData.error < 0);
    });

    rFB.task()
        .name       ("Schedule command")
        .schedules  ({nestedLoop.pl.command})
        .args       ({      nestedLoop.di.data})
        .func       ([] (NestedLoopData& rData) -> TaskActions
    {
        return {.cancel = rData.command == 0};
    });

    rFB.task()
        .name       ("Apply command")
        .sync_with  ({nestedLoop.pl.command(EStgIntermediate::Read), nestedLoop.pl.valueInner(EStgContinuous::Modify), nestedLoop.pl.value(EStgContinuous::Modify)})
        .args       ({      nestedLoop.di.data})
        .func       ([] (NestedLoopData& rData)
    {
        rData.value += rData.command;
    });
});

TEST(Framework, NestedLoop)
{
    register_pltype_info();

    auto pSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    osp::Logger_t logger = std::make_shared<spdlog::logger>("executor", pSink);
    osp::set_thread_logger(logger);

    Framework fw;

    ContextId const ctx = fw.m_contextIds.create();

    ContextBuilder cb{ctx, {}, fw};
    cb.add_feature(ftrNestedLoop);
    ContextBuilder::finalize(std::move(cb));

    auto const nestedLoop = fw.get_interface<FINestedLoop>(ctx);
    auto       &rData     = fw.data_get<NestedLoopData>(nestedLoop.di.data);

    osp::exec::SinglethreadFWExecutor exec;
    exec.m_log = logger;
    exec.load(fw);

    exec.wait(fw);

    ASSERT_FALSE(exec.is_running(fw, nestedLoop.loopblks.outer));
    ASSERT_FALSE(exec.is_running(fw, nestedLoop.loopblks.inner));

    exec.task_finish(fw, nestedLoop.tasks.outerSchedule);
    exec.wait(fw);

    ASSERT_FALSE(exec.is_running(fw, nestedLoop.loopblks.outer));
    ASSERT_FALSE(exec.is_running(fw, nestedLoop.loopblks.inner));

    rData.setpoint = 10;
    exec.task_finish(fw, nestedLoop.tasks.outerSchedule);
    exec.wait(fw);
    ASSERT_EQ(rData.value, 10);
    ASSERT_FALSE(exec.is_running(fw, nestedLoop.loopblks.outer));
    ASSERT_FALSE(exec.is_running(fw, nestedLoop.loopblks.inner));

    rData.setpoint = -5;
    exec.task_finish(fw, nestedLoop.tasks.outerSchedule);

    exec.wait(fw);
    ASSERT_EQ(rData.value, -5);
    ASSERT_FALSE(exec.is_running(fw, nestedLoop.loopblks.outer));
    ASSERT_FALSE(exec.is_running(fw, nestedLoop.loopblks.inner));
}

//-----------------------------------------------------------------------------


void register_pltype_info()
{
    auto &rPltypeReg = PipelineTypeIdReg::instance();
    rPltypeReg.assign_pltype_info<EStgContinuous>   (gc_infoForEStgContinuous);
    rPltypeReg.assign_pltype_info<EStgOptionalPath> (gc_infoForEStgOptionalPath);
    rPltypeReg.assign_pltype_info<EStgIntermediate> (gc_infoForEStgIntermediate);
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

