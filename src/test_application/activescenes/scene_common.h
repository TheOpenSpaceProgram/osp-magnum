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
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#pragma once

#include "scenarios.h"

#include <osp/Active/activetypes.h>
#include <osp/CommonPhysics.h>
#include <osp/Active/drawing.h>

#include <entt/container/dense_map.hpp>

namespace testapp::scenes
{

inline auto const delete_ent_set = osp::wrap_args([] (osp::active::EntSet_t& rSet, osp::active::EntVector_t const& rDelTotal) noexcept
{
    for (osp::active::ActiveEnt const ent : rDelTotal)
    {
        rSet.reset(std::size_t(ent));
    }
});

struct NamedMeshes
{
    // Required for std::is_copy_assignable to work properly inside of entt::any
    NamedMeshes() = default;
    NamedMeshes(NamedMeshes const& copy) = delete;
    NamedMeshes(NamedMeshes&& move) = default;

    entt::dense_map<osp::phys::EShape,
                    osp::active::MeshIdOwner_t> m_shapeToMesh;
    entt::dense_map<std::string_view,
                    osp::active::MeshIdOwner_t> m_namedMeshs;
};

/**
 * @brief Support for Time, ActiveEnts, Hierarchy, Transforms, Drawing, and more...
 */
osp::Session setup_common_scene(
        Builder_t&                  rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Tags&                  rTags,
        osp::TopDataId              idResources,
        osp::PkgId                  pkg);

/**
 * @brief Support a single material, aka: a Set of ActiveEnts and dirty flags
 *
 * Multiple material sessions can be setup for each material
 */
osp::Session setup_material(
        Builder_t&                  rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Tags&                  rTags,
        osp::Session const&         scnCommon);

}
