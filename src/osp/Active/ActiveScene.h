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

#include <utility>
#include <vector>
#include <stack>

#include "../OSPApplication.h"

#include "../types.h"
#include "activetypes.h"
#include "scene.h"

namespace osp::active
{

/**
 * An ECS 3D Game Engine scene that implements a scene graph hierarchy.
 *
 * The state of scene is represented with Active Entities (ActiveEnt) which are
 * compositions of Active Components (AComp-prefixed structs). Behaviours are
 * added by injecting System functions into the Update Order (a list of
 * functions called in a specific order, use debug_update_add).
 *
 */
class ActiveScene
{

public:

    ActiveScene(OSPApplication& app, Package& context);

    OSPApplication& get_application() { return m_app; };

    /**
     * @return Root entity of the entire scene graph
     */
    constexpr ActiveEnt hier_get_root() const { return m_root; }

    /**
     * @return Internal entt::registry
     */
    constexpr ActiveReg_t& get_registry()
    { return m_registry; }

    /**
     * @return Internal entt::registry
     */
    constexpr ActiveReg_t const& get_registry() const
    { return m_registry; }

    template<typename ACOMP_T, typename ... ARGS_T>
    ACOMP_T& reg_root_get_or_emplace(ARGS_T &&... args)
    {
        return m_registry.get_or_emplace<ACOMP_T>(
                    m_root, std::forward<ARGS_T>(args)...);
    }

    /**
     * Shorthand for get_registry().get<T>()
     * @tparam T Component to get
     * @return Reference to component
     */
    template<class T>
    decltype(auto) reg_get(ActiveEnt ent) { return m_registry.get<T>(ent); }

    template<class T>
    decltype(auto) reg_get(ActiveEnt ent) const { return m_registry.get<T>(ent); }


    /**
     * Shorthand for get_registry().try_get<T>()
     * @tparam T Component to get
     * @return Pointer to component
     */
    template<class T>
    constexpr T* reg_try_get(ActiveEnt ent) { return m_registry.try_get<T>(ent); };

    /**
     * Shorthand for get_registry().emplace<T>()
     * @tparam T Component to emplace
     */
    template<class T, typename... Args>
    decltype(auto) reg_emplace(ActiveEnt ent, Args&& ... args)
    {
        return m_registry.emplace<T>(ent, std::forward<Args>(args)...);
    }


    /**
     * Mark an entity for deletion
     *
     * @param ent [in] Entity to delete
     */
    void mark_delete(ActiveEnt ent)
    {
        m_registry.emplace_or_replace<ACompDelete>(ent);
    }

    /**
     * Update everything in the update order, including all systems and stuff
     */
    void update();

    /**
     * Delete entities with ACompDelete
     *
     * @param rScene [ref] scene with entities
     */
    static void update_delete(ActiveScene& rScene)
    {
        ActiveReg_t &rReg = rScene.get_registry();
        auto view = rReg.view<ACompDelete>();
        rReg.destroy(std::begin(view), std::end(view));
    }

    /**
     * Calculate world transformations and draw to all render targets
     */
    void draw();

    constexpr UpdateOrder_t& get_update_order() { return m_updateOrder; }

    constexpr RenderOrder_t& get_render_order() { return m_renderOrder; }

    // TODO
    constexpr float get_time_delta_fixed() const { return 1.0f / 60.0f; }

    template<typename... ARGS_T>
    void debug_update_add(ARGS_T &&... args)
    { m_updateHandles.emplace_back(std::forward<ARGS_T>(args)...); }

    template<typename... ARGS_T>
    void debug_render_add(ARGS_T &&... args)
    { m_renderHandles.emplace_back(std::forward<ARGS_T>(args)...); }

    Package& get_context_resources() { return m_context; }
private:

    OSPApplication& m_app;
    Package& m_context;

    UpdateOrder_t m_updateOrder;
    RenderOrder_t m_renderOrder;

    std::vector<UpdateOrderHandle_t> m_updateHandles;
    std::vector<RenderOrderHandle_t> m_renderHandles;

    ActiveReg_t m_registry;
    ActiveEnt m_root;

};

enum class EHierarchyTraverseStatus : bool
{
    Continue = true,
    Stop = false
};

}
