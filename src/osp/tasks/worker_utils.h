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


template <typename T>
struct MainDataPair
{
    T &m_rData;
    MainDataId m_id;
};


template <typename T, typename ... ARGS_T>
MainDataPair<T> main_emplace(MainDataSpan_t mainData, MainDataIt_t& rItCurr, ARGS_T&& ... args)
{
    MainDataIt_t const itFirst = std::begin(mainData);
    MainDataIt_t const itLast = std::end(mainData);

    // search for next empty spot in mainData to emplace T into
    rItCurr = std::find_if(rItCurr, itLast, [] (entt::any const &any)
    {
        return ! bool(any);
    });

    // Expected to always contain an empty entt::any
    assert(rItCurr != itLast);

    T& rData = rItCurr->emplace<T>(std::forward<ARGS_T>(args) ...);

    return {rData, std::distance(itFirst, rItCurr)};
}


template <typename T>
T& main_get(MainDataSpan_t mainData, MainDataId const id)
{
    return entt::any_cast<T&>(mainData[id]);
}


} // namespace osp
