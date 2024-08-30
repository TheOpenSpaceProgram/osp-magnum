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
    using ACtxPhysics               = osp::active::ACtxPhysics;
    using ACtxSceneGraph            = osp::active::ACtxSceneGraph;
    using ACompTransform            = osp::active::ACompTransform;
    using ACompTransformStorage_t   = osp::active::ACompTransformStorage_t;
public:


    static void resize_body_data(ACtxJoltWorld& rCtxWorld);

    /**
     * @brief Respond to scene origin shifts by translating all rigid bodies
     *
     * @param rCtxPhys      [ref] Generic physics context with m_originTranslate
     * @param rCtxWorld     [ref] Jolt World
     */
    static void update_translate(
            ACtxPhysics& rCtxPhys,
            ACtxJoltWorld& rCtxWorld) noexcept;

    /**
     * @brief Step the entire Jolt World forward in time
     *
     * @param rCtxPhys      [ref] Generic Physics context. Updates linear and angular velocity.
     * @param rCtxWorld     [ref] Jolt world to update
     * @param timestep      [in] Time to step world, passed to Jolt update
     * @param rTf           [ref] Relative transforms used by rigid bodies
     */
    static void update_world(
            ACtxPhysics&                            rCtxPhys,
            ACtxJoltWorld&                          rCtxWorld,
            float                                   timestep,
            osp::active::ACompTransformStorage_t&   rTf) noexcept;

    static void remove_components(
            ACtxJoltWorld& rCtxWorld, ActiveEnt ent) noexcept;

    static Ref<Shape> create_primitive(ACtxJoltWorld &rCtxWorld, osp::EShape shape, Vec3Arg scale);

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
    static void scale_shape(Ref<Shape> rShape, Vec3Arg scale);

    //Get the inverse mass of a jolt body
    static float get_inverse_mass_no_lock(PhysicsSystem& physicsSystem, BodyId bodyId);
    
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
            CompoundShapeSettings&                  rCompound) noexcept;

};

}
