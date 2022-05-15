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
#include "../id_map.h"

#include "../Resource/resourcetypes.h"

#include <Magnum/Magnum.h>
#include <Magnum/Math/Color.h>

#include <longeron/id_management/registry.hpp>
#include <longeron/id_management/refcount.hpp>

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

/**
 * @brief Mesh that describes the appearance of an entity
 *
 * The renderer will synchronize this component with a GPU resource
 */
enum class MeshId : uint32_t { };

/**
 * @brief Texture component that describes the appearance of an entity
 *
 * The renderer will synchronize this component with a GPU resource
 */
enum class TexId : uint32_t { };

struct ACompColor : Magnum::Color4 {};

struct MaterialData
{
    active_sparse_set_t     m_comp;
    std::vector<ActiveEnt>  m_added;
    std::vector<ActiveEnt>  m_removed;
};

using MeshRefCount_t    = lgrn::IdRefCount<MeshId>;
using MeshIdOwner_t     = MeshRefCount_t::Owner_t;

using TexRefCount_t     = lgrn::IdRefCount<TexId>;
using TexIdOwner_t      = TexRefCount_t::Owner_t;

/**
 * @brief Mesh Ids, texture Ids, and storage for drawing-related components
 */
struct ACtxDrawing
{

    // Drawing Components
    acomp_storage_t<ACompOpaque>            m_opaque;
    acomp_storage_t<ACompTransparent>       m_transparent;
    acomp_storage_t<ACompVisible>           m_visible;
    acomp_storage_t<ACompColor>             m_color;

    // Data needed by each unique material Id: assigned entities and
    // added/removed queues. Access with m_materials[material ID]
    std::vector<MaterialData>               m_materials;

    // Scene-space Meshes
    lgrn::IdRegistry<MeshId>                m_meshIds;
    MeshRefCount_t                          m_meshRefCounts;

    // Scene-space Textures
    lgrn::IdRegistry<TexId>                 m_texIds;
    TexRefCount_t                           m_texRefCounts;

    // Meshes and textures assigned to ActiveEnts
    acomp_storage_t<TexIdOwner_t>           m_diffuseTex;
    std::vector<osp::active::ActiveEnt>     m_diffuseDirty;

    acomp_storage_t<MeshIdOwner_t>          m_mesh;
    std::vector<osp::active::ActiveEnt>     m_meshDirty;
};

/**
 * @brief Associates mesh/texture resources Ids from ACtxDrawing with Resources
 */
struct ACtxDrawingRes
{
    // Associate Texture Ids with resources
    IdMap_t<ResId, TexId>                   m_resToTex;
    IdMap_t<TexId, ResIdOwner_t>            m_texToRes;

    // Associate Mesh Ids with resources
    IdMap_t<ResId, MeshId>                  m_resToMesh;
    IdMap_t<MeshId, ResIdOwner_t>           m_meshToRes;
};

//

} // namespace osp::active

