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

public:

    static void destroy(ACtxNwtWorld &rCtxWorld);

    static void update_translate(
            osp::active::ACtxPhysics& rCtxPhys,
            ACtxNwtWorld& rCtxWorld);

    static void update_colliders(
            osp::active::ACtxPhysics& rCtxPhys,
            ACtxNwtWorld& rCtxWorld,
            std::vector<osp::active::ActiveEnt>& rCollidersDirty);

    static void update_world(
            osp::active::ACtxPhysics& rCtxPhys,
            ACtxNwtWorld& rCtxWorld,
            Corrade::Containers::ArrayView<osp::active::ACtxPhysInputs> inputs,
            osp::active::acomp_storage_t<osp::active::ACompHierarchy> const& rHier,
            osp::active::acomp_storage_t<osp::active::ACompTransform>& rTf,
            osp::active::acomp_storage_t<osp::active::ACompTransformControlled>& rTfControlled,
            osp::active::acomp_storage_t<osp::active::ACompTransformMutable>& rTfMutable);

    static NewtonWorld const* debug_get_world();

private:

    /**
     * Search descendents for collider components and add NewtonCollisions to a
     * vector. Make sure NewtonCompoundCollisionBeginAddRemove(rCompound) is
     * called first.
     *
     * @param rScene    [in] ActiveScene containing entity and physics world
     * @param ent       [in] Entity containing colliders, and recurse into children
     * @param transform [in] Total transform from Hierarchy
     * @param nwtWorld  [in] Newton world from scene
     * @param rCompound [out] Compound collision to add new colliders to
     */
    static void find_colliders_recurse(
            osp::active::ACtxPhysics& rCtxPhys,
            ACtxNwtWorld& rCtxWorld,
            osp::active::acomp_storage_t<osp::active::ACompHierarchy> const& rHier,
            osp::active::acomp_storage_t<osp::active::ACompTransform> const& rTf,
            osp::active::ActiveEnt ent,
            osp::active::ActiveEnt firstChild,
            osp::Matrix4 const& transform,
            NewtonCollision* pCompound);

    /**
     * @brief Create Newton bodies and colliders for entities with ACompPhysBody
     *
     * @param rScene    [ref] ActiveScene containing entity and physics world
     * @param entity    [in] Entity containing ACompNwtBody
     * @param pNwtWorld [in] Newton physics world
     */
    static void create_body(
            osp::active::ACtxPhysics& rCtxPhys,
            ACtxNwtWorld& rCtxWorld,
            osp::active::acomp_storage_t<osp::active::ACompHierarchy> const& rHier,
            osp::active::acomp_storage_t<osp::active::ACompTransform> const& rTf,
            osp::active::acomp_storage_t<osp::active::ACompTransformControlled>& rTfControlled,
            osp::active::acomp_storage_t<osp::active::ACompTransformMutable>& rTfMutable,
            osp::active::ActiveEnt ent,
            NewtonWorld const* pNwtWorld);

};

}

