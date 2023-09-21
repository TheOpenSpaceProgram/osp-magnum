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

#include "../core/math_types.h"

namespace osp
{

/**
 * @brief Basic primitive shapes used for collisions and inertia calculations
 */
enum class EShape : uint8_t
{
    None        = 0,
    Custom      = 1,
    Sphere      = 2,
    Box         = 3,
    Capsule     = 4,
    Cylinder    = 5
};

/**
 * Compute the volume of an EShape
 * 
 * Given the type of shape and the scale in X,Y,Z, computes the volume of the
 * primitive shape. Axis-aligned shapes (e.g. cylinder, capsule) are aligned
 * along the z-axis.
 * 
 * As this function is meant to deal with shapes that are defined within parts
 * in Blender, the default size of each primitive is inherited from Blender's
 * default empty, which is a bounding box with dimensions 2x2x2 meters. See
 * function implementation for shape-specific details.
 * 
 * @param shape [in] The primitive shape to compute
 * @param scale [in] The scale in X,Y,Z to apply to the shape
 * 
 * @return the volume of the shape in m^3
 */
float shape_volume(EShape shape, Vector3 scale);

/**
 * Transform an inertia tensor
 *
 * Transforms an inertia tensor using the parallel axis theorem.
 * See "Tensor generalization" section on
 * https://en.wikipedia.org/wiki/Parallel_axis_theorem for more information.
 *
 * @param I           [in] The original inertia tensor
 * @param mass        [in] The total mass of the object
 * @param translation [in] The translation part of the transformation
 * @param rotation    [in] The rotation part of the transformation
 *
 * @return The transformed inertia tensor
 */
Matrix3 transform_inertia_tensor(Matrix3 I, float mass,
    Vector3 translation, Matrix3 rotation);

/**
 * Compute the inertia tensor for a collider shape
 * 
 * Automatically selects the correct function necessary to compute the inertia
 * for the given shape
 * 
 * @param shape [in] The shape of the collider
 * @param scale [in] The (x, y, z) scale of the collider
 * @param mass  [in] The total mass of the collider
 * 
 * @return The moment of inertia about the principle axes (x, y, z)
 */
Vector3 collider_inertia_tensor(EShape shape, Vector3 scale, float mass);

/**
 * Compute the inertia tensor for a cylinder
 *
 * Computes the moment of inertia about the principal axes of a cylinder
 * with specified mass, height, and radius, whose axis of symmetry lies
 * along the z-axis
 *
 * @return The moment of inertia about the 3 principal axes (x, y, z)
 */
constexpr Vector3 cylinder_inertia_tensor(float radius, float height, float mass) noexcept
{
    float const r2 = radius * radius;
    float const h2 = height * height;

    float const xx = (1.0f / 12.0f) * (3.0f*r2 + h2);
    float const yy = xx;
    float const zz = r2 / 2.0f;

    return Vector3{mass*xx, mass*yy, mass*zz};
}

/**
 * Compute the inertia tensor for a cuboid
 *
 * Computes the moment of inertia about the principal axes of a rectangular
 * prism with specified mass, and dimensions (x, y, z)
 *
 * @param dimensions [in] Vector containing x, y, and z dimensions of the box
 * @param mass [in] mass of the box
 *
 * @return The moment of inertia about the 3 principal axes (x, y, z)
 */
constexpr Vector3 cuboid_inertia_tensor(const Vector3 dimensions, float mass) noexcept
{
    float const x2 = dimensions.x() * dimensions.x();
    float const y2 = dimensions.y() * dimensions.y();
    float const z2 = dimensions.z() * dimensions.z();

    float const xx = y2 + z2;
    float const yy = x2 + z2;
    float const zz = x2 + y2;

    // Pre-multiply constant to avoid non-constexpr operator*(float, Vector3)
    float const c = (1.0f / 12.0f) * mass;
    return Vector3{c*xx, c*yy, c*zz};
}

/**
 * Compute the inertia tensor for an ellipsoid
 * 
 * Computes the moment of inertia about the principle axes of an ellipsoid
 * with specified mass and semiaxes (a, b, c) corresponding to (x, y, z)
 * 
 * @param semiaxes [in] The radii of the ellipsoid in the x, y, and z directions
 * @param mass     [in] Mass of the ellipsoid
 * 
 * @return The moment of inertia about the 3 principle axes (x, y, z)
 */
constexpr Vector3 ellipsoid_inertia_tensor(const Vector3 semiaxes, float mass) noexcept
{
    float const a2 = semiaxes.x() * semiaxes.x();
    float const b2 = semiaxes.y() * semiaxes.y();
    float const c2 = semiaxes.z() * semiaxes.z();

    float const xx = b2 + c2;
    float const yy = a2 + c2;
    float const zz = a2 + b2;

    // Pre-multiply constant to avoid non-constexpr operator*(float, Vector3)
    float const c = (1.0f / 5.0f) * mass;
    return Vector3{c*xx, c*yy, c*zz};
}

} // namespace osp
