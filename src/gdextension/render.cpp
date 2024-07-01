/**
 * Open Space Program
 * Copyright © 2019-2021 Open Space Program Project
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

#include "render.h"

#include <Corrade/Containers/ArrayViewStl.h>
#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/RenderbufferFormat.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/ImageView.h>
#include <Magnum/Mesh.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/TextureData.h>
#include <cstdint>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/input_map.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/surface_tool.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <osp/core/Resources.h>
#include <osp/drawing/own_restypes.h>
#include <osp/util/logging.h>

using Magnum::Trade::ImageData2D;
using Magnum::Trade::MeshData;
using Magnum::Trade::TextureData;

using Magnum::GL::Mesh;
using Magnum::GL::Texture2D;

using osp::ResId;

using osp::draw::RenderGd;
using osp::draw::SysRenderGd;

using osp::draw::MeshGdId;
using osp::draw::TexGdId;

constexpr godot::Image::Format formatMToGd(Magnum::PixelFormat pFormat)
{
    switch ( pFormat )
    {

    case Magnum::PixelFormat::R8Unorm:
        return godot::Image::FORMAT_R8;
    case Magnum::PixelFormat::RG8Unorm:
        return godot::Image::FORMAT_RG8;
    case Magnum::PixelFormat::RGB8Unorm:
        return godot::Image::FORMAT_RGB8;
    case Magnum::PixelFormat::RGBA8Unorm:
        return godot::Image::FORMAT_RGBA8;
    case Magnum::PixelFormat::R8Srgb:
        return godot::Image::FORMAT_R8;
    case Magnum::PixelFormat::RG8Srgb:
        return godot::Image::FORMAT_RG8;
    case Magnum::PixelFormat::RGB8Srgb:
        return godot::Image::FORMAT_RGB8;
    case Magnum::PixelFormat::RGBA8Srgb:
        return godot::Image::FORMAT_RGBA8;
    case Magnum::PixelFormat::RGB32F:
        return godot::Image::FORMAT_RGBF;
    case Magnum::PixelFormat::RGBA32F:
        return godot::Image::FORMAT_RGBAF;
    default:
        return godot::Image::FORMAT_MAX;
    }
}

constexpr godot::RenderingServer::PrimitiveType primitiveMToGd(Magnum::MeshPrimitive primitive)
{
    switch ( primitive )
    {

    case Magnum::MeshPrimitive::Points:
        return godot::RenderingServer::PrimitiveType::PRIMITIVE_POINTS;
    case Magnum::MeshPrimitive::Lines:
        return godot::RenderingServer::PrimitiveType::PRIMITIVE_LINES;
    case Magnum::MeshPrimitive::LineStrip:
        return godot::RenderingServer::PrimitiveType::PRIMITIVE_LINE_STRIP;
    case Magnum::MeshPrimitive::Triangles:
        return godot::RenderingServer::PrimitiveType::PRIMITIVE_TRIANGLES;
    case Magnum::MeshPrimitive::TriangleStrip:
        return godot::RenderingServer::PrimitiveType::PRIMITIVE_TRIANGLE_STRIP;
    default:
        return godot::RenderingServer::PrimitiveType::PRIMITIVE_MAX;
    }
}

void SysRenderGd::compile_resource_textures(
    ACtxDrawingRes const &rCtxDrawRes, Resources &rResources, RenderGd &rRenderGd)
{
    // TODO: Eventually have dirty flags instead of checking every entry.

    for ( [[maybe_unused]] auto const &[_, scnOwner] : rCtxDrawRes.m_texToRes )
    {
        ResId const texRes       = scnOwner.value();

        // New element will be emplaced if it isn't present yet
        auto const [it, success] = rRenderGd.m_resToTex.try_emplace(texRes);
        if ( ! success )
        {
            continue;
        }

        // New element emplaced, this means we've just found a resource that
        // isn't synchronized yet.

        // Create new Texture GL Id
        TexGdId const newId      = rRenderGd.m_texIds.create();

        // Create owner, this adds to the resource's reference count
        ResIdOwner_t renderOwner = rResources.owner_create(restypes::gc_texture, texRes);

        // Track with two-way map and store owner
        rRenderGd.m_texToRes.emplace(newId, std::move(renderOwner));
        it->second          = newId;

        ResId const imgRes  = rResources.data_get<TextureImgSource>(restypes::gc_texture, texRes);
        auto const &texData = rResources.data_get<TextureData>(restypes::gc_texture, texRes);
        auto const &imgData = rResources.data_get<ImageData2D>(restypes::gc_image, imgRes);

        if ( texData.type() != Magnum::Trade::TextureType::Texture2D )
        {

            OSP_LOG_WARN("Unsupported texture type for texture resource: {}",
                         rResources.name(restypes::gc_texture, texRes));
            continue;
        }
        godot::RenderingServer  *rs = godot::RenderingServer::get_singleton();
        godot::Ref<godot::Image> img =
            godot::Image::create_from_data(imgData.size().x(),
                                           imgData.size().y(),
                                           true,
                                           formatMToGd(imgData.format()),
                                           godot::Variant(imgData.data()));
        godot::RID rid = rs->texture_2d_create(img);
        rRenderGd.m_texGd.emplace(newId, rid);
    }
}

void SysRenderGd::compile_resource_meshes(
    ACtxDrawingRes const &rCtxDrawRes, Resources &rResources, RenderGd &rRenderGd)
{
    // TODO: Eventually have dirty flags instead of checking every entry.

    for ( [[maybe_unused]] auto const &[_, scnOwner] : rCtxDrawRes.m_meshToRes )
    {
        ResId const meshRes      = scnOwner.value();

        // New element will be emplaced if it isn't present yet
        auto const [it, success] = rRenderGd.m_resToMesh.try_emplace(meshRes);
        if ( ! success )
        {
            continue;
        }

        // New element emplaced, this means we've just found a resource that
        // isn't synchronized yet.

        // Create new Mesh GL Id
        MeshGdId const newId     = rRenderGd.m_meshIds.create();

        // Create owner, this adds to the resource's reference count
        ResIdOwner_t renderOwner = rResources.owner_create(restypes::gc_mesh, meshRes);

        // Track with two-way map and store owner
        rRenderGd.m_meshToRes.emplace(newId, std::move(renderOwner));
        it->second                 = newId;

        // Get mesh data
        MeshData const &meshData   = rResources.data_get<MeshData>(restypes::gc_mesh, meshRes);
        auto            primitive  = primitiveMToGd(meshData.primitive());

        godot::RenderingServer *rs = godot::RenderingServer::get_singleton();

        godot::SurfaceTool      st;
        // why are there two different PrimitiveType enums ?
        st.begin(static_cast<godot::Mesh::PrimitiveType>(primitive));

        godot::RID mesh = rs->mesh_create();

        // TODO copy other attributes as well maybe.
        for ( auto v : meshData.positions3DAsArray() )
        {
            //st.set_uv({0, 0});
            st.add_vertex(godot::Vector3(v.x(), v.y(), v.z()));
        }
        auto indices = meshData.indicesAsArray();
        //TODO do that using an iterator I guess.
        for ( auto i = indices.end() - 1; i >= indices.begin(); i--)
        {
            st.add_index(static_cast<int32_t>(*i));
        }
        if ( primitive == godot::RenderingServer::PRIMITIVE_TRIANGLES )
        {
            //st.deindex();
            st.generate_normals();
            //st.generate_tangents();
            
        }

        godot::Array meshArray = st.commit_to_arrays();

        rs->mesh_add_surface_from_arrays(mesh, primitive, meshArray);

        rRenderGd.m_meshGd.emplace(newId, mesh);
    }
}

void SysRenderGd::sync_drawent_mesh(DrawEnt const                           ent,
                                    KeyedVec<DrawEnt, MeshIdOwner_t> const &cmpMeshIds,
                                    IdMap_t<MeshId, ResIdOwner_t> const    &meshToRes,
                                    MeshGdEntStorage_t                     &rCmpMeshGl,
                                    InstanceGdEntStorage_t                 &rCmpInstanceGd,
                                    RenderGd                               &rRenderGd)
{
    ACompMeshGd         &rEntMeshGl   = rCmpMeshGl[ent];
    MeshIdOwner_t const &entMeshScnId = cmpMeshIds[ent];

    // Make sure dirty entity has a MeshId component
    if ( entMeshScnId.has_value() )
    {
        // Check if scene mesh ID is properly synchronized
        if ( rEntMeshGl.m_scnId == entMeshScnId )
        {
            return; // No changes needed
        }

        rEntMeshGl.m_scnId = entMeshScnId;

        // Check if MeshId is associated with a resource
        if ( auto const &foundIt = meshToRes.find(entMeshScnId); foundIt != meshToRes.end() )
        {
            ResId const meshResId = foundIt->second;

            // Mesh should have been loaded beforehand, assign it!
            rEntMeshGl.m_glId     = rRenderGd.m_resToMesh.at(meshResId);
        }
        else
        {
            OSP_LOG_WARN("No mesh data found for Mesh {} from Entity {}",
                         std::size_t(entMeshScnId.value()),
                         std::size_t(ent));
        }
    }
    else
    {
        if ( rEntMeshGl.m_glId != lgrn::id_null<MeshGdId>() )
        {
            // ACompMesh removed, remove ACompMeshGL too
            rEntMeshGl = {};
        }
        else
        {
            // Why is this entity here?
        }
    }
}

void SysRenderGd::sync_drawent_texture(DrawEnt const                          ent,
                                       KeyedVec<DrawEnt, TexIdOwner_t> const &cmpTexIds,
                                       IdMap_t<TexId, ResIdOwner_t> const    &texToRes,
                                       TexGdEntStorage_t                     &rCmpTexGl,
                                       RenderGd                              &rRenderGd)
{
    ACompTexGd         &rEntTexGl   = rCmpTexGl[ent];
    TexIdOwner_t const &entTexScnId = cmpTexIds[ent];

    // Make sure dirty entity has a MeshId component
    if ( entTexScnId.has_value() )
    {
        // Check if scene mesh ID is properly synchronized
        if ( rEntTexGl.m_scnId == entTexScnId )
        {
            return; // No changes needed
        }

        rEntTexGl.m_scnId = entTexScnId;

        // Check if MeshId is associated with a resource
        if ( auto const &foundIt = texToRes.find(entTexScnId); foundIt != texToRes.end() )
        {
            ResId const texResId = foundIt->second;

            // Mesh should have been loaded beforehand, assign it!
            rEntTexGl.m_gdId     = rRenderGd.m_resToTex.at(texResId);
        }
        else
        {
            OSP_LOG_WARN("No mesh data found for Mesh {} from Entity {}",
                         std::size_t(entTexScnId.value()),
                         std::size_t(ent));
        }
    }
    else
    {
        if ( rEntTexGl.m_gdId != lgrn::id_null<TexGdId>() )
        {
            // ACompMesh removed, remove ACompMeshGL too
            rEntTexGl = {};
        }
        else
        {
            // Why is this entity here?
        }
    }
}

void SysRenderGd::clear_resource_owners(RenderGd &rRenderGd, Resources &rResources)
{
    for ( [[maybe_unused]] auto &&[_, rOwner] : std::exchange(rRenderGd.m_texToRes, {}) )
    {
        rResources.owner_destroy(restypes::gc_texture, std::move(rOwner));
    }
    rRenderGd.m_resToTex.clear();

    for ( [[maybe_unused]] auto &&[_, rOwner] : std::exchange(rRenderGd.m_meshToRes, {}) )
    {
        rResources.owner_destroy(restypes::gc_mesh, std::move(rOwner));
    }
    rRenderGd.m_resToMesh.clear();
}
// FIXME
void SysRenderGd::render_opaque(
    RenderGroup const &group, DrawEntSet_t const &visible, ViewProjMatrix const &viewProj)
{
    draw_group(group, visible, viewProj);
}
// FIXME
void SysRenderGd::render_transparent(
    RenderGroup const &group, DrawEntSet_t const &visible, ViewProjMatrix const &viewProj)
{
    draw_group(group, visible, viewProj);
}

void SysRenderGd::draw_group(
    RenderGroup const &group, DrawEntSet_t const &visible, ViewProjMatrix const &viewProj)
{
    for ( auto const &[ent, toDraw] : entt::basic_view{ group.entities }.each() )
    {
        if ( visible.contains(ent) )
        {
            toDraw.draw(ent, viewProj, toDraw.data);
        }
    }
}
