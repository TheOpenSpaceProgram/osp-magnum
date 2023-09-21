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

#include <osp/activescene/basic.h>
#include <osp/drawing/drawing.h>
#include <osp/scientific/shapes.h>

#include <entt/container/dense_map.hpp>

namespace testapp::scenes
{

struct NamedMeshes
{
    // Required for std::is_copy_assignable to work properly inside of entt::any
    NamedMeshes() = default;
    NamedMeshes(NamedMeshes const& copy) = delete;
    NamedMeshes(NamedMeshes&& move) = default;

    entt::dense_map<osp::EShape, osp::draw::MeshIdOwner_t>      m_shapeToMesh;
    entt::dense_map<std::string_view, osp::draw::MeshIdOwner_t> m_namedMeshs;
};

osp::Session setup_scene(
        osp::TopTaskBuilder&                rBuilder,
        osp::ArrayView<entt::any>           topData,
        osp::Session const&                 application);

/**
 * @brief Support for Time, ActiveEnts, Hierarchy, Transforms, Drawing, and more...
 */
osp::Session setup_common_scene(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         scene,
        osp::Session const&         application,
        osp::PkgId                  pkg);


}
