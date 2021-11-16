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

#include "activetypes.h"
#include "basic.h"
#include "../types.h"

#include "../Resource/Resource.h"

#include <Corrade/Containers/ArrayView.h>
#include <entt/core/family.hpp>

// MSVC freaks out if these are forward declared
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/MeshData.h>

#include <functional>
#include <unordered_map>

namespace osp::active
{

/**
 * @brief An object that is completely opaque
 */
struct ACompOpaque {};

/**
 *  @brief An object with transparency
 */
struct ACompTransparent {};

/**
 * @brief Visibility state of this object
 */
struct ACompVisible {};

/**
 * @brief World transform used for rendering.
 *
 * All ascendents of an entity using this must also have this component
 */
struct ACompDrawTransform
{
    Matrix4 m_transformWorld;
};

struct ACompMesh
{
    osp::DependRes<Magnum::Trade::MeshData> m_mesh;
};

struct ACompTexture
{
    osp::DependRes<Magnum::Trade::ImageData2D> m_texture;
};

struct MaterialData
{
    active_sparse_set_t     m_comp;
    std::vector<ActiveEnt>  m_added;
    std::vector<ActiveEnt>  m_removed;
};

/**
 * @brief Storage for Drawing components
 */
struct ACtxDrawing
{
    std::vector<MaterialData>               m_materials;
    acomp_storage_t<ACompOpaque>            m_opaque;
    acomp_storage_t<ACompTransparent>       m_transparent;
    acomp_storage_t<ACompVisible>           m_visible;
    acomp_storage_t<ACompDrawTransform>     m_drawTransform;
    acomp_storage_t<ACompMesh>              m_mesh;

    acomp_storage_t<ACompTexture>           m_diffuseTex;

    std::vector<osp::active::ActiveEnt>     m_meshDirty;
    std::vector<osp::active::ActiveEnt>     m_diffuseDirty;
};

} // namespace osp::active

