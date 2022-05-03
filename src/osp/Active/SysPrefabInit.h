/**
 * Open Space Program
 * Copyright © 2019-2022 Open Space Program Project
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
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHEfR DEALINGS IN THE
 * SOFTWARE.
 */
#pragma once

#include "activetypes.h"
#include "basic.h"
#include "drawing.h"
#include "physics.h"

#include "../types.h"
#include "../Resource/resourcetypes.h"

#include <Corrade/Containers/ArrayView.h>

namespace osp::active
{

/**
 * @brief Data needed to initialize a Prefab
 *
 * This allows separate systems (doing physics, hierarchy, drawables, ...) to
 * work in arallel on initializing a prefab.
 *
 * Intended to be created and quickly destroyed once the prefab is created.
 * This is often the span of a single frame.
 */
struct TmpPrefabInit
{
    ResId m_importerRes{lgrn::id_null<ResId>()};
    PrefabId m_prefabId;

    // Entities that make up the prefab
    Corrade::Containers::ArrayView<ActiveEnt const> m_prefabToEnt;

    // Parent and transform to assign root objects in the prefab
    ActiveEnt m_parent{lgrn::id_null<ActiveEnt>()};
    Matrix4 const* m_pTransform{nullptr};
};


class SysPrefabInit
{
public:

    using PrefabInitView_t = Corrade::Containers::ArrayView<TmpPrefabInit const>;

    static void init_hierarchy(
            PrefabInitView_t                    prefabInits,
            Resources const&                    rResources,
            acomp_storage_t<ACompHierarchy>&    rHier) noexcept;

    static void init_transforms(
            PrefabInitView_t                    prefabInits,
            Resources const&                    rResources,
            acomp_storage_t<ACompTransform>&    rTransform) noexcept;

    static void init_drawing(
            PrefabInitView_t                    prefabInits,
            Resources&                          rResources,
            ACtxDrawing&                        rCtxDraw,
            ACtxDrawingRes&                     rCtxDrawRes,
            int                                 materialId) noexcept;

    static void init_physics(
            PrefabInitView_t                    prefabInits,
            Resources const&                    rResources,
            ACtxPhysInputs&                     rPhysIn,
            ACtxPhysics&                        rCtxPhys,
            ACtxHierBody&                       rCtxHierBody) noexcept;
};



} // namespace osp::active
