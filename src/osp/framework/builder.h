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
#pragma once

#include "framework.h"

#include "../core/array_view.h"

#include <entt/core/any.hpp>

#include <type_traits>
#include <utility>

#include <array>
#include <variant>

namespace osp::fw
{

/**
 * @brief Wrap a lambda with arbitrary arguments into a TaskImpl::Func_t function pointer
 *
 * TaskImpl::Func_t is `TaskActions(*)(WorkerContext, ArrayView<entt::any>) noexcept`
 *
 * Each entt::any argument from TaskImpl::Func_t will be casted and mapped 1-on-1 to each argument
 * of the lambda.
 *
 * WorkerContext lambda arguments are handled as a special case, and are given WorkerContext
 * directly from TaskImpl::Func_t instead of casting an entt::any, but will still use a slot in
 * the ArrayView. Prefer using an empty entt::any for this.
 *
 * If the given function's return value is TaskActions, then it will be forwarded as the return
 * value of the output TaskImpl::Func_t. Otherwise, the return value is ignored and the output
 * TaskImpl::Func_t returns an empty TaskActions.
 *
 * Example
 *
 * @code
 *
 * auto lambda = [] (int a, WorkerContext ctx, float b) { ... };
 * using LAMBDA_T = decltype(lambda);
 *
 * // `as_task_impl<LAMBDA_T>::value` is roughly equivalent to a function pointer to...
 * TaskActions task_impl_out(WorkerContext ctx, ArrayView<entt::any> topData) noexcept
 * {
 *     LAMBDA_T{} ( entt::any_cast<int>(topData[0]), ctx, entt::any_cast<float>(topData[2]) );
 *     return TaskActions{};
 * }
 *
 * // This be called with...
 * std::vector<entt::any> args = { {69}, {}, {69.69f} };
 * task_impl_out(WorkerContext{}, args);
 * @endcode
 */
template<CStatelessLambda FUNCTOR_T>
struct as_task_impl
{
private:

    template<typename T>
    static constexpr decltype(auto) cast_argument([[maybe_unused]] ArrayView<entt::any> args,  [[maybe_unused]] WorkerContext ctx,  [[maybe_unused]] std::size_t index)
    {
        if constexpr (std::is_same_v<T, WorkerContext>)
        {
            return ctx;
        }
        else
        {
            entt::any& argIn = args[index];

            [[maybe_unused]] entt::type_info const& expected = entt::type_id<T>();
            [[maybe_unused]] entt::type_info const& functor  = entt::type_id<FUNCTOR_T>();
            LGRN_ASSERTMV(argIn.type().hash() == expected.hash(),
                          "Incorrect type passed as entt::any argument",
                          expected.name(),
                          argIn.type().name(),
                          index,
                          functor.name());

            return entt::any_cast<T&>(argIn);
        }
    }

    template<typename RETURN_T, typename ... ARGS_T>
    struct with_args
    {
        template<std::size_t ... INDEX>
        static constexpr RETURN_T call(ArrayView<entt::any> args, WorkerContext ctx, [[maybe_unused]] std::index_sequence<INDEX...> indices) noexcept
        {
            return FUNCTOR_T{}(cast_argument<ARGS_T>(args, ctx, INDEX) ...);
        }

        static TaskActions task_impl_out([[maybe_unused]] WorkerContext ctx, ArrayView<entt::any> args) noexcept
        {
            LGRN_ASSERTMV(args.size() >= sizeof...(ARGS_T), "Incorrect number of arguments", args.size(), sizeof...(ARGS_T));

            if constexpr (std::is_same_v<RETURN_T, TaskActions>)
            {
                return call(args, ctx, std::make_index_sequence<sizeof...(ARGS_T)>{});
            }
            else
            {
                call(args, ctx, std::make_index_sequence<sizeof...(ARGS_T)>{});
                return {};
            }
        }
    };

    template<typename RETURN_T, typename ... ARGS_T>
    static constexpr with_args<RETURN_T, ARGS_T...> dummy([[maybe_unused]] RETURN_T(*func)(ARGS_T...));

    using with_args_spec = decltype( dummy(as_function_ptr_t<FUNCTOR_T>{}) );

public:
    static inline constexpr TaskImpl::Func_t value = &with_args_spec::task_impl_out;
};

template<CStatelessLambda FUNCTOR_T>
inline constexpr TaskImpl::Func_t as_task_impl_v = as_task_impl<FUNCTOR_T>::value;


struct TaskRef
{
    template<typename RANGE_T>
    TaskRef& add_edges(std::vector<TplTaskPipelineStage>& rContainer, RANGE_T const& add)
    {
        for (auto const [pipeline, stage] : add)
        {
            rContainer.push_back({
                .task     = taskId,
                .pipeline = pipeline,
                .stage    = stage
            });
        }
        return *this;
    }

    TaskRef& name(std::string_view debugName)
    {
        rFramework.taskImpl.resize(rFramework.tasks.m_taskIds.capacity());
        rFramework.taskImpl[taskId].debugName = debugName;
        return *this;
    }

    TaskRef& args(std::initializer_list<DataId> args)
    {
        rFramework.taskImpl.resize(rFramework.tasks.m_taskIds.capacity());
        rFramework.taskImpl[taskId].args = args;
        return *this;
    }

    TaskRef& run_on(TplPipelineStage const tpl) noexcept
    {
        rFramework.tasks.m_taskRunOn.resize(rFramework.tasks.m_taskIds.capacity());
        rFramework.tasks.m_taskRunOn[taskId] = tpl;

        return *this;
    }

    TaskRef& schedules(TplPipelineStage const tpl) noexcept
    {
        rFramework.tasks.m_pipelineControl[tpl.pipeline].scheduler = taskId;

        return run_on(tpl);
    }

    TaskRef& sync_with(ArrayView<TplPipelineStage const> const specs) noexcept
    {
        return add_edges(rFramework.tasks.m_syncWith, specs);
    }

    TaskRef& sync_with(std::initializer_list<TplPipelineStage const> specs) noexcept
    {
        return add_edges(rFramework.tasks.m_syncWith, specs);
    }

    template<typename FUNC_T>
    TaskRef& func(FUNC_T&& funcArg)
    {
        rFramework.taskImpl.resize(rFramework.tasks.m_taskIds.capacity());
        rFramework.taskImpl[taskId].func = as_task_impl_v<FUNC_T>;
        return *this;
    }

    TaskRef& func_raw(TaskImpl::Func_t func)
    {
        rFramework.taskImpl.resize(rFramework.tasks.m_taskIds.capacity());
        rFramework.taskImpl[taskId].func = func;
        return *this;
    }

    TaskId          taskId;
    Framework       &rFramework;

}; // struct TaskRef

template <typename ENUM_T>
struct PipelineRef
{

    constexpr operator PipelineId() noexcept
    {
        return pipelineId;
    }

    PipelineRef& parent(PipelineId const parent)
    {
        rFramework.tasks.m_pipelineParents[pipelineId] = parent;
        return static_cast<PipelineRef&>(*this);
    }

    PipelineRef& parent_with_schedule(PipelineId const parent)
    {
        rFramework.tasks.m_pipelineParents[pipelineId] = parent;

        constexpr ENUM_T const scheduleStage = stage_schedule(ENUM_T{0});
        static_assert(scheduleStage != lgrn::id_null<ENUM_T>(), "Pipeline type has no schedule stage");

        TaskId const scheduler = rFramework.tasks.m_pipelineControl[parent].scheduler;
        LGRN_ASSERTM(scheduler != lgrn::id_null<TaskId>(), "Parent Pipeline has no scheduler task");

        rFramework.tasks.m_syncWith.push_back({
            .task     = scheduler,
            .pipeline = pipelineId,
            .stage    = StageId(scheduleStage)
        });

        return static_cast<PipelineRef&>(*this);
    }

    PipelineRef& loops(bool const loop)
    {
        rFramework.tasks.m_pipelineControl[pipelineId].isLoopScope = loop;
        return *this;
    }

    PipelineRef& wait_for_signal(ENUM_T stage)
    {
        rFramework.tasks.m_pipelineControl[pipelineId].waitStage = StageId(stage);
        return *this;
    }

    PipelineId      pipelineId;
    Framework       &rFramework;

}; // struct TaskRef

/**
 * @brief builder passed to feature def setup functions
 *
 * containing info about the current feature being built.
 */
struct FeatureBuilder
{
    TaskRef task()
    {
        TaskId const taskId = rFramework.tasks.m_taskIds.create();
        rFramework.taskImpl.resize(rFramework.tasks.m_taskIds.capacity());
        rSession.tasks.push_back(taskId);
        return task(taskId);
    };

    [[nodiscard]] constexpr TaskRef task(TaskId const taskId) noexcept
    {
        return { taskId, rFramework };
    }

    [[nodiscard]] entt::any& data(DataId const dataId) noexcept
    {
        return rFramework.data[dataId];
    }

    template <typename ENUM_T>
    [[nodiscard]] constexpr PipelineRef<ENUM_T> pipeline(PipelineDef<ENUM_T> pipelineDef) noexcept
    {
        return PipelineRef<ENUM_T>{ pipelineDef.m_value, rFramework };
    }

    Framework       &rFramework;
    FeatureSession  &rSession;
    FSessionId      sessionId;
    ContextId       ctx;
};


struct FeatureDef
{
    using SetupFunc_t = void(*)(FeatureBuilder&, entt::any) noexcept;

    enum class ERelationType : std::uint8_t { DependOn, Implement };

    struct FIRelationship
    {
        FITypeId            subject;
        ERelationType       type;
        bool                optional;
    };

    SetupFunc_t func;
    std::initializer_list<FIRelationship> relationships;
};


//-----------------------------------------------------------------------------


struct TagDependOn {};
struct TagImplement {};

template<CFeatureInterfaceDef FI_T>
using DependOn = FInterfaceShorthand<FI_T, TagDependOn>;

template<CFeatureInterfaceDef FI_T>
using Implement = FInterfaceShorthand<FI_T, TagImplement>;



/**
 * @brief Compile-time predicate for filter_parameter_pack to only allow FeatureDefSetupArg
 */
template<typename T>
struct is_setup_arg : std::false_type { };

template<typename FI_T, typename TAG_T>
struct is_setup_arg<FInterfaceShorthand<FI_T, TAG_T>> : std::true_type {};


static_assert( ! is_setup_arg<float>::value );
static_assert(   is_setup_arg< DependOn<FIEmpty> >::value );
static_assert(   is_setup_arg< Implement<FIEmpty> >::value );


//-----------------------------------------------------------------------------


template<CFeatureInterfaceDef FI_T>
inline FeatureDef::FIRelationship relations_from_params_aux(DependOn<FI_T>)
{
    return { .subject    = FITypeInfoRegistry::instance().get_or_register<FI_T>(),
             .type       = FeatureDef::ERelationType::DependOn,
             .optional   = false };
}

template<CFeatureInterfaceDef FI_T>
inline FeatureDef::FIRelationship relations_from_params_aux(Implement<FI_T>)
{
    return { .subject    = FITypeInfoRegistry::instance().get_or_register<FI_T>(),
             .type       = FeatureDef::ERelationType::Implement,
             .optional   = false };
}

/**
 * @brief Get static initializer_list of FeatureDef::FIRelationship from a
 *        Stuple< DependOn<...>/Implement<...> ... >
 */
template<typename ... ARGS_T>
inline std::initializer_list<FeatureDef::FIRelationship> relations_from_params(Stuple<ARGS_T...> _)
{
    static std::initializer_list<FeatureDef::FIRelationship> const asdf
            = { relations_from_params_aux(ARGS_T{}) ... };
    return asdf;
}


//-----------------------------------------------------------------------------

// determine what gets passed into the feature def setup function
//
// auto const ftrExample = feature_def( /* SETUP FUNCTION */
//         [] (FeatureBuilder& a, Implement<FIFoo> foo, DependOn<FIBar> bar)
// {
//     std::cout << "ho!\n";
// });ContextId
//
// The real setup function is `using SetupFunc_t = void(*)(FeatureBuilder&)` and can't really
// support the Implement/DependOn stuff directly. So here, we create a new function using template
// magic that fits the SetupFunc_t signature and does logic to fill the Implement/DependOn args.
//
// After extracting function arg types, the example will end up being called like:
//
// func( call_setup_args_aux< FeatureBuilder& >(rBuilder),
//       call_setup_args_aux< Implement<FIFoo> >(rBuilder),
//       call_setup_args_aux< DependOn<FIBar> >(rBuilder) )
//
//
template<typename T>
struct Tag { };

template<typename T>
inline int call_setup_args_aux(Tag<T>, FeatureBuilder &rBuilder, entt::any const&)
{
    static_assert("unsupported type");
    return 0xdeadbeef;
}

inline FeatureBuilder& call_setup_args_aux(Tag<FeatureBuilder&>, FeatureBuilder &rBuilder, entt::any const&)
{
    return rBuilder;
}

inline entt::any call_setup_args_aux(Tag<entt::any>, FeatureBuilder &rBuilder, entt::any const& setupData)
{
    return std::move(setupData);
}

template<typename FI_T, typename TAG_T>
inline FInterfaceShorthand<FI_T, TAG_T> call_setup_args_aux(Tag< FInterfaceShorthand<FI_T, TAG_T> >, FeatureBuilder &rFB, entt::any const&)
{
    FInterfaceShorthand<FI_T, TAG_T> out = rFB.rFramework.get_interface_shorthand<FI_T, TAG_T>(rFB.ctx);
    LGRN_ASSERTM(out.id.has_value(), "this must have been added by feature_def(...) add_feature(...)");
    return out;
}

template<typename FUNC_T, typename ... FUNC_ARG_T>
constexpr FeatureDef::SetupFunc_t make_setup_func(FUNC_T, Stuple<FUNC_ARG_T...>)
{
    return [] (FeatureBuilder& a, entt::any setupData) noexcept -> void
    {
        // look through ParamsFiltered_t
        // make arguments?

        // call FUNC_T
        FUNC_T hey;

        hey(call_setup_args_aux(Tag<FUNC_ARG_T>{}, a, setupData) ...);
    };
};

//-----------------------------------------------------------------------------


/**
 * @brief define feature
 *
 * does witchcraft to...
 * * automatically implement or depend-on FeatureInterface (FI)
 *
 * @return
 */
template<CStatelessLambda FUNC_T>
FeatureDef feature_def(FUNC_T func)
{
    using FuncPtr_t        = as_function_ptr_t<FUNC_T>;
    using Params_t         = stuple_from_func_params<FuncPtr_t>;
    using ParamsFiltered_t = typename filter_parameter_pack<Params_t, is_setup_arg>::value;

    return { .func          = make_setup_func(func, Params_t{}),
             .relationships = relations_from_params(ParamsFiltered_t{}) };
}

//-----------------------------------------------------------------------------



struct ContextBuilder
{
    struct ErrDependencyNotFound
    {

    };

    struct ErrAlreadyImplemented
    {

    };


    using Error_t = std::variant<ErrDependencyNotFound, ErrAlreadyImplemented>;


    /**
     * @brief
     *
     * somewhat behaves like C++23/Rust and_then()
     */
    void add_feature(FeatureDef const &def, entt::any setupData = {}) noexcept
    {
        FeatureSession fsession;

        m_fw.resize_ctx();

        ContextId const mainCtx = m_contexts.front();
        FeatureContext &rCtx    = m_fw.contextData[mainCtx];

        auto const find_dependency = [this] (FITypeId type) -> FIInstanceId
        {
            for (ContextId const ctxId : m_contexts)
            {
                FeatureContext &rCtx    = m_fw.contextData[ctxId];
                FIInstanceId   &rFInter = rCtx.finterSlots[type];

                if (rFInter.has_value())
                {
                    return rFInter;
                }
            }
            return {};
        };

        for (FeatureDef::FIRelationship const& relation : def.relationships)
        {
            if (relation.type == FeatureDef::ERelationType::DependOn)
            {
                FIInstanceId const found = find_dependency(relation.subject);

                if (found.has_value())
                {
                    fsession.finterDependsOn.push_back(found);
                }
                else
                {
                    // Not found, ERROR!
                    m_errors.push_back(ErrDependencyNotFound{});
                }
            }
            else if (relation.type == FeatureDef::ERelationType::Implement)
            {
                FIInstanceId &rFInter = rCtx.finterSlots[relation.subject];

                if (rFInter.has_value())
                {
                    // Already exists, error
                    m_errors.push_back(ErrAlreadyImplemented{});
                }
                else
                {
                    FITypeInfo const& subjectInfo = m_rInfo.info_for(relation.subject);

                    rFInter = m_fw.fiinstIds.create();
                    m_fw.fiinstData.resize(m_fw.fiinstIds.capacity());
                    FeatureInterface &rFI = m_fw.fiinstData[rFInter];

                    rFI.context = mainCtx;
                    rFI.type    = relation.subject;
                    rFI.data     .resize(subjectInfo.dataCount);
                    rFI.pipelines.resize(subjectInfo.pipelines.size());

                    m_fw.dataIds.create(rFI.data.begin(), rFI.data.end());
                    auto const dataCapacity = m_fw.dataIds.capacity();
                    m_fw.data.resize(dataCapacity);

                    m_fw.tasks.m_pipelineIds.create(rFI.pipelines.begin(), rFI.pipelines.end());

                    auto const pipelineCapacity = m_fw.tasks.m_pipelineIds.capacity();
                    m_fw.tasks.m_pipelineInfo   .resize(pipelineCapacity);
                    m_fw.tasks.m_pipelineControl.resize(pipelineCapacity);
                    m_fw.tasks.m_pipelineParents.resize(pipelineCapacity, lgrn::id_null<PipelineId>());

                    for (std::size_t i = 0; i < subjectInfo.pipelines.size(); ++i)
                    {
                        PipelineId const plId = rFI.pipelines[i];
                        m_fw.tasks.m_pipelineInfo[plId].stageType = subjectInfo.pipelines[i].type;
                        m_fw.tasks.m_pipelineInfo[plId].name      = subjectInfo.pipelines[i].name;
                    }

                    fsession.finterImplements.push_back(rFInter);
                }
            }
        }

        if ( ! m_errors.empty() )
        {
            return; // in error state, do nothing
        }

        FeatureBuilder frog{
            .rFramework = m_fw,
            .rSession = fsession,
            .sessionId = {},
            .ctx = mainCtx };

        def.func(frog, std::move(setupData));
    }


    static void apply(ContextBuilder &&eat)
    {

    }


    std::vector<Error_t>    m_errors;
    std::vector<ContextId>  m_contexts;
    FITypeInfoRegistry      &m_rInfo = FITypeInfoRegistry::instance();
    Framework               &m_fw;

};

struct FrameworkBuilder
{



    Framework &m_fw;
};

} // namespace osp::fw


#if 0

namespace osp
{



/**
 * @brief Wrap a function with arbitrary arguments into a TopTaskFunc_t
 *
 * A regular TopTaskFunc_t accepts a ArrayView<entt::any> for passing data of
 * arbitrary types. This needs to be manually casted.
 *
 * wrap_args creates a wrapper function that will automatically cast the
 * entt::anys and call the underlying function.
 *
 * @param a[in]
 *
 * @return TopTaskFunc_t function pointer to wrapper
 */
template<typename FUNC_T>
constexpr TopTaskFunc_t wrap_args(FUNC_T funcArg)
{
    // TODO: wrap function pointers into a functor
    static_assert ( ! std::is_function_v<FUNC_T>, "Support for function pointers not yet implemented");

    auto const functionPtr = +funcArg;

    static_assert(std::is_function_v< std::remove_pointer_t<decltype(functionPtr)> >);

    // The original funcArg is discarded, but will be reconstructed in
    // wrap_args_trait::apply, as static_lambda_t is stored in the type
    return wrap_args_trait<FUNC_T>::unpack(functionPtr);
}

//-----------------------------------------------------------------------------

struct TopTaskBuilder;
struct TopTaskTaskRef;

struct TopTaskBuilderTraits
{
    using Builder_t     = TopTaskBuilder;
    using TaskRef_t     = TopTaskTaskRef;

    template <typename ENUM_T>
    using PipelineRef_t = PipelineRefBase<TopTaskBuilderTraits, ENUM_T>;
};


/**
 * @brief Convenient interface for building TopTasks
 */
struct TopTaskBuilder : public TaskBuilderBase<TopTaskBuilderTraits>
{
    TopTaskBuilder(Tasks& rTasks, TaskEdges& rEdges, TopTaskDataVec_t& rData)
     : TaskBuilderBase<TopTaskBuilderTraits>( {rTasks, rEdges} )
     , m_rData{rData}
    { }
    TopTaskBuilder(TopTaskBuilder const& copy) = delete;
    TopTaskBuilder(TopTaskBuilder && move) = default;

    TopTaskBuilder& operator=(TopTaskBuilder const& copy) = delete;

    TopTaskDataVec_t & m_rData;
};

struct TopTaskTaskRef : public TaskRefBase<TopTaskBuilderTraits>
{
    inline TopTaskTaskRef& name(std::string_view debugName);
    inline TopTaskTaskRef& args(std::initializer_list<TopDataId> dataUsed);

    template<typename FUNC_T>
    TopTaskTaskRef& func(FUNC_T&& funcArg);
    inline TopTaskTaskRef& func_raw(TopTaskFunc_t func);

    inline TopTaskTaskRef& important_deps_count(int value);

    template<typename CONTAINER_T>
    TopTaskTaskRef& push_to(CONTAINER_T& rContainer);
};

TopTaskTaskRef& TopTaskTaskRef::name(std::string_view debugName)
{
    m_rBuilder.m_rData.resize(m_rBuilder.m_rTasks.m_taskIds.capacity());
    m_rBuilder.m_rData[m_taskId].m_debugName = debugName;
    return *this;
}

TopTaskTaskRef& TopTaskTaskRef::args(std::initializer_list<TopDataId> dataUsed)
{
    m_rBuilder.m_rData.resize(m_rBuilder.m_rTasks.m_taskIds.capacity());
    m_rBuilder.m_rData[m_taskId].m_dataUsed = dataUsed;
    return *this;
}

template<typename FUNC_T>
TopTaskTaskRef& TopTaskTaskRef::func(FUNC_T&& funcArg)
{
    m_rBuilder.m_rData.resize(m_rBuilder.m_rTasks.m_taskIds.capacity());
    m_rBuilder.m_rData[m_taskId].m_func = wrap_args(funcArg);
    return *this;
}

TopTaskTaskRef& TopTaskTaskRef::func_raw(TopTaskFunc_t func)
{
    m_rBuilder.m_rData.resize(m_rBuilder.m_rTasks.m_taskIds.capacity());
    m_rBuilder.m_rData[m_taskId].m_func = func;
    return *this;
}

template<typename CONTAINER_T>
TopTaskTaskRef& TopTaskTaskRef::push_to(CONTAINER_T& rContainer)
{
    rContainer.push_back(m_taskId);
    return *this;
}


} // namespace osp


#endif
