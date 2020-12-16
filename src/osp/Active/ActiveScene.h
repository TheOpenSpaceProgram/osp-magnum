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

#include "../OSPApplication.h"
#include "../UserInputHandler.h"

#include "../types.h"
#include "activetypes.h"
//#include "physics.h"

//#include "SysDebugRender.h"
//#include "SysNewton.h"
#include "SysMachine.h"
//#include "SysVehicle.h"
//#include "SysWire.h"

namespace osp::active
{

/**
 * An ECS 3D Game Engine scene that implements a scene graph hierarchy.
 *
 * Components are prefixed with AComp, for "Active Component."
 *
 * Features are added through "Dynamic Systems" (IDynamicSystem), which are
 * classes that add functions to the UpdateOrder.
 *
 *
 * In ECS, "System" refers to the functions; but here, systems refer to the
 * classes containing those functions too.
 */
class ActiveScene
{

public:
    ActiveScene(UserInputHandler &userInput, OSPApplication& app);
    ~ActiveScene();

    OSPApplication& get_application() { return m_app; };

    /**
     * @return Root entity of the entire scene graph
     */
    constexpr ActiveEnt hier_get_root() { return m_root; }

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
     * @return Internal entt::registry
     */
    constexpr entt::registry& get_registry() { return m_registry; }

    /**
     * Shorthand for get_registry().get<T>()
     * @tparam T Component to get
     * @return Reference to component
     */
    template<class T>
    decltype(auto) reg_get(ActiveEnt ent) { return m_registry.get<T>(ent); };

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

    constexpr UpdateOrder& get_update_order() { return m_updateOrder; }

    constexpr RenderOrder& get_render_order() { return m_renderOrder; }

    // TODO
    constexpr float get_time_delta_fixed() { return 1.0f / 60.0f; }

    /**
     *
     * @tparam T
     */
    template<class T, typename... Args>
    void system_machine_add(std::string const& name, Args &&... args);

    /**
     * Find a registered SysMachine by name. This accesses a map.
     * @param name [in] Name used as a key
     * @return Iterator to specified SysMachine
     */
    MapSysMachine::iterator system_machine_find(std::string const& name);

    bool system_machine_it_valid(MapSysMachine::iterator it);

    /**
     *
     * @tparam T
     */
    template<class T, typename... Args>
    T& dynamic_system_add(std::string const& name, Args &&... args);

    /**
     * Find a registered IDynamicSystem by name. This accesses a map.
     * @param name [in] Name used as a key
     * @return Iterator to specified IDynamicSys
     */
    MapDynamicSys::iterator dynamic_system_find(std::string const& name);

    /**
     * Get a registered IDynamicSystem by name, and cast it to SYSTEM_T. This
     * accesses a map. If a system isn't found, this will assert.
     * @param name [in] Name used as a key
     * @return Reference to specified IDynamicSys
     */
    template<class SYSTEM_T>
    SYSTEM_T& dynamic_system_get(std::string const& name);

    bool dynamic_system_it_valid(MapDynamicSys::iterator it);

private:

    void on_hierarchy_construct(entt::registry& reg, ActiveEnt ent);
    void on_hierarchy_destruct(entt::registry& reg, ActiveEnt ent);

    OSPApplication& m_app;

    //std::vector<std::vector<ActiveEnt> > m_hierLevels;
    entt::basic_registry<ActiveEnt> m_registry;
    ActiveEnt m_root;
    bool m_hierarchyDirty;

    float m_timescale;

    UserInputHandler &m_userInput;
    //std::vector<std::reference_wrapper<ISysMachine>> m_update_sensor;
    //std::vector<std::reference_wrapper<ISysMachine>> m_update_physics;

    UpdateOrder m_updateOrder;
    RenderOrder m_renderOrder;

    MapSysMachine m_sysMachines; // TODO: Put this in SysVehicle
    MapDynamicSys m_dynamicSys;

    // TODO: base class and a list for Systems (or not)
    //SysDebugRender m_render;
    //SysPhysics m_physics;
    //SysWire m_wire;
    //SysVehicle m_vehicles;

    //SysDebugObject m_debugObj;
    //std::tuple<SysPhysics, SysWire, SysDebugObject> m_systems;

};

// move these to another file eventually

template<class T, typename... Args>
void ActiveScene::system_machine_add(std::string const& name,
                                             Args &&... args)
{
    m_sysMachines.emplace(name, std::make_unique<T>(*this, args...));
}

template<class T, typename... Args>
T& ActiveScene::dynamic_system_add(std::string const& name,
                                             Args &&... args)
{
    auto ptr = std::make_unique<T>(*this, args...);
    T &refReturn = *(ptr.get());

    auto pair = m_dynamicSys.emplace(name, std::move(ptr));

    return refReturn;
}

template<class SYSTEM_T>
SYSTEM_T& ActiveScene::dynamic_system_get(std::string const& name)
{
    MapDynamicSys::iterator it = dynamic_system_find(name);

    assert(dynamic_system_it_valid(it));

    return static_cast<SYSTEM_T&>(*(it->second));
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

    void calculate_projection();
};


}
