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

#include "drawing.h"

#include <unordered_map>

namespace osp::active
{

using DrawTransforms_t = KeyedVec<DrawEnt, Matrix4>;

/**
 * @brief View and Projection matrix
 */
struct ViewProjMatrix
{
    ViewProjMatrix(Matrix4 const& view, Matrix4 const& proj)
     : m_viewProj{proj * view}
     , m_view{view}
     , m_proj{proj}
    { }

    Matrix4 m_viewProj;
    Matrix4 m_view;
    Matrix4 m_proj;
};

/**
 * @brief Stores a draw function and user data needed to draw a single entity
 */
struct EntityToDraw
{
    using UserData_t = std::array<void*, 4>;

    /**
     * @brief A function pointer to a Shader's draw() function
     *
     * @param ActiveEnt         [in] The entity being drawn
     * @param ViewProjMatrix    [in] View and projection matrix
     * @param UserData_t        [in] Non-owning user data
     */
    using ShaderDrawFnc_t = void (*)(
            DrawEnt, ViewProjMatrix const&, UserData_t) noexcept;

    constexpr void operator()(
            DrawEnt ent,
            ViewProjMatrix const& viewProj) const noexcept
    {
        m_draw(ent, viewProj, m_data);
    }

    ShaderDrawFnc_t m_draw;

    // Non-owning user data passed to draw function, such as the shader
    UserData_t m_data;

}; // struct EntityToDraw

/**
 * @brief Tracks a set of entities and their assigned drawing functions
 *
 * RenderGroups are intended to be associated with certain rendering techniques
 * like Forward, Deferred, and Shadow mapping.
 *
 * This also works with game-specific modes like Thermal Imaging.
 */
struct RenderGroup
{
    using Storage_t = entt::basic_storage<EntityToDraw, DrawEnt>;
    using ArrayView_t = Corrade::Containers::ArrayView<const ActiveEnt>;

    /**
     * @return Iterable view for stored entities
     */
    decltype(auto) view()
    {
        return entt::basic_view{m_entities};
    }

    /**
     * @return Iterable view for stored entities
     */
    decltype(auto) view() const
    {
        return entt::basic_view{m_entities};
    }

    Storage_t m_entities;

}; // struct RenderGroup

struct ACtxRenderGroups
{
    // Required for std::is_copy_assignable to work properly inside of entt::any
    ACtxRenderGroups() = default;
    ACtxRenderGroups(ACtxRenderGroups const& copy) = delete;
    ACtxRenderGroups(ACtxRenderGroups&& move) = default;

    std::unordered_map< std::string, RenderGroup > m_groups;

};

class SysRender
{
public:

    /**
     * @brief Attempt to create a scene mesh associated with a resource
     *
     * @param rCtxDrawing       [ref] Drawing data
     * @param rCtxDrawingRes    [ref] Resource drawing data
     * @param rResources        [ref] Application Resources containing meshes
     * @param resId             [in] Mesh Resource Id
     *
     * @return Id of new mesh, existing mesh Id if it already exists.
     */
    static MeshId own_mesh_resource(
            ACtxDrawing& rCtxDrawing,
            ACtxDrawingRes& rCtxDrawingRes,
            Resources& rResources,
            ResId resId);

    static TexId own_texture_resource(
            ACtxDrawing& rCtxDrawing,
            ACtxDrawingRes& rCtxDrawingRes,
            Resources& rResources,
            ResId resId);

    /**
     * @brief Remove all mesh and texture components, aware of refcounts
     *
     * @param rCtxDrawing       [ref] Drawing data
     */
    static void clear_owners(ACtxDrawing& rCtxDrawing);

    /**
     * @brief Dissociate resources from the scene's meshes and textures
     *
     * @param rCtxDrawingRes    [ref] Resource drawing data
     * @param rResources        [ref] Application Resources
     */
    static void clear_resource_owners(
            ACtxDrawingRes& rCtxDrawingRes,
            Resources& rResources);


    template<typename ITA_T, typename ITB_T>
    static void assure_draw_transforms(
            acomp_storage_t<Matrix4>& rDrawTf, ITA_T first, ITB_T const& last);

    template<typename IT_T, typename ITB_T>
    static void update_draw_transforms(
            ACtxSceneGraph const&                   rScnGraph,
            KeyedVec<ActiveEnt, DrawEnt> const&     activeToDraw,
            acomp_storage_t<ACompTransform> const&  transform,
            DrawTransforms_t&                       rDrawTf,
            EntSet_t const&                         useDrawTf,
            IT_T                                    first,
            ITB_T const&                            last);

    /**
     * @brief Set all dirty flags/vectors
     *
     * @param rCtxDrawing [ref] Drawing data
     */
    static void set_dirty_all(ACtxDrawing& rCtxDrawing);

    /**
     * @brief Clear all dirty flags/vectors
     *
     * @param rCtxDrawing [ref] Drawing data
     */
    static void clear_dirty_all(ACtxDrawing& rCtxDrawing);

    template<typename IT_T>
    static void update_delete_drawing(
            ACtxDrawing& rCtxDraw, IT_T first, IT_T const& last);

    template<typename IT_T>
    static void update_delete_groups(
            ACtxRenderGroups& rCtxGroups, IT_T first, IT_T const& last);

    static MeshIdOwner_t add_drawable_mesh(ACtxDrawing& rDrawing, ACtxDrawingRes& rDrawingRes, Resources& rResources, PkgId const pkg, std::string_view const name);

    static constexpr decltype(auto) gen_drawable_mesh_adder(ACtxDrawing& rDrawing, ACtxDrawingRes& rDrawingRes, Resources& rResources, PkgId const pkg);

private:
    static void update_draw_transforms_recurse(
            ACtxSceneGraph const&                   rScnGraph,
            KeyedVec<ActiveEnt, DrawEnt> const&     activeToDraw,
            acomp_storage_t<ACompTransform> const&  rTf,
            DrawTransforms_t&                       rDrawTf,
            EntSet_t const&                         useDrawTf,
            ActiveEnt                               ent,
            Matrix4 const&                          parentTf,
            bool                                    root);

}; // class SysRender

template<typename ITA_T, typename ITB_T>
void SysRender::assure_draw_transforms(
        acomp_storage_t<Matrix4>& rDrawTf, ITA_T first, ITB_T const& last)
{
    while (first != last)
    {
        if ( ! rDrawTf.contains(*first))
        {
            rDrawTf.emplace(*first);
        }
        std::advance(first, 1);
    }
}

template<typename IT_T, typename ITB_T>
void SysRender::update_draw_transforms(
        ACtxSceneGraph const&                   rScnGraph,
        KeyedVec<ActiveEnt, DrawEnt> const&     activeToDraw,
        acomp_storage_t<ACompTransform> const&  rTf,
        DrawTransforms_t&                       rDrawTf,
        EntSet_t const&                         needDrawTf,
        IT_T                                    first,
        ITB_T const&                            last)
{
    static constexpr Matrix4 const identity{};

    while (first != last)
    {
        update_draw_transforms_recurse(rScnGraph, activeToDraw, rTf, rDrawTf, needDrawTf, *first, identity, true);

        std::advance(first, 1);
    }
}


template<typename STORAGE_T, typename REFCOUNT_T>
void remove_refcounted(
        DrawEnt const ent, STORAGE_T &rStorage, REFCOUNT_T &rRefcount)
{
    auto &rOwner = rStorage[ent];
    if (rOwner.has_value())
    {
        rRefcount.ref_release(std::move(rOwner));
    }
}

template<typename IT_T>
void SysRender::update_delete_drawing(
        ACtxDrawing& rCtxDraw, IT_T first, IT_T const& last)
{
    while (first != last)
    {
        DrawEnt const ent = rCtxDraw.m_activeToDraw[*first];

        // Textures and meshes are reference counted
        remove_refcounted(ent, rCtxDraw.m_diffuseTex, rCtxDraw.m_texRefCounts);
        remove_refcounted(ent, rCtxDraw.m_mesh, rCtxDraw.m_meshRefCounts);


        std::advance(first, 1);
    }
}

template<typename IT_T>
void SysRender::update_delete_groups(
        ACtxRenderGroups& rCtxGroups, IT_T first, IT_T const& last)
{
    if (first == last)
    {
        return;
    }
    for ([[maybe_unused]] auto& [_, rGroup] : rCtxGroups.m_groups)
    {
        rGroup.m_entities.remove(first, last);
    }
}

constexpr decltype(auto) SysRender::gen_drawable_mesh_adder(ACtxDrawing& rDrawing, ACtxDrawingRes& rDrawingRes, Resources& rResources, PkgId const pkg)
{
    return [&rDrawing, &rDrawingRes, &rResources, pkg] (std::string_view const name) -> MeshIdOwner_t
    {
        return add_drawable_mesh(rDrawing, rDrawingRes, rResources, pkg, name);
    };
}



} // namespace osp::active
