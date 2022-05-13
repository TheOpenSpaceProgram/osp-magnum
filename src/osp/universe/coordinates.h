/**
 * Open Space Program
 * Copyright © 2019-2021 Open Space Program Project
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

#include "universetypes.h"
#include "../types.h"
#include "../CommonMath.h"

#include <optional>
#include <tuple>
#include <variant>
#include <vector>

namespace osp::universe
{

/**
 * @brief A functor used to transform positions between coordinate spaces
 *
 * Transforming coordinates from one space to another is translation and scale.
 *
 * Parent to Child: f(x) = (precision difference) * (x - childPos)
 * Child to Parent: f(x) = (precision difference) * x + childPos
 *
 * These two can be re-arranged into a general form:
 *
 * f(x) = x * 2^expX + c * 2^expC
 *
 */
struct CoordspaceTransform
{
    using Vec_t = Magnum::Math::Vector3<uint64_t>;

    /**
     * @brief Transform a Vector3g using this CoordspaceTransform
     *
     * @param in [in] Vector3g to transform
     *
     * @return Transformed Vector3g
     */
    Vector3g operator()(Vector3g in) const noexcept
    {
        using osp::math::mul_2pow;
        return mul_2pow<Vector3g, spaceint_t>(in, m_expX)
                + mul_2pow<Vector3g, spaceint_t>(Vector3g(m_c), m_expC);
    }

    /**
     * @brief Substitute another CoordspaceTransform into this transform,
     *        resulting in a new composite transform.
     *
     * With coordinate spaces A, B, and C, transforming from A->B(x) is a,
     * function and B->C(c) is also a function. This means A->C(x) is equal to
     * B->C(A->B)
     *
     * In general form, the algebra can be worked out like this:
     *
     * in(x) = x * 2^expX2  +  c2 * 2^expC2
     * out(x) = in(x) * 2^expX1  +  c1 * 2^expC1
     *
     * plug in(x) into out as x
     * out(x) = (x * 2^expX2  +  c2 * 2^expC2) * 2^expX1  +  c1 * 2^expC1
     * out(x) = x*2^(expX1+expX2)  +  c1 * 2^expC1  +  c2 * 2^(expC2+expX1)
     *
     * Combine c1 and c2 terms by splitting off one of the exponents, and
     * multiplying into a c value so that both c1 and c2 have the same exponent
     *
     * ie. c*2^expC -> (c*2^expD) * 2^(expC - expD)
     * out(x) = x*2^(expX1+expX2) + c3 * 2^expC3
     *
     * @param in [in] CoordspaceTransform to substitute as X
     *
     * @return A new CoordspaceTransform formed from substitution
     */
    constexpr CoordspaceTransform operator()(CoordspaceTransform const& in) const noexcept
    {
        CoordspaceTransform result;

        result.m_expX = m_expX + in.m_expX;

        int16_t expC1 = m_expC;
        int16_t expC2 = m_expX + in.m_expC;

        if (expC1 == expC2)
        {
            result.m_expC = expC1;
            result.m_c = m_c + in.m_c;
        }
        else if (expC1 > expC2)
        {
            // expC1 too high, multiply into m_c
            result.m_expC = expC2;
            result.m_c = m_c * 2L * (1 << (expC1-expC2)) + in.m_c;

        }
        else if (expC1 < expC2)
        {
            // expC2 too high, multiply into in.m_c
            result.m_expC = expC1;
            result.m_c = m_c + in.m_c * 2L * (1 << (expC2-expC1));
        }

        return result;
    }

    int16_t m_expX{0};

    Vec_t m_c{0};
    int16_t m_expC{0};

}; // struct CoordspaceTransform

namespace transform
{

constexpr CoordspaceTransform scaled(
        CoordspaceTransform x, int16_t from, int16_t to) noexcept
{
    return CoordspaceTransform{int16_t(x.m_expX + (from - to)), x.m_c, x.m_expC};
}

constexpr CoordspaceTransform child_to_parent(
        Vector3g const childPos, int16_t childPrec, int16_t parentPrec) noexcept
{
    int16_t exp = childPrec - parentPrec;
    return CoordspaceTransform{exp, CoordspaceTransform::Vec_t(childPos), 0};
}

constexpr CoordspaceTransform parent_to_child(
        Vector3g const childPos, int16_t childPrec, int16_t parentPrec) noexcept
{
    int16_t exp = parentPrec - childPrec;
    // individual vector components is workaround for non-constexpr operator-()
    return CoordspaceTransform{exp, CoordspaceTransform::Vec_t( -childPos.x(), -childPos.y(), -childPos.z()), exp};
}

}


}
