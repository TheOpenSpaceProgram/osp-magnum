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

// An int for space
typedef int64_t SpaceInt;

// A Vector3 for space
//typedef Magnum::Math::Vector3<SpaceInt> Vector3s;


class Vector3s// : private Magnum::Math::Vector3<SpaceInt>
{
// using a subclass added a macros for a bunch of operators

public:

    //Vector3s() : Magnum::Math::Vector3<SpaceInt>(), m_precision(10) {}
    Vector3s() : m_vector(), m_precision(10) {}
    Vector3s(Magnum::Math::Vector3<SpaceInt> const& vector, int8_t precision) :
        m_vector(vector), m_precision(precision) {}
    Vector3s(Vector3s const& copy) = default;

    Vector3 to_meters()
    {
        return Vector3(m_vector) * Magnum::Math::pow<float>(2.0f, -10);
    }

    //using Magnum::Math::Vector3<SpaceInt>::x;
    //using Magnum::Math::Vector3<SpaceInt>::y;
    //using Magnum::Math::Vector3<SpaceInt>::z;
    SpaceInt& x() { return m_vector.x(); }
    SpaceInt& y() { return m_vector.y(); }
    SpaceInt& z() { return m_vector.z(); }
    int8_t& precision() { return m_precision; }


    //using Magnum::Math::Vector3<SpaceInt>::operator +;
    //using Magnum::Math::Vector3<SpaceInt>::operator -;

    Vector3s operator+(const Vector3s rhs) const
    {
        // put more stuff here eventually
        return Vector3s(m_vector + rhs.m_vector, m_precision);
    }

    Vector3s operator-(const Vector3s rhs) const
    {
        // put more stuff here eventually
        return Vector3s(m_vector - rhs.m_vector, m_precision);
    }

private:
    Magnum::Math::Vector3<SpaceInt> m_vector;
    int8_t m_precision;
};
