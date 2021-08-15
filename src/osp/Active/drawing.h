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
#include "../types.h"

#include <Corrade/Containers/ArrayView.h>
#include <entt/core/family.hpp>

#include <functional>
#include <unordered_map>

namespace osp::active
{
    struct EntityToDraw;
}

template<>
struct entt::storage_traits<osp::active::ActiveEnt, osp::active::EntityToDraw> {
    using storage_type = basic_storage<osp::active::ActiveEnt, osp::active::EntityToDraw>;
};

namespace osp::active
{

/**
 * @brief A function pointer to a Shader's draw() function
 *
 * @param ActiveEnt   [in] The entity being drawn; used to fetch component data
 * @param ActiveScene [ref] The scene containing the entity's component data
 * @param ACompCamera [in] Camera used to draw the scene
 * @param void*       [in] User data
 */
using ShaderDrawFnc_t = void (*)(
        ActiveEnt, ActiveScene&, ACompCamera const&, void*) noexcept;

using drawable_family_t = entt::family<struct drawable_type>;
using drawable_id_t = drawable_family_t::family_type;

struct DrawableCommon { };

/**
 * @brief Describes a draw function and user data needed to draw a single entity
 */
struct EntityToDraw
{
    ShaderDrawFnc_t m_draw;
    // Non-owning user data passed to draw function, such as the shader
    void *m_data;
};


struct RenderGroup
{
    using Storage_t = entt::storage_traits<ActiveEnt, EntityToDraw>::storage_type;
    using ArrayView_t = Corrade::Containers::ArrayView<ActiveEnt>;

    using DrawAssigner_t = std::function<
            void(ActiveScene&, Storage_t&, ArrayView_t) >;

    template<typename DRAWABLE_T>
    void set_assigner(DrawAssigner_t assigner)
    {
        drawable_id_t id = drawable_family_t::type<DRAWABLE_T>;
        m_assigners.resize(std::max(m_assigners.size(), size_t(id + 1)));
        m_assigners[id] = std::move(assigner);
    }

    decltype(auto) view()
    {
        return entt::basic_view{m_entities};
    }


    // index with drawable_id_t
    std::vector<DrawAssigner_t> m_assigners;
    Storage_t m_entities;

}; // struct RenderGroup

struct ACtxRenderGroups
{
    ACtxRenderGroups(ACtxRenderGroups const&) = delete;

    template<typename ... DRAWABLE_T>
    void resize_to_fit()
    {
        size_t minSize{std::max({drawable_family_t::type<DRAWABLE_T> ...}) + 1};

        m_added.resize(minSize);
    }

    template<typename DRAWABLE_T>
    void add(ActiveEnt ent)
    {
        m_added[drawable_family_t::type<DRAWABLE_T>].push_back(ent);
    }

    std::unordered_map< std::string, RenderGroup > m_groups;

    std::vector< std::vector<ActiveEnt> > m_added;

}; // struct ACtxRenderGroups

struct ACompDrawable
{
    drawable_id_t m_type;
};

template<typename DRAWABLE_T>
ACompDrawable make_drawable()
{
    return ACompDrawable{ drawable_family_t::type<DRAWABLE_T> };
}

/**
 * Represents an entity which requests the scene to be drawn to m_target
 */
struct ACompRenderingAgent
{
    ActiveEnt m_target;  // Must have ACompRenderTarget
};

/**
 * Describes the type of view used by a rendering agent
 */
struct ACompPerspective3DView
{
    ActiveEnt m_camera;
};

/* An object that is completely opaque */
struct ACompOpaque {};

/* An object with transparency */
struct ACompTransparent {};

/* Visibility state of this object */
struct ACompVisible {};

/**
 * World transform used for rendering. All ascendents of an entity using this
 * must also have this component
 */
struct ACompDrawTransform
{
    Matrix4 m_transformWorld;
};

}
