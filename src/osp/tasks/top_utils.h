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
#include "top_worker.h"

#include <entt/core/any.hpp>

#include <cassert>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

namespace osp
{

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

template <typename T, typename ... ARGS_T>
T& top_emplace(ArrayView<entt::any> const topData, TopDataId id, ARGS_T&& ... args)
{
    entt::any &rData = topData[std::size_t(id)];
    rData.emplace<T>(std::forward<ARGS_T>(args) ...);
    return entt::any_cast<T&>(rData);
}

template <typename T, typename ANY_T>
T& top_assign(ArrayView<entt::any> const topData, TopDataId id, ANY_T&& any)
{
    entt::any &rData = topData[std::size_t(id)];
    rData = std::forward<ANY_T>(any);
    return entt::any_cast<T&>(rData);
}


template <typename T>
[[nodiscard]] T& top_get(ArrayView<entt::any> const topData, TopDataId const id)
{
    return entt::any_cast<T&>(topData[id]);
}



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
            return entt::any_cast<T&>(topData[argIndex]);
        }
    }

    template<typename ... ARGS_T, std::size_t ... INDEX_T>
    static constexpr decltype(auto) cast_args(ArrayView<entt::any> topData, WorkerContext ctx, [[maybe_unused]] std::index_sequence<INDEX_T...> indices) noexcept
    {
        assert(topData.size() == sizeof...(ARGS_T));
        return FUNCTOR_T{}(cast_arg<ARGS_T>(topData, ctx, INDEX_T) ...);
    }

    template<typename RETURN_T, typename ... ARGS_T>
    static TopTaskStatus wrapped_task([[maybe_unused]] WorkerContext ctx, ArrayView<entt::any> topData) noexcept
    {
        if constexpr (std::is_void_v<RETURN_T>)
        {
            cast_args<ARGS_T ...>(topData, ctx, std::make_index_sequence<sizeof...(ARGS_T)>{});
            return TopTaskStatus::Success;
        }
        else if (std::is_same_v<RETURN_T, TopTaskStatus>)
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


} // namespace osp
