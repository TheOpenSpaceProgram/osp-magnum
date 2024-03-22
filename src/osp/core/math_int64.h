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

#include "math_types.h"

namespace osp
{

/**
 * @brief int64 abs(lhs - rhs) with no risk of overflow
 */
constexpr std::uint64_t absdelta(std::int64_t lhs, std::int64_t rhs) noexcept
{
    // TODO: maybe use int128 for compilers that support it?

    bool const lhsPositive = lhs > 0;
    bool const rhsPositive = rhs > 0;
    if      (   lhsPositive && ! rhsPositive )
    {
        return std::uint64_t(lhs) + std::uint64_t(-rhs);
    }
    else if ( ! lhsPositive &&   rhsPositive )
    {
        return std::uint64_t(-lhs) + std::uint64_t(rhs);
    }
    // else, lhs and rhs are the same sign. no risk of overflow

    return std::uint64_t((lhs > rhs) ? (lhs - rhs) : (rhs - lhs));
};

/**
 * @brief (distance between a and b) > threshold
 *
 * This function is quick and dirty, max threshold is limited to 1,431,655,765
 */
constexpr bool is_distance_near(Vector3l const a, Vector3l const b, std::uint64_t const threshold)
{
    std::uint64_t const dx = absdelta(a.x(), b.x());
    std::uint64_t const dy = absdelta(a.y(), b.y());
    std::uint64_t const dz = absdelta(a.z(), b.z());

    // 1431655765 = sqrt(2^64)/3 = max distance without risk of overflow
    constexpr std::uint64_t dmax = 1431655765ul;

    if (dx > dmax || dy > dmax || dz > dmax)
    {
        return false;
    }

    std::uint64_t const magnitudeSqr = (dx*dx + dy*dy + dz*dz);

    return magnitudeSqr < threshold*threshold;
}

} // namespace osp
