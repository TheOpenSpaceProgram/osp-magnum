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

#include "tasks.h"

#include "../core/array_view.h"
#include "../core/strong_id.h"
#include "../core/global_id.h"

#include "../core/keyed_vector.h"
#include "../tasks/tasks.h"

#include <entt/core/any.hpp>

#include <Corrade/Containers/ArrayViewStl.h>

#include <utility>
#include <unordered_map>

namespace osp::fw
{

//-----------------------------------------------------------------------------

// METAPROGRAMMING LIBRARY

/**
 * @brief Stupid/Shitty tuple. Features FIFO capabilities on its parameter pack.
 *
 * Unlike std::tuple, this type does not store data
 */
template<typename HEAD_T = void, typename ... TAIL_T>
struct Stuple
{
    using Head_t        = HEAD_T;

    /**
     * Stuple<int, char>::appended_t<float> == Stuple<int, char, float>
     */
    template<typename APPEND_T>
    using appended_t    = Stuple<HEAD_T, TAIL_T..., APPEND_T>;

    /**
     * Stuple<int, char>::decapitated_t == Stuple<char>
     */
    using decapitated_t = Stuple<TAIL_T...>; // TAIL_T[0] becomes the new head
};

/**
 * @brief Specialization for Empty Stuple
 *
 * tricky C++: empty stuple "Stuple<>" will evaluate to this, since "= void" is set default above
 */
template<>
struct Stuple<void>
{
    // Don't accidentally make Stuple<void, APPEND_T>
    template<typename APPEND_T>
    using appended_t = Stuple<APPEND_T>;

    // No head, can't be decapitated
};

template<typename T>
concept CFuncPtr = std::is_function_v< std::remove_pointer_t<T> >;

// Dummy function used with decltype() to extract arguments from a function pointer
template<typename RETURN_T, typename ... ARGS_T>
constexpr Stuple<ARGS_T...> stuple_from_func_params_dummy([[maybe_unused]] RETURN_T(*func)(ARGS_T...));

template <CFuncPtr FUNC_T>
using stuple_from_func_params = decltype(stuple_from_func_params_dummy(FUNC_T{}));

template< typename IN_T, template <class> typename PRED_T, typename OUT_T = Stuple<> >
struct filter_parameter_pack
{
    using Head_t = typename IN_T::Head_t;
    static constexpr bool passed = PRED_T<Head_t>::value;

    using value = typename std::conditional_t< passed,
        // Filter passed. Remove Head_t from IN_T and add it to OUT_T for the next recursion step
        filter_parameter_pack<typename IN_T::decapitated_t, PRED_T, typename OUT_T::template appended_t<Head_t>>,
        // Filter Failed. Remove Head_t from IN_T for the next recursion step. OUT_T is unchanged
        filter_parameter_pack<typename IN_T::decapitated_t, PRED_T, OUT_T>  >::value;
};

template< template <class> typename PRED_T, typename OUT_T >
struct filter_parameter_pack<Stuple<>, PRED_T, OUT_T>
{
    using value = OUT_T; // No more inputs left, done filtering
};





template <typename CALLABLE_T>
struct as_function_ptr { };

// Convert lambda to function pointer using `+`, since C++ is the type of language where this shit
// SOMEHOW works. Function pointers are left untouched.
template <typename CALLABLE_T>
    requires (requires (CALLABLE_T t) { { +t } -> CFuncPtr; })
struct as_function_ptr<CALLABLE_T> { using type = decltype(+CALLABLE_T{}); };

template <typename CALLABLE_T>
using as_function_ptr_t = typename as_function_ptr<CALLABLE_T>::type;

template<typename T>
concept CStatelessLambda = requires () { typename as_function_ptr_t<T>; } && ! CFuncPtr<T>;

//-----------------------------------------------------------------------------

namespace move_this_to_a_unit_test
{

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



}; // move_this_to_a_unit_test



//-----------------------------------------------------------------------------


using ContextId         = StrongId<std::uint32_t, struct DummyForContextId>;


/**
 * @brief (Feature Interface) Type Id
 */
using FITypeId          = StrongId<std::uint32_t, struct DummyForFITypeId>;

/**
 * @brief (Feature Interface) Instance Id
 */
using FIInstanceId      = StrongId<std::uint32_t, struct DummyForFIInstanceId>;

/**
 * @brief Feature Session Id
 */
using FSessionId        = StrongId<std::uint32_t, struct DummyForFSessionId>;


//-----------------------------------------------------------------------------

struct FIEmpty
{
    struct DataIds { };
    struct Pipelines { };
};

template<typename T>
concept CFeatureInterfaceDef = requires{ typename T::DataIds; typename T::Pipelines; };


struct FITypeInfo
{
    std::size_t                  dataCount;
    std::vector<PipelineDefInfo> pipelines;
};

/**
 * can be bound to a cpp type, but also registered at runtime
 */
class FITypeInfoRegistry
{
    using FITypeIdReg_t = GlobalIdReg<FITypeId>;

public:

    [[nodiscard]] static constexpr std::size_t size() noexcept
    {
      return FITypeIdReg_t::size();
    }

    [[nodiscard]] static FITypeInfoRegistry& instance() noexcept
    {
        static FITypeInfoRegistry instance;
        return instance;
    }

    [[nodiscard]] FITypeInfo const& info_for(FITypeId id) const noexcept { return m_registeredTypes[id]; }

    [[nodiscard]] FITypeId register_type(FITypeInfo info) noexcept
    {
        FITypeId const newId = FITypeIdReg_t::create();
        m_registeredTypes.resize(FITypeIdReg_t::size());
        m_registeredTypes[newId] = info;
        return newId;
    }

    template<CFeatureInterfaceDef FI_T>
    [[nodiscard]] static FITypeId get_or_register() noexcept
    {
        static FITypeId id = register_fi<FI_T>(); // called only once

        return id;
    }

private:

    template<CFeatureInterfaceDef FI_T>
    [[nodiscard]] static FITypeId register_fi() noexcept
    {
        typename FI_T::Pipelines const pl;
        auto const members = arrayCast<PipelineDefBlank_t const>(  arrayCast<std::byte const>( arrayView(&pl, 1) )  );

        std::vector<PipelineDefInfo> info;
        info.reserve(members.size());
        for (PipelineDefBlank_t const& plDef : members)
        {
            info.push_back(PipelineDefInfo{.name = plDef.m_name, .type = plDef.m_type});
        }

        return instance().register_type({
                .dataCount = sizeof(typename FI_T::DataIds) / sizeof(DataId),
                .pipelines = std::move(info) });
    }

    FITypeInfoRegistry() = default;

    KeyedVec<FITypeId, FITypeInfo> m_registeredTypes;
};

struct FeatureInterface
{
    std::vector<DataId>             data;
    std::vector<PipelineId>         pipelines;

    FITypeId                        type;
    ContextId                       context;
};


template<CFeatureInterfaceDef FI_T, typename TAG_T = void>
struct FInterfaceShorthand
{
    FIInstanceId id;
    ContextId    ctx;
    typename FI_T::DataIds   di;
    typename FI_T::Pipelines pl;
};

//-----------------------------------------------------------------------------

struct FeatureSession
{
    std::vector<FIInstanceId>       finterDependsOn;
    std::vector<FIInstanceId>       finterImplements;
    std::vector<TaskId>             tasks;
};

struct FeatureContext
{
    KeyedVec<FITypeId, FIInstanceId> finterSlots;
};

struct FrameworkModify
{
    struct Command
    {
        using Func_t = void(*)(entt::any);
        entt::any   userData;
        Func_t      func;
    };

    std::vector<Command> commands;
};

struct Framework
{
    void resize_ctx()
    {
        contextData.resize(contextIds.capacity());
        for (ContextId const ctxId : contextIds)
        {
            contextData[ctxId].finterSlots.resize(FITypeInfoRegistry::size());
        }
    }

    [[nodiscard]] inline FIInstanceId get_interface_id(FITypeId type, ContextId ctx) noexcept
    {
        return contextData[ctx].finterSlots[type];
    }

    template<CFeatureInterfaceDef FI_T>
    [[nodiscard]] inline FIInstanceId get_interface_id(ContextId ctx) noexcept
    {
        return contextData[ctx].finterSlots[FITypeInfoRegistry::get_or_register<FI_T>()];
    }

    template<CFeatureInterfaceDef FI_T, typename TAG_T = void>
    [[nodiscard]] FInterfaceShorthand<FI_T, TAG_T> get_interface_shorthand(ContextId ctx) noexcept
    {
        FIInstanceId const fiId = get_interface_id<FI_T>(ctx);
        FInterfaceShorthand<FI_T, TAG_T> out { .id = fiId, .ctx = ctx };

        if (fiId.has_value())
        {
            FeatureInterface const &rInterface = fiinstData[fiId];
            auto const plMembers = arrayCast<PipelineDefBlank_t>(  arrayCast<std::byte>( arrayView(&out.pl, 1) )  );
            for (std::size_t i = 0; i < plMembers.size(); ++i)
            {
                plMembers[i].m_value = rInterface.pipelines[i];
            }

            auto const diMembers = arrayCast<DataId>(  arrayCast<std::byte>( arrayView(&out.di, 1) )  );
            for (std::size_t i = 0; i < diMembers.size(); ++i)
            {
                diMembers[i] = rInterface.data[i];
            }
        }

        return out;
    }

    Tasks                                       tasks;
    KeyedVec<TaskId, TaskImpl>                  taskImpl;

    lgrn::IdRegistryStl<DataId>                 dataIds;
    KeyedVec<DataId, entt::any>                 data;

    lgrn::IdRegistryStl<ContextId>              contextIds;
    KeyedVec< ContextId, FeatureContext >       contextData;

    lgrn::IdRegistryStl<FIInstanceId>           fiinstIds;
    KeyedVec<FIInstanceId, FeatureInterface>    fiinstData;

    lgrn::IdRegistryStl<FSessionId>             fsessionIds;
    KeyedVec<FSessionId, FeatureSession>        fsessionData;

};

class IExecutor
{
public:

    virtual void load(Framework& rAppTasks) = 0;

    virtual void run(Framework& rAppTasks, osp::PipelineId pipeline) = 0;

    virtual void signal(Framework& rAppTasks, osp::PipelineId pipeline) = 0;

    virtual void wait(Framework& rAppTasks) = 0;

    virtual bool is_running(Framework const& rAppTasks) = 0;
};


} // namespace osp
