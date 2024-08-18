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

#include "draw_ent.h"

#include "../core/copymove_macros.h"
#include "../core/id_map.h"
#include "../core/keyed_vector.h"
#include "../core/math_types.h"
#include "../core/resourcetypes.h"

#include "../activescene/active_ent.h"
#include "../activescene/basic.h"

#include "../scientific/shapes.h"

#include <Magnum/Magnum.h>
#include <Magnum/Math/Color.h>

#include <longeron/id_management/refcount.hpp>
#include <longeron/id_management/registry_stl.hpp>
#include <longeron/id_management/id_set_stl.hpp>

namespace osp::draw
{

using DrawEntVec_t  = std::vector<DrawEnt>;
using DrawEntSet_t  = lgrn::IdSetStl<DrawEnt>;

struct Material
{
    DrawEntSet_t m_ents;
    DrawEntVec_t m_dirty;
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


using MeshRefCount_t    = lgrn::IdRefCount<MeshId>;
using MeshIdOwner_t     = MeshRefCount_t::Owner_t;

using TexRefCount_t     = lgrn::IdRefCount<TexId>;
using TexIdOwner_t      = TexRefCount_t::Owner_t;

/**
 * @brief Mesh Ids, texture Ids, and storage for drawing-related components
 */
struct ACtxDrawing
{
    // Scene-space Meshes
    lgrn::IdRegistryStl<MeshId>             m_meshIds;
    MeshRefCount_t                          m_meshRefCounts;

    // Scene-space Textures
    lgrn::IdRegistryStl<TexId>              m_texIds;
    TexRefCount_t                           m_texRefCounts;
};

// Note: IdOwner members below are move-only. Move constructors need to be defined for the structs
//       (can use OSP_MOVE_ONLY_CTOR_ASSIGN) or else there will be huge compiler errors somehow.

/**
 * @brief Associates mesh/texture resources Ids from ACtxDrawing with Resources
 */
struct ACtxDrawingRes
{
    ACtxDrawingRes() = default;
    OSP_MOVE_ONLY_CTOR_ASSIGN(ACtxDrawingRes);

    // Associate Texture Ids with resources
    IdMap_t<ResId, TexId>                   m_resToTex;
    IdMap_t<TexId, ResIdOwner_t>            m_texToRes;

    // Associate Mesh Ids with resources
    IdMap_t<ResId, MeshId>                  m_resToMesh;
    IdMap_t<MeshId, ResIdOwner_t>           m_meshToRes;
};

using DrawEntColors_t = KeyedVec<DrawEnt, Magnum::Color4>;
using DrawEntTextures_t = KeyedVec<DrawEnt, TexIdOwner_t>;
using DrawTransforms_t = KeyedVec<DrawEnt, Matrix4>;

struct ACtxSceneRender
{
    ACtxSceneRender() = default;
    OSP_MOVE_ONLY_CTOR_ASSIGN(ACtxSceneRender);

    void resize_draw()
    {
        std::size_t const size = m_drawIds.capacity();

        m_opaque.resize(size);
        m_transparent.resize(size);
        m_visible.resize(size);

        m_drawTransform .resize(size);
        m_color         .resize(size, {1.0f, 1.0f, 1.0f, 1.0f}); // Default white
        m_diffuseTex    .resize(size);
        m_mesh          .resize(size);

        for (MaterialId matId : m_materialIds)
        {
            m_materials[matId].m_ents.resize(size);
        }
    }

    void resize_active(std::size_t const size)
    {
        m_needDrawTf.resize(size);
        m_activeToDraw      .resize(size, lgrn::id_null<DrawEnt>());
        drawTfObserverEnable.resize(size, 0);
    }

    lgrn::IdRegistryStl<DrawEnt>            m_drawIds;

    DrawEntSet_t                            m_opaque;
    DrawEntSet_t                            m_transparent;
    DrawEntSet_t                            m_visible;
    DrawEntColors_t                         m_color;

    active::ActiveEntSet_t                  m_needDrawTf;
    KeyedVec<active::ActiveEnt, DrawEnt>    m_activeToDraw;

    KeyedVec<active::ActiveEnt, uint16_t>   drawTfObserverEnable;
    DrawTransforms_t                        m_drawTransform;

    // Meshes and textures assigned to DrawEnts
    KeyedVec<DrawEnt, TexIdOwner_t>         m_diffuseTex;
    DrawEntVec_t                            m_diffuseDirty;

    KeyedVec<DrawEnt, MeshIdOwner_t>        m_mesh;
    DrawEntVec_t                            m_meshDirty;

    lgrn::IdRegistryStl<MaterialId>         m_materialIds;
    KeyedVec<MaterialId, Material>          m_materials;
};

struct Camera
{
    Matrix4 m_transform;

    float   m_near          { 0.25f };
    float   m_far           { 1024.0f };
    float   m_aspectRatio   { 1.0f };
    Deg     m_fov           { 45.0f };

    constexpr void set_aspect_ratio(Vector2 const viewport) noexcept
    {
        m_aspectRatio = viewport.x() / viewport.y();
    }

    [[nodiscard]] Matrix4 perspective() const noexcept
    {
        return Matrix4::perspectiveProjection(m_fov, m_aspectRatio, m_near, m_far);
    }
};


struct NamedMeshes
{
    NamedMeshes() = default;
    OSP_MOVE_ONLY_CTOR_ASSIGN(NamedMeshes) // Huge compile errors without this. MeshIdOwner_t is move-only.

    entt::dense_map<osp::EShape, osp::draw::MeshIdOwner_t>      m_shapeToMesh;
    entt::dense_map<std::string_view, osp::draw::MeshIdOwner_t> m_namedMeshs;
};

} // namespace osp::draw

