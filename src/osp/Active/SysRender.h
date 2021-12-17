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

namespace osp::active
{

/**
 * @brief Stores a draw function and user data needed to draw a single entity
 */
struct EntityToDraw
{
    using UserData_t = std::array<void*, 4>;

    /**
     * @brief A function pointer to a Shader's draw() function
     *
     * @param ActiveEnt    [in] The entity being drawn
     * @param ACompCamera  [in] Camera used to draw the scene
     * @param UserData_t   [in] Non-owning user data
     */
    using ShaderDrawFnc_t = void (*)(
            ActiveEnt, ACompCamera const&, UserData_t) noexcept;

    constexpr void operator()(
            ActiveEnt ent,
            ACompCamera const& camera) const noexcept
    {
        m_draw(ent, camera, m_data);
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
    using ArrayView_t = Corrade::Containers::ArrayView<ActiveEnt>;

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
     * @brief Update draw ACompDrawTransform according to the hierarchy of
     *        ACompTransforms
     *
     * TODO: this is a rather expensive operation that recalculates ALL draw
     *       transforms, regardless of if they changed or not. Consider adding
     *       dirty flags or similar.
     *
     * @param hier      [in] Storage for hierarchy components
     * @param viewTf    [in] View for local-space transform components
     * @param rDrawTf   [out] Storage for world-space draw transform components
     */
    static void update_draw_transforms(
            acomp_storage_t<ACompHierarchy> const& hier,
            acomp_view_t<ACompTransform> const& viewTf,
            acomp_storage_t<ACompDrawTransform>& rDrawTf);

}; // class SysRender

} // namespace osp::active
