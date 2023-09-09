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

#include "../bitvector.h"
#include "../id_map.h"
#include "../keyed_vector.h"
#include "../types.h"

#include "../Resource/resourcetypes.h"

#include <Magnum/Magnum.h>
#include <Magnum/Math/Color.h>

#include <longeron/id_management/refcount.hpp>

namespace osp::active
{


struct BasicDrawProps
{
    bool m_opaque:1         { false };
    bool m_transparent:1    { false };
};

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

/**
 * @brief Associates mesh/texture resources Ids from ACtxDrawing with Resources
 */
struct ACtxDrawingRes
{
    // Required for std::is_copy_assignable to work properly inside of entt::any
    ACtxDrawingRes() = default;
    ACtxDrawingRes(ACtxDrawingRes const& copy) = delete;
    ACtxDrawingRes(ACtxDrawingRes&& move) = default;

    // Associate Texture Ids with resources
    IdMap_t<ResId, TexId>                   m_resToTex;
    IdMap_t<TexId, ResIdOwner_t>            m_texToRes;

    // Associate Mesh Ids with resources
    IdMap_t<ResId, MeshId>                  m_resToMesh;
    IdMap_t<MeshId, ResIdOwner_t>           m_meshToRes;
};

struct ACtxSceneRender
{
    // Required for std::is_copy_assignable to work properly inside of entt::any
    ACtxSceneRender() = default;
    ACtxSceneRender(ACtxSceneRender const& copy) = delete;
    ACtxSceneRender(ACtxSceneRender&& move) = default;

    void resize_draw()
    {
        std::size_t const size = m_drawIds.capacity();

        bitvector_resize(m_visible, size);
        m_drawBasic     .resize(size);
        m_color         .resize(size);
        m_diffuseTex    .resize(size);
        m_mesh          .resize(size);
    }

    void resize_active(std::size_t const size)
    {
        bitvector_resize(m_needDrawTf, size);
        m_activeToDraw.resize(size, lgrn::id_null<DrawEnt>());
    }

    lgrn::IdRegistryStl<DrawEnt>            m_drawIds;

    DrawEntSet_t                            m_visible;
    KeyedVec<DrawEnt, BasicDrawProps>       m_drawBasic;
    KeyedVec<DrawEnt, Magnum::Color4>       m_color;

    DrawEntSet_t                            m_needDrawTf;
    KeyedVec<ActiveEnt, DrawEnt>            m_activeToDraw;

    // Meshes and textures assigned to DrawEnts
    KeyedVec<DrawEnt, TexIdOwner_t>         m_diffuseTex;
    std::vector<DrawEnt>                    m_diffuseDirty;

    KeyedVec<DrawEnt, MeshIdOwner_t>        m_mesh;
    std::vector<DrawEnt>                    m_meshDirty;

    lgrn::IdRegistryStl<MaterialId>         m_materialIds;
    KeyedVec<MaterialId, Material>          m_materials;

    //DrawTransforms_t        m_drawTransform;
};

struct Camera
{
    Matrix4 m_transform;

    float m_near{0.25f};
    float m_far{1024.0f};
    float m_aspectRatio{1.0f};
    Deg m_fov{45.0f};

    constexpr void set_aspect_ratio(Vector2 const viewport) noexcept
    {
        m_aspectRatio = viewport.x() / viewport.y();
    }

    [[nodiscard]] Matrix4 perspective() const noexcept
    {
        return Matrix4::perspectiveProjection(m_fov, m_aspectRatio, m_near, m_far);
    }
};

} // namespace osp::active

