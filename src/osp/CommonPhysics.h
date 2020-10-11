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

#include "types.h"

namespace osp::phys
{

// Formerly PrototypePart
enum class ECollisionShape : uint8_t
{
    NONE = 0,
    COMBINED = 1,
    SPHERE = 2,
    BOX = 3,
    CAPSULE = 4,
    CYLINDER = 5,
    //MESH = 6,
    CONVEX_HULL = 7,
    TERRAIN = 8
};

/**
 * Compute the volume of an ECollisionShape
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
float shape_volume(ECollisionShape shape, Vector3 scale);

// Formerly SysNewton
/**
 * Generic rigid body state
 */
struct DataRigidBody
{
    // Modify these
    Vector3 m_inertia{1, 1, 1};
    Vector3 m_netForce{0, 0, 0};
    Vector3 m_netTorque{0, 0, 0};

    float m_mass{1.0f};
    Vector3 m_velocity{0, 0, 0};
    Vector3 m_rotVelocity{0, 0, 0};
    Vector3 m_centerOfMassOffset{0, 0, 0};

    bool m_colliderDirty{false}; // set true if collider is modified
    bool m_inertiaDirty{false}; // set true if rigidbody is modified
};

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
Vector3 collider_inertia_tensor(ECollisionShape shape, Vector3 scale, float mass);

/**
 * Compute the inertia tensor for a cylinder
 *
 * Computes the moment of inertia about the principal axes of a cylinder
 * with specified mass, height, and radius, whose axis of symmetry lies
 * along the z-axis
 *
 * @return The moment of inertia about the 3 principal axes (x, y, z)
 */
constexpr Vector3 cylinder_inertia_tensor(float radius, float height, float mass)
{
    float r2 = radius * radius;
    float h2 = height * height;
    float xx = (1.0f / 12.0f) * (3.0f*r2 + h2);
    float yy = xx;
    float zz = r2 / 2.0f;

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
constexpr Vector3 cuboid_inertia_tensor(const Vector3 dimensions, float mass)
{
    float x2 = dimensions.x() * dimensions.x();
    float y2 = dimensions.y() * dimensions.y();
    float z2 = dimensions.z() * dimensions.z();

    float xx = y2 + z2;
    float yy = x2 + z2;
    float zz = x2 + y2;

    // Pre-multiply constant to avoid non-constexpr operator*(float, Vector3)
    float c = (1.0f / 12.0f) * mass;
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
constexpr Vector3 ellipsoid_inertia_tensor(const Vector3 semiaxes, float mass)
{
    float a2 = semiaxes.x() * semiaxes.x();
    float b2 = semiaxes.y() * semiaxes.y();
    float c2 = semiaxes.z() * semiaxes.z();

    float xx = b2 + c2;
    float yy = a2 + c2;
    float zz = a2 + b2;

    // Pre-multiply constant to avoid non-constexpr operator*(float, Vector3)
    float c = (1.0f / 5.0f) * mass;
    return Vector3{c*xx, c*yy, c*zz};
}

} // namespace osp::phys
