#pragma once

#include <map>

#include <Magnum/Magnum.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Quaternion.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/Math/Matrix3.h>

#include <Magnum/Platform/Sdl2Application.h>

//typedef KeyMap std::map<Magnum::Platform::Application::KeyEvent>

using Magnum::Vector2;
using Magnum::Vector2i;

using Magnum::Vector3;
using Magnum::Quaternion;
using Magnum::Matrix4;

using Magnum::Rad;

// An int for space
typedef int64_t SpaceInt;

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
