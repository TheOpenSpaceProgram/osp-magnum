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

#include <cstring>
#include <entt/core/any.hpp>

#include <Corrade/Containers/ArrayViewStl.h>

#include <utility>

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
    requires (CFuncPtr< std::decay_t<decltype(+CALLABLE_T{})> >)
struct as_function_ptr<CALLABLE_T> { using type = std::decay_t<decltype(+CALLABLE_T{})>; };

template <typename CALLABLE_T>
using as_function_ptr_t = typename as_function_ptr<CALLABLE_T>::type;

template<typename T>
concept CStatelessLambda = requires () { typename as_function_ptr_t<T>; } && ! CFuncPtr<T>;

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
    std::string                  name;
    std::size_t                  dataCount{};
    std::vector<PipelineDefInfo> pipelines;
};

/**
 * @brief Global information on known existing Feature Interfaces
 *
 * Each unique type of Feature Interface is assigned an ID at runtime.
 *
 * Feature interfaces can be created with a compile time struct (see any feature_interface.h file
 * for examples).
 *
 * Feature interfaces can be registered at runtime by manually calling register_type().
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
        std::vector<PipelineDefInfo> info;
        if constexpr ( ! std::is_empty_v<typename FI_T::Pipelines> )
        {
            typename FI_T::Pipelines const pl; // default-init sets names and stuff
            auto const members = arrayCast<PipelineDefBlank_t const>(  arrayCast<std::byte const>( arrayView(&pl, 1) )  );

            info.reserve(members.size());
            for (PipelineDefBlank_t const& plDef : members)
            {
                info.push_back(PipelineDefInfo{.name = plDef.m_name, .type = plDef.m_type});
            }
        }
        // else, (typename FI_T::Pipelines) is an empty struct. sizeof(EmptyStruct) is one byte,
        // not zero, which confuses arrayCast.

        return instance().register_type({
                .name      = std::string{entt::type_id<FI_T>().name()},
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

/**
 * @brief Running instance of a feature, added as part of a context.
 *
 * These are created when running ContextBuilder::add_feature(...).
 */
struct FeatureSession
{
    std::vector<FIInstanceId>       finterDependsOn;
    std::vector<FIInstanceId>       finterImplements;
    std::vector<TaskId>             tasks;
};

struct FeatureContext
{
    KeyedVec<FITypeId, FIInstanceId> finterSlots;
    std::vector<FSessionId>          sessions;
};

/**
 * @brief Data for an entire application. Stores arbitrary data, tasks with dependencies and flow
 *        control, and means of managing features in a dynamic way.
 *
 * Requires a separate executor to run.
 */
struct Framework
{
    void resize_ctx()
    {
        m_contextData.resize(m_contextIds.capacity());
        for (ContextId const ctxId : m_contextIds)
        {
            m_contextData[ctxId].finterSlots.resize(FITypeInfoRegistry::size());
        }
    }

    [[nodiscard]] inline FIInstanceId get_interface_id(FITypeId type, ContextId ctx) noexcept
    {
        if (    ! ctx.has_value()
             || ! type.has_value()
             || ctx.value >= m_contextData.size() )
        {
            return {};
        }
        FeatureContext const &rFtrCtx = m_contextData[ctx];

        if ( type.value >= rFtrCtx.finterSlots.size() )
        {
            return {};
        }

        return rFtrCtx.finterSlots[type];
    }

    template<CFeatureInterfaceDef FI_T>
    [[nodiscard]] inline FIInstanceId get_interface_id(ContextId ctx) noexcept
    {
        FITypeId const type = FITypeInfoRegistry::get_or_register<FI_T>();
        return get_interface_id(type, ctx);
    }

    template<CFeatureInterfaceDef FI_T, typename TAG_T = void>
    [[nodiscard]] FInterfaceShorthand<FI_T, TAG_T> get_interface(ContextId ctx) noexcept
    {
        FIInstanceId const fiId = get_interface_id<FI_T>(ctx);
        FInterfaceShorthand<FI_T, TAG_T> out { .id = fiId, .ctx = ctx };

        if (fiId.has_value())
        {
            // Access struct members of out.pl and out.di as if they're arrays, in order to write
            // DataIds and PipelineIds to them.

            //Using this reference to avoid strict aliasing UB : https://gist.github.com/shafik/848ae25ee209f698763cffee272a58f8
            //Apparently the memcpy calls get optimized away. 

            FeatureInterface const &rInterface = m_fiinstData[fiId];

            if constexpr ( ! std::is_empty_v<typename FI_T::Pipelines> )
            {
                auto const plMembers = arrayCast<PipelineDefBlank_t>(  arrayCast<std::byte>( arrayView(&out.pl, 1) )  );
                for (std::size_t i = 0; i < plMembers.size(); ++i)
                {
                    std::memcpy(&plMembers[i].m_value,  &rInterface.pipelines[i], sizeof(PipelineId));
                }
            }

            if constexpr ( ! std::is_empty_v<typename FI_T::DataIds> )
            {
                auto const diMembers = arrayCast<DataId>(  arrayCast<std::byte>( arrayView(&out.di, 1) )  );
                for (std::size_t i = 0; i < diMembers.size(); ++i)
                {
                    std::memcpy(&diMembers[i], &rInterface.data[i], sizeof(DataId));
                }
            }
        }

        return out;
    }

    template<typename T>
    [[nodiscard]] T& data_get(DataId const dataId) noexcept
    {
        return entt::any_cast<T&>(m_data[dataId]);
    }

    template<typename T, typename ... ARGS_T>
    T& data_emplace(DataId const dataId, ARGS_T &&...args) noexcept
    {
        entt::any &rData = m_data[dataId];
        rData.emplace<T>(std::forward<ARGS_T>(args) ...);
        return entt::any_cast<T&>(rData);
    }

    void close_context(ContextId ctx);

    Tasks                                       m_tasks;
    KeyedVec<TaskId, TaskImpl>                  m_taskImpl;

    lgrn::IdRegistryStl<DataId>                 m_dataIds;
    KeyedVec<DataId, entt::any>                 m_data;

    lgrn::IdRegistryStl<ContextId>              m_contextIds;
    KeyedVec< ContextId, FeatureContext >       m_contextData;

    lgrn::IdRegistryStl<FIInstanceId>           m_fiinstIds;
    KeyedVec<FIInstanceId, FeatureInterface>    m_fiinstData;

    lgrn::IdRegistryStl<FSessionId>             m_fsessionIds;
    KeyedVec<FSessionId, FeatureSession>        m_fsessionData;

};

class IExecutor
{
public:

    virtual void load(Framework &rFW) = 0;

    virtual void run(Framework &rFW, osp::PipelineId pipeline) = 0;

    virtual void signal(Framework &rFW, osp::PipelineId pipeline) = 0;

    virtual void wait(Framework &rFW) = 0;

    virtual bool is_running(Framework const &rFW) = 0;
};


} // namespace osp::fw
