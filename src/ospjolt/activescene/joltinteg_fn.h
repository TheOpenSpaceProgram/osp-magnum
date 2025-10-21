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

#include "joltinteg.h"


#include <osp/activescene/basic.h>
#include <osp/activescene/physics.h>

#include <Corrade/Containers/ArrayView.h>

// IWYU pragma: no_include <cstdint>
// IWYU pragma: no_include <stdint.h>
// IWYU pragma: no_include <type_traits>

namespace ospjolt
{
class SysJolt
{
    using ActiveEnt                 = osp::active::ActiveEnt;
    using ACtxBasic                 = osp::active::ACtxBasic;
    using ACtxPhysics               = osp::active::ACtxPhysics;
    using ACtxSceneGraph            = osp::active::ACtxSceneGraph;
    using ACompTransform            = osp::active::ACompTransform;
    using ACompTransformStorage_t   = osp::active::ACompTransformStorage_t;
public:


    /**
     * @brief Step the entire Jolt World forward in time
     *
     * @param rCtxBasic       [ref]
     * @param rCtxPhys        [ref] Generic Physics context. Updates linear and angular velocity.
     * @param rCtxWorld       [ref] Jolt world to update
     * @param timestep        [in] Time to step world, passed to Jolt update
     */
    static void update_world(
            ACtxBasic                   &rCtxBasic,
            ACtxPhysics                 &rCtxPhys,
            ACtxJoltWorld               &rCtxWorld,
            float                       timestep) noexcept;

    static void remove_components(
            ACtxJoltWorld& rCtxWorld, ActiveEnt ent) noexcept;

    static JPH::Ref<JPH::Shape> create_primitive(ACtxJoltWorld &rCtxWorld, osp::EShape shape, JPH::Vec3Arg scale);

    template<typename IT_T>
    static void update_delete(
            ACtxJoltWorld &rCtxWorld, IT_T first, IT_T const& last) noexcept
    {
        while (first != last)
        {
            remove_components(rCtxWorld, *first);
            std::advance(first, 1);
        }
    }
    //Apply a scale to a shape.
    static void scale_shape(JPH::Ref<JPH::Shape> rShape, JPH::Vec3Arg scale);

    //Get the inverse mass of a jolt body
    static float get_inverse_mass_no_lock(JPH::PhysicsSystem const& physicsSystem, BodyId bodyId);
    
private:

    /**
     * @brief Find shapes in an entity and its hierarchy, and add them to
     *        a Jolt Compound Shape
     *
     * @param rCtxPhys      [in] Generic Physics context.
     * @param rCtxWorld     [ref] Jolt world
     * @param rHier         [in] Storage for hierarchy components
     * @param rTf           [in] Storage for relative hierarchy transforms
     * @param ent           [in] Entity to search
     * @param transform     [in] Transform relative to root (part of recursion)
     * @param rCompound     [out] Jolt CompoundShapeSettings to add shapes to
     */
    static void find_shapes_recurse(
            ACtxPhysics const&                      rCtxPhys,
            ACtxJoltWorld&                          rCtxWorld,
            ACtxSceneGraph const&                   rScnGraph,
            ACompTransformStorage_t const&          rTf,
            ActiveEnt                               ent,
            osp::Matrix4 const&                     transform,
            JPH::CompoundShapeSettings&             rCompound) noexcept;

};

}
