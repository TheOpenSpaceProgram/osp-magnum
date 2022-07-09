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

#include <algorithm>
#include <cstdint>

namespace osp
{

template<std::size_t N, typename T>
auto& unpack(Corrade::Containers::ArrayView<T> in)
{
    return *reinterpret_cast<T(*)[N]>(in.data());
}

[[nodiscard]] inline MainDataId main_find_empty(MainDataSpan_t const mainData, MainDataId current = 0)
{
    while ( bool(mainData[current]) && (current < mainData.size()) )
    {
        ++current;
    }
    return current;
}

template <typename IT_T, typename ITB_T>
MainDataId main_find_empty(MainDataSpan_t mainData, MainDataId current, IT_T destFirst, const ITB_T destLast)
{
    while (destFirst != destLast)
    {
        current = main_find_empty(mainData, current);

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
[[nodiscard]] T& main_emplace(MainDataSpan_t mainData, MainDataId id, ARGS_T&& ... args)
{
    entt::any &rData = mainData[std::size_t(id)];
    rData.emplace<T>(std::forward<ARGS_T>(args) ...);
    return entt::any_cast<T&>(rData);
}

template <typename T>
[[nodiscard]] T& main_get(MainDataSpan_t mainData, MainDataId const id)
{
    return entt::any_cast<T&>(mainData[id]);
}


} // namespace osp
