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


#include <cstdint>
#include <type_traits>

namespace osp::math
{

/**
 * @return Integer 2^exponent
 */
template <typename INT_T>
constexpr INT_T int_2pow(int exponent) noexcept
{
    static_assert(std::is_integral<INT_T>::value, "Integer required");
    return INT_T(1) << exponent;
}

/**
 * @return true if value is power of two
 */
template <typename INT_T>
constexpr bool is_power_of_2(INT_T value) noexcept
{
    static_assert(std::is_integral_v<INT_T>, "Integer required");
    // Test to see if the value contains more than 1 set bit
    return !(value == 0) && !(value & (value - 1));
}

/**
 * @brief Multiply a type by a power of two, allowing negative exponents
 *
 * @param value    [in] Value to multiply
 * @param exponent [in] Exponent to raise (or lower) 2 by
 *
 * @tparam T     Type to multiply
 * @tparam INT_T Integer type the power of two will be calculated in
 *
 * @return value multiplied by 2^exponent
 */
template <typename T, typename INT_T>
constexpr T mul_2pow(T value, int exponent) noexcept
{
    // Multiply by power of two if exponent is positive
    // Divide by power of two if exponent is negative
    // exponent = 0 will multiply by 1
    return (exponent >= 0) ? (value * int_2pow<INT_T>(exponent))
                          : (value / int_2pow<INT_T>(-exponent));
}

} // namespace osp::math
