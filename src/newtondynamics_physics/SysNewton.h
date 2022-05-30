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

#include <osp/Active/physics.h>      // for ACompShape
#include <osp/Active/basic.h>        // for ActiveEnt, ActiveReg_t, basic_sp...

#include <osp/types.h>               // for Vector3, Matrix4
#include <osp/CommonPhysics.h>       // for ECollisionShape, ECollisionShape...

#include <Corrade/Containers/ArrayView.h>

// IWYU pragma: no_include <cstdint>
// IWYU pragma: no_include <stdint.h>
// IWYU pragma: no_include <type_traits>

class NewtonBody;
class NewtonWorld;
class NewtonCollision;

namespace ospnewton
{

struct ACtxNwtWorld;

class SysNewton
{
    template<typename COMP_T>
    using acomp_storage_t           = osp::active::acomp_storage_t<COMP_T>;

    using ActiveEnt                 = osp::active::ActiveEnt;
    using ACtxPhysics               = osp::active::ACtxPhysics;
    using ACompHierarchy            = osp::active::ACompHierarchy;
    using ACompTransform            = osp::active::ACompTransform;
    using ACompTransformControlled  = osp::active::ACompTransformControlled;
    using ACompTransformMutable     = osp::active::ACompTransformMutable;
public:

    /**
     * @brief Respond to scene origin shifts by translating all rigid bodies
     *
     * @param rCtxPhys      [ref] Generic physics context with m_originTranslate
     * @param rCtxWorld     [ref] Newton World
     */
    static void update_translate(
            ACtxPhysics& rCtxPhys,
            ACtxNwtWorld& rCtxWorld) noexcept;

    /**
     * @brief Synchronize generic physics colliders with Newton colliders
     *
     * @param rCtxPhys          [ref] Generic physics context
     * @param rCtxWorld         [ref] Newton World
     * @param rCollidersDirty   [in] Colliders to update
     */
    static void update_colliders(
            ACtxPhysics& rCtxPhys,
            ACtxNwtWorld& rCtxWorld,
            std::vector<ActiveEnt> const& collidersDirty) noexcept;

    /**
     * @brief Step the entire Newton World forward in time
     *
     * @param rCtxPhys      [ref] Generic Physics context. Updates linear and angular velocity.
     * @param rCtxWorld     [ref] Newton world to update
     * @param timestep      [in] Time to step world, passed to Newton update
     * @param inputs        [ref] Physics inputs (from different threads)
     * @param rHier         [in] Storage for Hierarchy components
     * @param rTf           [ref] Relative transforms used by rigid bodies
     * @param rTfControlled [ref] Flags for controlled transforms
     * @param rTfMutable    [ref] Flags for mutable transforms
     */
    static void update_world(
            ACtxPhysics& rCtxPhys,
            ACtxNwtWorld& rCtxWorld,
            float timestep,
            Corrade::Containers::ArrayView<osp::active::ACtxPhysInputs> inputs,
            acomp_storage_t<osp::active::ACompHierarchy> const& rHier,
            acomp_storage_t<osp::active::ACompTransform>& rTf,
            acomp_storage_t<osp::active::ACompTransformControlled>& rTfControlled,
            acomp_storage_t<osp::active::ACompTransformMutable>& rTfMutable) noexcept;

    static void remove_components(
            ACtxNwtWorld& rCtxWorld, ActiveEnt ent) noexcept;

    template<typename IT_T>
    static void update_delete(
            ACtxNwtWorld &rCtxWorld, IT_T first, IT_T const& last) noexcept
    {
        while (first != last)
        {
            remove_components(rCtxWorld, *first);
            std::advance(first, 1);
        }
    }

private:

    /**
     * @brief Find colliders in an entity and its hierarchy, and add them to
     *        a Newton Compound Collision
     *
     * @param rCtxPhys      [in] Generic Physics context.
     * @param rCtxWorld     [ref] Newton world
     * @param rHier         [in] Storage for hierarchy components
     * @param rTf           [in] Storage for relative hierarchy transforms
     * @param ent           [in] Entity to search
     * @param firstChild    [in] First child of ent (to reduce rHier accesses)
     * @param transform     [in] Transform relative to root (part of recursion)
     * @param pCompound     [out] NewtonCompoundCollision to add colliders to
     */
    static void find_colliders_recurse(
            ACtxPhysics const& rCtxPhys,
            ACtxNwtWorld& rCtxWorld,
            acomp_storage_t<ACompHierarchy> const& rHier,
            acomp_storage_t<ACompTransform> const& rTf,
            ActiveEnt ent,
            ActiveEnt firstChild,
            osp::Matrix4 const& transform,
            NewtonCollision* pCompound) noexcept;

    /**
     * @brief Create Newton bodies and colliders for entities with ACompPhysBody
     *
     * @param rScene    [ref] ActiveScene containing entity and physics world
     * @param entity    [in] Entity containing ACompNwtBody
     * @param pNwtWorld [in] Newton physics world
     */
    static void create_body(
            ACtxPhysics const& rCtxPhys,
            ACtxNwtWorld& rCtxWorld,
            acomp_storage_t<ACompHierarchy> const& rHier,
            acomp_storage_t<ACompTransform> const& rTf,
            acomp_storage_t<ACompTransformControlled>& rTfControlled,
            acomp_storage_t<ACompTransformMutable>& rTfMutable,
            ActiveEnt ent,
            NewtonWorld const* pNwtWorld) noexcept;

};

}

