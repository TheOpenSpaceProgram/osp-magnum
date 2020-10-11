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

#include "CommonPhysics.h"

#include <iostream>
#include <assert.h>

namespace osp::phys
{

float shape_volume(ECollisionShape shape, Vector3 scale)
{
    static constexpr float sc_pi = Magnum::Math::Constants<Magnum::Float>::pi();
    switch (shape)
    {
    case ECollisionShape::NONE:
        return 0.0f;
    case ECollisionShape::SPHERE:
        // Default radius: 1
        return (4.0f / 3.0f) * sc_pi * scale.x() * scale.x() * scale.x();
    case ECollisionShape::BOX:
        // Default width: 2
        return 2.0f*scale.x() * 2.0f*scale.y() * 2.0f*scale.z();
    case ECollisionShape::CYLINDER:
        // Default radius: 1, default height: 2
        return sc_pi * scale.x() * scale.x() * 2.0f*scale.z();
    case ECollisionShape::CAPSULE:
    case ECollisionShape::CONVEX_HULL:
    case ECollisionShape::TERRAIN:
    case ECollisionShape::COMBINED:
    default:
        std::cout << "Error: unsupported shape for volume calc\n";
        assert(false);
    }
}

Matrix3 transform_inertia_tensor(Matrix3 I, float mass, Vector3 translation, Matrix3 rotation)
{
    // Apply rotation via similarity transformation
    I = rotation.transposed() * I * rotation;

    // Translate via tensor generalized parallel axis theorem
    using Magnum::Math::dot;
    const Vector3 r = translation;
    const Matrix3 outerProductR = {r * r.x(), r * r.y(), r * r.z()};
    const Matrix3 E3 = Matrix3{};

    return I + mass * (dot(r, r) * E3 - outerProductR);
}

Vector3 collider_inertia_tensor(ECollisionShape shape, Vector3 scale, float mass)
{
    switch (shape)
    {
    case ECollisionShape::CYLINDER:
    {
        // Default cylinder dimensions: radius 1, height 2
        const float height = 2.0f * scale.z();
        // If sclY != sclX I will be mad
        const float radius = scale.x();
        return cylinder_inertia_tensor(radius, height, mass);
    }
    case ECollisionShape::BOX:
    {
        // Default box dimensions: 2x2x2
        const Vector3 dimensions = 2.0f * scale;
        return cuboid_inertia_tensor(dimensions, mass);
    }
    case ECollisionShape::SPHERE:
    {
        // Default sphere: radius = 1
        const Vector3 semiaxes = scale;
        return ellipsoid_inertia_tensor(semiaxes, mass);
    }
    case ECollisionShape::CAPSULE:
    case ECollisionShape::CONVEX_HULL:
    case ECollisionShape::TERRAIN:
    case ECollisionShape::COMBINED:
    default:
        std::cout << "ERROR: unknown collision shape\n";
        assert(false);
        return Vector3{0.0f};
    }
}

} // namespace osp::phys
