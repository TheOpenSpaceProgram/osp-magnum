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

#include <cassert>
#include <iterator>
#include <type_traits>

namespace osp
{

/**
 * @brief Create a structured binding-compatible type from a contiguous
 *        container. A c-array is used.
 */
template<std::size_t N, typename RANGE_T>
constexpr auto& unpack(RANGE_T &rIn)
{
    using ptr_t = decltype(rIn.data());
    using type_t = std::remove_pointer_t<ptr_t>;

    assert(N <= rIn.size());
    return *reinterpret_cast<type_t(*)[N]>(std::data(rIn));
}

template<std::size_t N, typename CONTAINER_T>
constexpr auto& resize_then_unpack(CONTAINER_T &rIn)
{
    rIn.resize(N);
    return unpack<N, CONTAINER_T>(rIn);
}

} // namespace osp
