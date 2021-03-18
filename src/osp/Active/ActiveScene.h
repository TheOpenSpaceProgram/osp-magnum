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
#include "../UserInputHandler.h"

#include "../types.h"
#include "activetypes.h"
//#include "physics.h"

//#include "SysDebugRender.h"
//#include "SysNewton.h"
#include "SysMachine.h"
//#include "SysVehicle.h"
#include "SysWire.h"
#include "adera/SysExhaustPlume.h"

#include <Magnum/GL/Framebuffer.h>

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

    ActiveScene(UserInputHandler &userInput, OSPApplication& app, Package& context);
    ~ActiveScene();

    OSPApplication& get_application() { return m_app; };

    /**
     * @return Root entity of the entire scene graph
     */
    constexpr ActiveEnt hier_get_root() const { return m_root; }

    /**
     * Create a new entity, and add a ACompHierarchy to it
     * @param parent [in] Entity to assign as parent
     * @param name   [in] Name of entity
     * @return New entity created
     */
    ActiveEnt hier_create_child(ActiveEnt parent,
                                std::string const& name = "Innocent Entity");

    /**
     * Set parent-child relationship between two nodes containing an
     * ACompHierarchy
     *
     * @param parent [in]
     * @param child  [in]
     */
    void hier_set_parent_child(ActiveEnt parent, ActiveEnt child);

    /**
     * Destroy an entity and all its descendents
     * @param ent [in]
     */
    void hier_destroy(ActiveEnt ent);

    /**
     * Cut an entity out of it's parent. This will leave the entity with no
     * parent.
     * @param ent [in]
     */
    void hier_cut(ActiveEnt ent);

    /**
     * Traverse the scene hierarchy
     * 
     * Calls the specified callable on each entity of the scene hierarchy
     * @param root The entity whose sub-tree to traverse
     * @param callable A function that accepts an ActiveEnt as an argument and
     *                 returns false if traversal should stop, true otherwise
     */
    template <typename FUNC_T>
    void hierarchy_traverse(ActiveEnt root, FUNC_T callable);

    /**
     * @return Internal entt::registry
     */
    constexpr ActiveReg_t& get_registry()
    { return m_registry; }

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
     * Update everything in the update order, including all systems and stuff
     */
    void update();

    /**
     * Update the m_transformWorld of entities with ACompTransform and
     * ACompHierarchy. Intended for physics interpolation
     */
    void update_hierarchy_transforms();

    /**
     * Calculate transformations relative to camera, and draw every
     * CompDrawableDebug
     * @param camera [in] Entity containing a ACompCamera
     */
    void draw(ActiveEnt camera);

    constexpr UserInputHandler& get_user_input() { return m_userInput; }

    constexpr UpdateOrder_t& get_update_order() { return m_updateOrder; }

    //constexpr RenderOrder_t& get_render_order() { return m_renderOrder; }
    using RenderPass_t = std::function<void(ActiveScene& rScene, ACompCamera& rCamera)>;
    constexpr std::vector<RenderPass_t>& get_render_queue() { return m_renderQueue; }

    // TODO
    constexpr float get_time_delta_fixed() const { return 1.0f / 60.0f; }

    /**
     * Add support for a new machine type by adding an ISysMachine
     * @param name [in] Name to identify this machine type. This is used by part
     *                  configs to idenfity which machines to use.
     * @param sysMachine [in] ISysMachine to add. As this will transfer
     *                        ownership, use std::move()
     * @return Iterator to new machine added, or invalid iterator to end of map
     *         if the name already exists.
     */
     MapSysMachine_t::iterator system_machine_add(std::string_view name,
                            std::unique_ptr<ISysMachine> sysMachine);

    /**
     *
     * @tparam T
     */
    template<class SYSMACH_T, typename... ARGS_T>
    void system_machine_create(ARGS_T &&... args);

    /**
     * Find a registered SysMachine by name. This accesses a map.
     * @param name [in] Name used as a key
     * @return Iterator to specified SysMachine
     */
    MapSysMachine_t::iterator system_machine_find(std::string_view name);

    bool system_machine_it_valid(MapSysMachine_t::iterator it);

    template<typename... ARGS_T>
    void debug_update_add(ARGS_T &&... args)
    { m_updateHandles.emplace_back(std::forward<ARGS_T>(args)...); }

    template<typename... ARGS_T>
    void debug_render_add(ARGS_T &&... args)
    { m_renderHandles.emplace_back(std::forward<ARGS_T>(args)...); }

    Package& get_context_resources() { return m_context; }

private:

    void on_hierarchy_construct(ActiveReg_t& reg, ActiveEnt ent);
    void on_hierarchy_destruct(ActiveReg_t& reg, ActiveEnt ent);

    OSPApplication& m_app;
    Package& m_context;

    //std::vector<std::vector<ActiveEnt> > m_hierLevels;
    ActiveReg_t m_registry;
    ActiveEnt m_root;
    bool m_hierarchyDirty;

    float m_timescale;

    UserInputHandler &m_userInput;

    UpdateOrder_t m_updateOrder;
    //RenderOrder_t m_renderOrder;
    std::vector<RenderPass_t> m_renderQueue;

    std::vector<UpdateOrderHandle_t> m_updateHandles;
    std::vector<RenderOrderHandle_t> m_renderHandles;

    MapSysMachine_t m_sysMachines; // TODO: Put this in SysVehicle

};

// move these to another file eventually


template<class SYSMACH_T, typename... ARGS_T>
void ActiveScene::system_machine_create(ARGS_T &&... args)
{
    system_machine_add(SYSMACH_T::smc_name,
                       std::make_unique<SYSMACH_T>(*this, args...));
}

/**
 * Component for transformation (in meters)
 */
struct ACompTransform
{
    //Matrix4 m_transformPrev;
    Matrix4 m_transform;
    Matrix4 m_transformWorld;
    //bool m_enableFloatingOrigin;

    // For when transform is controlled by a specific system.
    // Examples of this behaviour:
    // * Entities with ACompRigidBody are controlled by SysPhysics, transform
    //   is updated each frame
    bool m_controlled{false};

    // if this is true, then transform can be modified, as long as
    // m_transformDirty is set afterwards
    bool m_mutable{true};
    bool m_transformDirty{false};
};

struct ACompHierarchy
{
    std::string m_name; // maybe move this somewhere

    //unsigned m_childIndex;
    unsigned m_level{0}; // 0 for root entity, 1 for root's child, etc...
    ActiveEnt m_parent{entt::null};
    ActiveEnt m_siblingNext{entt::null};
    ActiveEnt m_siblingPrev{entt::null};

    // as a parent
    unsigned m_childCount{0};
    ActiveEnt m_childFirst{entt::null};

    // transform relative to parent

};

// Stores the mass of entities
struct ACompMass
{
    float m_mass;
};

/**
 * Component that represents a camera
 */
struct ACompCamera
{
    float m_near, m_far;
    Magnum::Deg m_fov;
    Vector2 m_viewport;

    Matrix4 m_projection;
    Matrix4 m_inverse;

    // If empty, GL::DefaultFramebuffer is used
    DependRes<Magnum::GL::Framebuffer> m_renderTarget;

    void calculate_projection();
};

enum class EHierarchyTraverseStatus : bool
{
    Continue = true,
    Stop = false
};

template<typename FUNC_T>
void ActiveScene::hierarchy_traverse(ActiveEnt root, FUNC_T callable)
{
    using osp::active::ACompHierarchy;

    std::stack<ActiveEnt> parentNextSibling;
    ActiveEnt currentEnt = root;

    unsigned int rootLevel = reg_get<ACompHierarchy>(root).m_level;

    while (true)
    {
        ACompHierarchy &hier = reg_get<ACompHierarchy>(currentEnt);

        if (callable(currentEnt) == EHierarchyTraverseStatus::Stop) { return; }

        if (hier.m_childCount > 0)
        {
            // entity has some children
            currentEnt = hier.m_childFirst;

            // Save next sibling for later if it exists
            // Don't check siblings of the root node
            if ((hier.m_siblingNext != entt::null) && (hier.m_level > rootLevel))
            {
                parentNextSibling.push(hier.m_siblingNext);
            }
        }
        else if ((hier.m_siblingNext != entt::null) && (hier.m_level > rootLevel))
        {
            // no children, move to next sibling
            currentEnt = hier.m_siblingNext;
        }
        else if (parentNextSibling.size() > 0)
        {
            // last sibling, and not done yet
            // is last sibling, move to parent's (or ancestor's) next sibling
            currentEnt = parentNextSibling.top();
            parentNextSibling.pop();
        } else
        {
            break;
        }
    }
}

}
