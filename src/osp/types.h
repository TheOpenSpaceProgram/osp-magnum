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

#include <map>

#include <Magnum/Magnum.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Quaternion.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/Math/Matrix3.h>

//typedef KeyMap std::map<Magnum::Platform::Application::KeyEvent>

namespace osp
{

using Magnum::Vector2;
using Magnum::Vector2i;

using Magnum::Vector3;
using Magnum::Vector3d;
using Magnum::Vector4;
using Magnum::Quaternion;
using Magnum::Matrix3;
using Magnum::Matrix4;

using Magnum::Rad;

// An int for space
typedef int64_t SpaceInt;

// 1024 space units = 1 meter
// TODO: this should vary by trajectory, but for now it's global
const float gc_units_per_meter = 1024.0f;

// A Vector3 for space
//typedef Magnum::Math::Vector3<SpaceInt> Vector3sp;

using Vector3s = Magnum::Math::Vector3<SpaceInt>;

class Vector3sp// : private Magnum::Math::Vector3<SpaceInt>
{
// using a subclass added a macros for a bunch of operators

public:

    //Vector3sp() : Magnum::Math::Vector3<SpaceInt>(), m_precision(10) {}
    Vector3sp() : m_vector(), m_precision(10) {}
    Vector3sp(Vector3s const& vector, int8_t precision) :
        m_vector(vector), m_precision(precision) {}
    Vector3sp(Vector3sp const& copy) = default;

    Vector3 to_meters()
    {
        return Vector3(m_vector) * Magnum::Math::pow<float>(2.0f, -m_precision);
    }

    //using Magnum::Math::Vector3<SpaceInt>::x;
    //using Magnum::Math::Vector3<SpaceInt>::y;
    //using Magnum::Math::Vector3<SpaceInt>::z;
    SpaceInt& x() { return m_vector.x(); }
    SpaceInt& y() { return m_vector.y(); }
    SpaceInt& z() { return m_vector.z(); }

    SpaceInt x() const { return m_vector.x(); }
    SpaceInt y() const { return m_vector.y(); }
    SpaceInt z() const { return m_vector.z(); }
    int8_t& precision() { return m_precision; }


    //using Magnum::Math::Vector3<SpaceInt>::operator +;
    //using Magnum::Math::Vector3<SpaceInt>::operator -;

    Vector3sp operator+(const Vector3sp rhs) const
    {
        // put more stuff here eventually
        return Vector3sp(m_vector + rhs.m_vector, m_precision);
    }

    Vector3sp operator-(const Vector3sp rhs) const
    {
        // put more stuff here eventually
        return Vector3sp(m_vector - rhs.m_vector, m_precision);
    }

    float units_per_meter()
    {
        return Magnum::Math::pow<float>(2.0f, m_precision);
    }

private:
    Vector3s m_vector;
    int8_t m_precision;
};


}
