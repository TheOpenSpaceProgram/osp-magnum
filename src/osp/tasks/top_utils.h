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
#pragma once

#include "tasks.h"
#include "builder.h"
#include "top_tasks.h"
#include "top_worker.h"

#include <entt/core/any.hpp>

#include <Corrade/Containers/ArrayViewStl.h>

#include <cassert>
#include <functional>
#include <type_traits>
#include <utility>

namespace osp
{
    
/**
 * @brief Reserves the first available space in topData after the indicated index.
 * 
 * @return The index of the reserved value, or the index after the end of topData if no slots are
 * available after the indicated index.
 */
[[nodiscard]] inline TopDataId top_reserve(
        ArrayView<entt::any> const topData, TopDataId current = 0)
{
    while ( bool(topData[current]) && (current < topData.size()) )
    {
        ++current;
    }

    if (current < topData.size())
    {
        topData[current].emplace<Reserved>();
    }

    return current;
}

/**
 * @brief Reserves a slot in topData for each item in the output range [destFirst, destLast),
 * writing the associated indices into the range.
 *
 * @return The largest index that was reserved in topData, or if topData is full,
 * the largest index in topData
 */
template <typename IT_T, typename ITB_T>
TopDataId top_reserve(
        ArrayView<entt::any> const topData, TopDataId current,
        IT_T destFirst, const ITB_T destLast)
{
    while (destFirst != destLast)
    {
        current = top_reserve(topData, current);

        if (current < topData.size())
        {
            *destFirst = current;
            ++current;
            std::advance(destFirst, 1);
        }
        else
        {
            break;
        }
    }

    return current;
}

/**
 * @brief Constructs an object of type T at the indicated index.
 *
 * @return A reference to the newly constructed value
 */
template <typename T, typename ... ARGS_T>
T& top_emplace(ArrayView<entt::any> const topData, TopDataId id, ARGS_T&& ... args)
{
    entt::any &rData = topData[std::size_t(id)];
    rData.emplace<T>(std::forward<ARGS_T>(args) ...);
    return entt::any_cast<T&>(rData);
}

/**
 * @brief Assigns the value at index id of topData to the value any
 *
 * @return A reference to the newly assigned value
 */
template <typename T, typename ANY_T>
T& top_assign(ArrayView<entt::any> const topData, TopDataId id, ANY_T&& any)
{
    entt::any &rData = topData[std::size_t(id)];
    rData = std::forward<ANY_T>(any);
    return entt::any_cast<T&>(rData);
}

/**
 * @return A reference to the value at index id inside topData
 */
template <typename T>
[[nodiscard]] T& top_get(ArrayView<entt::any> const topData, TopDataId const id)
{
    return entt::any_cast<T&>(topData[id]);
}

//-----------------------------------------------------------------------------

template<typename FUNCTOR_T>
struct wrap_args_trait
{
    template<typename T>
    static constexpr decltype(auto) cast_arg(ArrayView<entt::any> topData, WorkerContext ctx, std::size_t const argIndex)
    {
        if constexpr (std::is_same_v<T, WorkerContext>)
        {
            return ctx; // WorkerContext gives ctx instead of casting
        }
        else
        {
            LGRN_ASSERTMV(topData.size() > argIndex, "Task function has more arguments than TopDataIds provided", topData.size(), argIndex);
            return entt::any_cast<T&>(topData[argIndex]);
        }
    }

    template<typename ... ARGS_T, std::size_t ... INDEX_T>
    static constexpr decltype(auto) cast_args(ArrayView<entt::any> topData, WorkerContext ctx, [[maybe_unused]] std::index_sequence<INDEX_T...> indices) noexcept
    {
        return FUNCTOR_T{}(cast_arg<ARGS_T>(topData, ctx, INDEX_T) ...);
    }

    template<typename RETURN_T, typename ... ARGS_T>
    static TaskActions wrapped_task([[maybe_unused]] WorkerContext ctx, ArrayView<entt::any> topData) noexcept
    {
        if constexpr (std::is_void_v<RETURN_T>)
        {
            cast_args<ARGS_T ...>(topData, ctx, std::make_index_sequence<sizeof...(ARGS_T)>{});
            return {};
        }
        else if constexpr (std::is_same_v<RETURN_T, TaskActions>)
        {
            return cast_args<ARGS_T ...>(topData, ctx, std::make_index_sequence<sizeof...(ARGS_T)>{});
        }
    }

    // Extract arguments and return type from function pointer
    template<typename RETURN_T, typename ... ARGS_T>
    static constexpr TopTaskFunc_t unpack([[maybe_unused]] RETURN_T(*func)(ARGS_T...))
    {
        return &wrapped_task<RETURN_T, ARGS_T ...>;
    }
};

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

//TopTaskRef& TopTaskRef::aware_of_dirty_depends(bool value)
//{
//    m_rBuilder.m_rData.resize(m_rBuilder.m_rTasks.m_taskIds.capacity());
//    m_rBuilder.m_rData[m_taskId].m_awareOfDirtyDeps = value;
//    return *this;
//}

template<typename CONTAINER_T>
TopTaskTaskRef& TopTaskTaskRef::push_to(CONTAINER_T& rContainer)
{
    rContainer.push_back(m_taskId);
    return *this;
}

} // namespace osp
