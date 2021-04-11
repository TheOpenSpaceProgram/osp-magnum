/**
 * Open Space Program
 * Copyright © 2019-2020 Open Space Program Project
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

#include "types.h"
#include <type_traits>

namespace osp::math
{

template <typename UINT_T>
constexpr UINT_T uint_2pow(UINT_T exponent)
{
    static_assert(std::is_integral<UINT_T>::value, "Unsigned int required");
    return UINT_T(1) << exponent;
}

template <typename UINT_T>
constexpr bool is_power_of_2(UINT_T value)
{
    static_assert(std::is_integral<UINT_T>::value, "Unsigned int required");
    // Test to see if the value contains more than 1 set bit
    return !(value == 0) && !(value & (value - 1));
}

/**
 * Calculates the number of blocks of @blockSize elements needed to hold @nElements
 */
template <typename INT_T>
constexpr INT_T num_blocks(INT_T nElements, INT_T blockSize)
{
    static_assert(std::is_integral<INT_T>::value, "Integral type required");

    INT_T remainder = nElements % blockSize;
    return (nElements / blockSize) + (remainder > 0) ? 1 : 0;
}

} // namespace osp::math
