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

#include <optional>
#include <vector>

namespace osp
{
    using Corrade::Containers::ArrayView;
}

namespace osp::active
{

/**
 * @brief Data needed to initialize a Prefab
 *
 * This allows separate systems (doing physics, hierarchy, drawables, ...) to
 * work in parallel on initializing a prefab.
 *
 * Intended to be created and quickly destroyed once the prefab is created.
 * This is often the span of a single frame.
 */
struct TmpPrefabInitBasic
{
    ResId m_importerRes{lgrn::id_null<ResId>()};
    PrefabId m_prefabId;

    // Parent and transform to assign root objects in the prefab
    ActiveEnt m_parent{lgrn::id_null<ActiveEnt>()};
    Matrix4 const* m_pTransform{nullptr};
};


struct ACtxPrefabInit
{
    std::vector<TmpPrefabInitBasic>             m_basic;
    std::vector< ArrayView<ActiveEnt const> >   m_ents;

    std::vector<ActiveEnt>                      m_newEnts;
};


class SysPrefabInit
{
public:

    static void init_subtrees(
            ACtxPrefabInit const&               rPrefabInit,
            Resources const&                    rResources,
            ACtxSceneGraph&                     rScnGraph) noexcept;

    static void init_transforms(
            ACtxPrefabInit const&               rPrefabInit,
            Resources const&                    rResources,
            acomp_storage_t<ACompTransform>&    rTransform) noexcept;

    static void init_drawing(
            ACtxPrefabInit const&               rPrefabInit,
            Resources&                          rResources,
            ACtxDrawing&                        rCtxDraw,
            ACtxDrawingRes&                     rCtxDrawRes,
            std::optional<EntSetPair>           material) noexcept;

    static void init_physics(
            ACtxPrefabInit const&               rPrefabInit,
            Resources const&                    rResources,
            ACtxPhysInputs&                     rPhysIn,
            ACtxPhysics&                        rCtxPhys,
            ACtxHierBody&                       rCtxHierBody) noexcept;
};



} // namespace osp::active
