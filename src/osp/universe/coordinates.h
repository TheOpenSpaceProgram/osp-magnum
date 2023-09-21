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

#include "universe.h"

#include "../core/math_2pow.h"

namespace osp::universe
{

inline Vector3g rotate_vector3g(Vector3g const in, Quaterniond const rot) noexcept
{
    // transformVector isn't constexpr.
    return Vector3g(rot.transformVector(Vector3d(in)));
}

constexpr bool quat_non_zero(Quaterniond const in) noexcept
{
    // scalar = cos(angle / 2); no angle means scalar = 1
    return in.scalar() != 1.0;
}


/**
 * @brief Describes a mathematical function used to transform positions between
 *        coordinate spaces
 *
 * 2D example:
 *
 * Parent: ... -O-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|
 *              0     1     2     3     4     5     6     7     8     9    10
 *
 *              |--|--|--|--|--|--|--|--|--|--O--|--|--|--|--|--|--|--|--|--|
 * Child:     -10 -9 -8 -7 -6 -5 -4 -3 -2 -1  0  1  2  3  4  5  6  7  8  9 10
 *
 * * Child's precision is 1 unit higher than parent.    precDiff = 1
 * * Child position: 5 (relative to parent)             childPos = 5
 *
 * From inspection, we can write functions to transform coordinates between
 * these spaces:
 *
 * To transform Parent-to-Child:    P->C(x) = 2^(precDiff) * (x - childPos)
 * For Child-to-Parent (inverse):   C->P(x) = 2^(-precDiff) * x + childPos
 *
 * * i.e. P->C(5) = 0,  P->C(3) = -4,   P->C(10) = 10
 *        C->P(0) = 5,  C->P(-4) = 3,   C->P(10) = 10
 *
 * Both equations can be re-arranged into a common form: f(x) = x*2^n + c*2^m
 *
 * P->C(x) = 2^(precDiff) * (x - childPos)
 * P->C(x) = x * 2^(precDiff) - childPos * 2^(precDiff)
 *           substitute:    n = m = precDiff;    c = -childPos
 * P->C(x) = x * 2^n + c * 2^m
 *
 * C->P(x) = 2^(-precDiff) * x + childPos
 *           substitute:    n = -precDiff;       c = childPos;     m = 0;
 * C->P(x) = x * 2^n * + c * 2^m
 *
 * This form generalizes both operations, as well as make them easier to
 * combine.
 *
 * The c * 2^m term may seem redundant, as they can be merged into a single
 * variable; however, keeping them separate is less susceptible to overflow
 * errors. This allows flawless transforms across intermediate coordinate
 * spaces of varrying precisions.
 *
 * Rotations add a bit more complexity. The two main parent-child operations
 * apply rotations in different places:
 *
 * P->C(x) = rotate-1(2^(precDiff) * (x - childPos))
 * C->P(x) = 2^(-precDiff) * rotate(x) + childPos
 *
 * Converting between forms will require multiplying c, which ruins precision
 * at large ranges. The common form can instead feature both inner and outer
 * rotations. Denoted R for outer, and r for inner:
 *
 * f(x) = R( r(x)*2^n + c*2^m )
 */
struct CoordTransformer
{
    Quaterniond     m_rotOut;
    Quaterniond     m_rotIn;
    Vector3g        m_c;
    int             m_n{0};
    int             m_m{0};

    Vector3g transform_position(Vector3g in) const noexcept
    {
        using osp::math::mul_2pow;

        if (quat_non_zero(m_rotIn))
        {
            in = rotate_vector3g(in, m_rotIn);
        }

        Vector3g out = mul_2pow<Vector3g, spaceint_t>(in, m_n)
                     + mul_2pow<Vector3g, spaceint_t>(Vector3g(m_c), m_m);

        if (quat_non_zero(m_rotOut))
        {
            out = rotate_vector3g(out, m_rotOut);
        }

        return out;
    }

    Quaterniond rotation() const noexcept
    {
        return m_rotOut * m_rotIn;
    }

    constexpr bool is_identity() const noexcept
    {
        return (m_n == 0) && m_c.isZero();
    }
};

/**
 * @brief Composite together two CoordTransformers
 *
 * Manually chaining transforms between spaces of different precisions can lead
 * to loss of information due to rounding.
 * ie. High precision -> Low precision (rounded!) -> High precision (oof!)
 *
 * Compositing the CoordTransformers prevents these errors, and only needs to
 * be calculated once when transforming multiple positions.
 *
 * For coordinate spaces A, B, and C, there exists transform functions
 * A->B(x) and B->C(x).
 *
 * A->C(x) can be formed by substituting functions into each other:
 *
 * A->C(x) = B->C(A->B(x))
 *
 * Algebra goes like this:
 *
 * given:    f1(x)  =  x * 2^n1  +  c1 * 2^m1
 * given:    f2(x)  =  x * 2^n2  +  c2 * 2^m2
 * unknown:  f3(x)  =  x * 2^n3  +  c3 * 2^m3  =  f1( f2(x) )
 *
 * f3(x)   =   f1( f2(x) )
 * f3(x)   =   f2(x)                    * 2^n1          +   c1 * 2^m1
 * f3(x)   =   ( x * 2^n2 + c2 * 2^m2 ) * 2^n1          +   c1 * 2^m1
 * f3(x)   =   x * 2^n2 * 2^n1   +   c2 * 2^m2 * 2^n1   +   c1 * 2^m1
 * f3(x)   =   x * 2^(n1+n2)     +   c2 * 2^(m2+n1)     +   c1 * 2^m1
 *               n3 = n1+n2
 * f3(x)   =   x * 2^n3          +   c2 * 2^(m2+n1)     +   c1 * 2^m1
 *
 * To combine the c2 and c1 terms, their exponents need to be the same. Either
 * term needs to be modified to match the other term. To avoid rounding losses,
 * we avoid splitting off a negative exponent.
 *
 * Exponent change:     2^u + 2^v  ->  2^(u-v+v) + 2^v  ->  2^(u-v)*2^(v) + b^v
 *                   -> (2^(u-v) + b)^v
 *
 * let d = (m2+n1) - m1
 *
 * if (d == 0):  both exponents are safe to combine
 *     c3 = (c2+c1)             m3 = m1 or (m2 + n1)
 *
 * if (d > 0):   c2*2^(m2+n1) + c1*2^m1  ->  c2*(2^d)*(2^m1) + c1*2^m1
 *     c3 = c2*(2^d) + c1;      m3 = m1
 *
 * if (d < 0):   c2*2^(m2+n1) + c1*2^m1  ->  c2*2^(m2+n1) + c1*2^(-d)*(m2+n1)
 *     c3 = c2 + c1*2^(-d);     m3 = m2+n1
 *
 * All variables are now known: h(x)  =  x * 2^n3  +  c3 * 2^m3
 *
 *
 * Accounting for rotations is a bit messier:
 *
 * f3(x)  =  R1[ r1( R2[ r2(x) * 2^n2 + c2 * 2^m2] ) * 2^n1 + c1 * 2^m1  ]
 *
 * Rotate functions are linear maps, with additivity and homogeneity:
 *  * f(U + V) = f(U) + f(V)
 *  * f(aU) = af(U)
 * Steps are similar to without rotations, so skipping over...
 *
 * f3(x)  =  R1[ r1(R2[r2(x)]) * 2^n3  +  r1(R2[c2]) * 2^(m2+n1)  +  c1 * 2^m1 ]
 *
 * To combine c1 and c2 terms, 2^ terms are dealt with previously (m3), but now
 * rotations must also match.
 *
 * Either rotate c2 by r1(R2[x]):
 * f3(x)  =  R1[   r1(R2[r2(x)]) * 2^n3   +   (r1(R2[c2]) + c1) * 2^m3   ]
 *           R3(x) = R1(x);      r3(x) = r1(R2[x])   c3 = (r1(R2[c2]) + c1)
 *
 * Or rotate all terms by inverse of r1(R2[x]):
 * f3(x)  =  R1[r1(R2[     r2(x) * 2^n3   +  (c2 + R2(r1-1[c2])) * 2^m3     ])]
 *           R3 = R1[r1(R2[x])]  r3(x) = r2(x)       c3 = (c2 + R2-1(r1-1[c1]))
 *
 * If r1(R2[x]) is an identity function, just ignore it.
 *
 * @param f1 [in] Outer function to composite
 * @param f2 [in] Inner function to composite
 *
 * @return Composite CoordTransformer f1( f2(x) )
 */
inline CoordTransformer coord_composite(
        CoordTransformer const& f1, CoordTransformer const& f2) noexcept
{
    Vector3g c1;
    Vector3g c2;
    int m3;

    int const d = f2.m_m + f1.m_n - f1.m_m;

    if (d == 0)
    {
        c1 = f1.m_c;
        c2 = f2.m_c;
        m3 = f1.m_m;
    }
    else if (d > 0)
    {
        c1 = f1.m_c;
        c2 = f2.m_c * math::int_2pow<spaceint_t>(d);
        m3 = f1.m_m;
    }
    else // if (d < 0)
    {
        c1 = f1.m_c * math::int_2pow<spaceint_t>(-d);
        c2 = f2.m_c;
        m3 = f2.m_m + f1.m_n;
    }

    Quaterniond const in1Out2 = f1.m_rotIn * f2.m_rotOut;
    Quaterniond out3;
    Quaterniond in3;

    if (quat_non_zero(in1Out2))
    {
        if (c1 > c2)
        {
            c2 = rotate_vector3g(c2, in1Out2);
            out3 = f1.m_rotOut;
            in3 = in1Out2 * f2.m_rotIn;
        }
        else
        {
            c1 = rotate_vector3g(c1, in1Out2.inverted());
            out3 = f1.m_rotOut * in1Out2;
            in3 = f2.m_rotIn;
        }
    }
    else
    {
        out3 = f1.m_rotOut;
        in3 = f2.m_rotIn;
    }

    return {
        .m_rotOut   = out3,
        .m_rotIn    = in3,
        .m_c        = c1 + c2,
        .m_n        = f1.m_n + f2.m_n,
        .m_m        = m3
    };
}

inline CoordTransformer coord_parent_to_child(
        CoSpaceTransform const& parent, CoSpaceTransform const& child) noexcept
{
    int const precisionDiff = child.m_precision - parent.m_precision;

    return {
        .m_rotOut   = child.m_rotation.inverted(),
        .m_c        = -child.m_position,
        .m_n        = precisionDiff,
        .m_m        = precisionDiff
    };
}

inline CoordTransformer coord_child_to_parent(
        CoSpaceTransform const& parent, CoSpaceTransform const& child) noexcept
{
    int const precisionDiff = child.m_precision - parent.m_precision;

    return {
        .m_rotIn    = child.m_rotation,
        .m_c        = child.m_position,
        .m_n        = -precisionDiff,
        .m_m        = 0
    };
}

}
