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
            ActiveEnt, ViewProjMatrix const&, UserData_t) noexcept;

    constexpr void operator()(
            ActiveEnt ent,
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
    using Storage_t = entt::storage_traits<ActiveEnt, EntityToDraw>::storage_type;
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

    inline static void add_draw_transforms_recurse(
            acomp_storage_t<ACompHierarchy> const& hier,
            acomp_storage_t<ACompDrawTransform>& rDrawTf,
            ActiveEnt ent);

    template<typename ITA_T, typename ITB_T>
    static void assure_draw_transforms(
            acomp_storage_t<ACompHierarchy> const& hier,
            acomp_storage_t<ACompDrawTransform>& rDrawTf,
            ITA_T first, ITB_T last);

    /**
     * @brief Update draw ACompDrawTransform according to the hierarchy of
     *        ACompTransforms
     *
     * TODO: this is a rather expensive operation that recalculates ALL draw
     *       transforms, regardless of if they changed or not. Consider adding
     *       dirty flags or similar.
     *
     * @param hier      [in] Storage for hierarchy components
     * @param transform [in] Storage for local-space transform components
     * @param rDrawTf   [out] Storage for world-space draw transform components
     */
    static void update_draw_transforms(
            acomp_storage_t<ACompHierarchy> const& hier,
            acomp_storage_t<ACompTransform> const& transform,
            acomp_storage_t<ACompDrawTransform>& rDrawTf);

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
            ACtxDrawing& rCtxDraw, IT_T first, IT_T last);

    template<typename IT_T>
    static void update_delete_groups(
            ACtxRenderGroups& rCtxGroups, IT_T first, IT_T last);

private:


}; // class SysRender

void SysRender::add_draw_transforms_recurse(
        acomp_storage_t<ACompHierarchy> const& hier,
        acomp_storage_t<ACompDrawTransform>& rDrawTf, ActiveEnt ent)
{
    if (rDrawTf.contains(ent))
    {
        return;
    }

    rDrawTf.emplace(ent); // Emplace the component

    ACompHierarchy const &currHier = hier.get(ent);

    if (currHier.m_level <= 1)
    {
        return; // Stop recursing when about to reach root
    }

    // Tail recurse into parent
    add_draw_transforms_recurse(hier, rDrawTf, currHier.m_parent);
}

template<typename ITA_T, typename ITB_T>
void SysRender::assure_draw_transforms(
        acomp_storage_t<ACompHierarchy> const& hier,
        acomp_storage_t<ACompDrawTransform>& rDrawTf,
        ITA_T first, ITB_T last)
{
    while (first != last)
    {
        add_draw_transforms_recurse(hier, rDrawTf, *first);
        std::advance(first, 1);
    }
}

template<typename STORAGE_T, typename REFCOUNT_T>
void remove_refcounted(
        ActiveEnt ent, STORAGE_T &rStorage, REFCOUNT_T &rRefcount)
{
    if (rStorage.contains(ent))
    {
        auto &rOwner = rStorage.get(ent);
        if (rOwner.has_value())
        {
            rRefcount.ref_release(rOwner);
        }
        rStorage.erase(ent);
    }
}

template<typename IT_T>
void SysRender::update_delete_drawing(
        ACtxDrawing& rCtxDraw, IT_T first, IT_T last)
{
    while (first != last)
    {
        ActiveEnt const ent = *first;
        rCtxDraw.m_opaque       .remove(ent);
        rCtxDraw.m_transparent  .remove(ent);
        rCtxDraw.m_visible      .remove(ent);

        // Textures and meshes are reference counted
        remove_refcounted(ent, rCtxDraw.m_diffuseTex, rCtxDraw.m_texRefCounts);
        remove_refcounted(ent, rCtxDraw.m_mesh, rCtxDraw.m_meshRefCounts);

        for (MaterialData& rMat : rCtxDraw.m_materials)
        {
            rMat.m_comp.remove(ent);
        }

        std::advance(first, 1);
    }
}

template<typename IT_T>
void SysRender::update_delete_groups(
        ACtxRenderGroups& rCtxGroups, IT_T first, IT_T last)
{
    if (first == last)
    {
        return;
    }
    for (auto& [_, rGroup] : rCtxGroups.m_groups)
    {
        rGroup.m_entities.remove(first, last);
    }
}

} // namespace osp::active
