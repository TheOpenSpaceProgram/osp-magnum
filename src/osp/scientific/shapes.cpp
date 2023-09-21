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

#include "shapes.h"

#include <cassert>

namespace osp
{

float shape_volume(EShape shape, Vector3 scale)
{
    static constexpr float sc_pi = Magnum::Math::Constants<Magnum::Float>::pi();
    switch (shape)
    {
    case EShape::None:
        return 0.0f;
    case EShape::Sphere:
        // Default radius: 1
        return (4.0f / 3.0f) * sc_pi * scale.x() * scale.x() * scale.x();
    case EShape::Box:
        // Default width: 2
        return 2.0f*scale.x() * 2.0f*scale.y() * 2.0f*scale.z();
    case EShape::Cylinder:
        // Default radius: 1, default height: 2
        return sc_pi * scale.x() * scale.x() * 2.0f*scale.z();
    default:
        return 0.0f;
    }   
}

Matrix3 transform_inertia_tensor(Matrix3 I, float mass, Vector3 translation, Matrix3 rotation)
{
    // Apply rotation via similarity transformation
    I = rotation.transposed() * I * rotation;

    // Translate via tensor generalized parallel axis theorem
    const Vector3 r = translation;
    const Matrix3 outerProductR = {r * r.x(), r * r.y(), r * r.z()};
    const Matrix3 E3 = Matrix3{};

    return I + mass * (Magnum::Math::dot(r, r) * E3 - outerProductR);
}

Vector3 collider_inertia_tensor(EShape shape, Vector3 scale, float mass)
{
    switch (shape)
    {
    case EShape::None:
        return Vector3{0.0f};
    case EShape::Cylinder:
    {
        // Default cylinder dimensions: radius 1, height 2
        const float height = 2.0f * scale.z();
        // If sclY != sclX I will be mad
        const float radius = scale.x();
        return cylinder_inertia_tensor(radius, height, mass);
    }
    case EShape::Box:
    {
        // Default box dimensions: 2x2x2
        const Vector3 dimensions = 2.0f * scale;
        return cuboid_inertia_tensor(dimensions, mass);
    }
    case EShape::Sphere:
    {
        // Default sphere: radius = 1
        const Vector3 semiaxes = scale;
        return ellipsoid_inertia_tensor(semiaxes, mass);
    }
    case EShape::Capsule:

    default:
        return Vector3{0.0f};
    }
}

} // namespace osp
