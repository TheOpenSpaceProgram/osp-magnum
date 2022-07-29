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

#include "top_worker.h"

#include <entt/core/any.hpp>

#include <cassert>
#include <type_traits>
#include <utility>

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

template <typename T>
[[nodiscard]] T& top_get(ArrayView<entt::any> const topData, TopDataId const id)
{
    return entt::any_cast<T&>(topData[id]);
}



template<typename FUNCTOR_T>
struct wrap_args_trait
{
    template<typename ... ARGS_T, std::size_t ... INDEX_T>
    static constexpr void apply_void(ArrayView<entt::any> topData, [[maybe_unused]] std::integer_sequence<std::size_t, INDEX_T...> indices) noexcept
    {
        FUNCTOR_T{}(entt::any_cast<ARGS_T&>(topData[INDEX_T]) ...);
    }

    template<typename ... ARGS_T>
    static TopTaskStatus wrapper_void([[maybe_unused]] WorkerContext& rCtx, ArrayView<entt::any> topData) noexcept
    {
        assert(topData.size() == sizeof...(ARGS_T));

        apply_void<ARGS_T ...>(topData, std::make_integer_sequence<std::size_t, sizeof...(ARGS_T)>{});

        return TopTaskStatus::Success;
    }

    template<typename ... ARGS_T, std::size_t ... INDEX_T>
    static constexpr TopTaskStatus apply(ArrayView<entt::any> topData, [[maybe_unused]] std::integer_sequence<std::size_t, INDEX_T...> indices) noexcept
    {
        return FUNCTOR_T{}(entt::any_cast<ARGS_T&>(topData[INDEX_T]) ...);
    }

    template<typename ... ARGS_T>
    static TopTaskStatus wrapper([[maybe_unused]] WorkerContext& rCtx, ArrayView<entt::any> topData) noexcept
    {
        assert(topData.size() == sizeof...(ARGS_T));

        return apply<ARGS_T ...>(topData, std::make_integer_sequence<std::size_t, sizeof...(ARGS_T)>{});
    }

    template<typename ... ARGS_T>
    static constexpr TopTaskFunc_t unpack([[maybe_unused]] TopTaskStatus(*func)(ARGS_T...))
    {
        // Parameter pack used to extract function arguments, func is discarded
        return &wrapper<ARGS_T ...>;
    }

    template<typename ... ARGS_T>
    static constexpr TopTaskFunc_t unpack([[maybe_unused]] void(*func)(ARGS_T...))
    {
        // Parameter pack used to extract function arguments, func is discarded
        return &wrapper_void<ARGS_T ...>;
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
constexpr TopTaskFunc_t wrap_args(FUNC_T const&& funcArg)
{
    // TODO: wrap function pointers into a functor
    static_assert ( ! std::is_function_v<FUNC_T>, "Support for function pointers not yet implemented");

    using static_lambda_t = std::remove_cvref_t<decltype(funcArg)>;
    auto const functionPtr = +funcArg;

    static_assert(std::is_function_v< std::remove_pointer_t<decltype(functionPtr)> >);

    // The original funcArg is discarded, but will be reconstructed in
    // wrap_args_trait::apply, as static_lambda_t is stored in the type
    return wrap_args_trait<static_lambda_t>::unpack(functionPtr);
}

} // namespace osp
