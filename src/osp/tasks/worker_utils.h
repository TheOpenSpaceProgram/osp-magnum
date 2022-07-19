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

#include "worker.h"

#include <entt/core/any.hpp>

#include <cassert>
#include <type_traits>
#include <utility>

namespace osp
{

[[nodiscard]] inline MainDataId main_reserve(
        ArrayView<entt::any> const mainData, MainDataId current = 0)
{
    while ( bool(mainData[current]) && (current < mainData.size()) )
    {
        ++current;
    }

    if (current < mainData.size())
    {
        mainData[current].emplace<Reserved>();
    }

    return current;
}

template <typename IT_T, typename ITB_T>
MainDataId main_reserve(
        ArrayView<entt::any> const mainData, MainDataId current,
        IT_T destFirst, const ITB_T destLast)
{
    while (destFirst != destLast)
    {
        current = main_reserve(mainData, current);

        if (current < mainData.size())
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
T& main_emplace(ArrayView<entt::any> const mainData, MainDataId id, ARGS_T&& ... args)
{
    entt::any &rData = mainData[std::size_t(id)];
    rData.emplace<T>(std::forward<ARGS_T>(args) ...);
    return entt::any_cast<T&>(rData);
}

template <typename T>
[[nodiscard]] T& main_get(ArrayView<entt::any> const mainData, MainDataId const id)
{
    return entt::any_cast<T&>(mainData[id]);
}



template<typename FUNCTOR_T>
struct wrap_args_trait
{
    template<typename ... ARGS_T, std::size_t ... INDEX_T>
    static constexpr MainTaskStatus apply(ArrayView<entt::any> mainData, std::integer_sequence<std::size_t, INDEX_T...> indices) noexcept
    {
        return FUNCTOR_T{}(entt::any_cast<ARGS_T&>(mainData[INDEX_T]) ...);
    }

    template<typename ... ARGS_T>
    static MainTaskStatus wrapper(WorkerContext& rCtx, ArrayView<entt::any> mainData) noexcept
    {
        assert(mainData.size() == sizeof...(ARGS_T));

        return apply<ARGS_T ...>(mainData, std::make_integer_sequence<std::size_t, sizeof...(ARGS_T)>{});
    }

    template<typename ... ARGS_T>
    static constexpr MainTaskFunc_t unpack(MainTaskStatus(*func)(ARGS_T...))
    {
        // Parameter pack used to extract function arguments, func is discarded
        return &wrapper<ARGS_T ...>;
    }
};

/**
 * @brief Wrap a function with arbitrary arguments into a MainTaskFunc_t
 *
 * A regular MainTaskFunc_t accepts a ArrayView<entt::any> for passing data of
 * arbitrary types. This needs to be manually casted.
 *
 * wrap_args creates a wrapper function that will automatically cast the
 * entt::anys and call the underlying function.
 *
 * @param a[in]
 *
 * @return MainTaskFunc_t function pointer to wrapper
 */
template<typename FUNC_T>
constexpr MainTaskFunc_t wrap_args(FUNC_T const&& funcArg)
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
