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

#include "basic_fn.h"
#include "physics.h"

#include "../core/resourcetypes.h"
#include "../vehicles/prefabs.h"

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
    Matrix4 const* m_pTransform{nullptr};
};


struct ACtxPrefabInit
{
    osp::active::ActiveEntSet_t                 roots;

    std::vector<TmpPrefabInitBasic>             spawnRequest;
    std::vector< ArrayView<ActiveEnt const> >   spawnedEntsOffset;
    std::vector<ActiveEnt>                      newEnts;
};


class SysPrefabInit
{
public:

    static void create_activeents(
            ACtxPrefabInit&             rPrefabInit,
            ACtxBasic&                  rBasic,
            Resources&                  rResources);

    static void add_to_subtree(
            TmpPrefabInitBasic const&   basic,
            ArrayView<ActiveEnt const>  ents,
            Resources const&            rResources,
            SubtreeBuilder&             rSubtree) noexcept;

    static void init_transforms(
            ACtxPrefabInit const&       rPrefabInit,
            Resources const&            rResources,
            ACompTransformStorage_t&    rTransform) noexcept;

    static void init_physics(
            ACtxPrefabInit const&       rPrefabInit,
            Resources const&            rResources,
            ACtxPhysics&                rCtxPhys) noexcept;
};


} // namespace osp::active
