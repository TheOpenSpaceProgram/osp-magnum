#pragma once

#include <vector>

#include <Magnum/Math/Color.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/Shaders/Phong.h>

#include <Newton.h>

#include <entt/entt.hpp>

#include "../Resource/PartPrototype.h"

#include "../../types.h"

#include "SysNewton.h"
#include "SysMachine.h"
#include "SysWire.h"

#include "Machines/UserControl.h"


namespace osp
{

// in case Newton Dynamics gets swapped out, one can implement a system class
// with all the same methods
using SysPhysics = SysNewton;

using ActiveEnt = entt::entity;

struct CompCamera
{
    float m_near, m_far;
    Magnum::Deg m_fov;
    Vector2 m_viewport;

    Matrix4 m_projection;

    void calculate_projection();
};

struct CompTransform
{
    //Matrix4 m_transformPrev;
    Matrix4 m_transform;
    Matrix4 m_transformWorld;
    bool m_enableFloatingOrigin;
};

struct CompHierarchy
{
    std::string m_name;

    //unsigned m_childIndex;
    unsigned m_level{0};
    ActiveEnt m_parent{entt::null};
    ActiveEnt m_siblingNext{entt::null};
    ActiveEnt m_siblingPrev{entt::null};

    // as a parent
    unsigned m_childCount{0};
    ActiveEnt m_childFirst{entt::null};

    // transform relative to parent

};

struct CompCollider
{
    ColliderType m_type;
    // TODO: mesh data
};

struct CompDrawableDebug
{
    Magnum::GL::Mesh* m_mesh;
    Magnum::Shaders::Phong* m_shader;
    Magnum::Color4 m_color;
};


//using CompHierarchyGroup = entt::basic_group<ActiveEnt, entt::exclude_t<>,
//                                             entt::get_t<>, CompHierarchy>;

//template <typename PhysicsEngine>
class ActiveScene
{

public:
    ActiveScene(UserInputHandler &userInput);
    ~ActiveScene();

    ActiveEnt hier_get_root() { return m_root; }
    ActiveEnt hier_create_child(ActiveEnt parent,
                                   std::string const& name = "Innocent Entity");
    void hier_set_parent_child(ActiveEnt parent, ActiveEnt child);

    entt::registry& get_registry() { return m_registry; }

    template<class T>
    T& reg_get(ActiveEnt ent) { return m_registry.get<T>(ent); };

    template<class T, typename... Args>
    T& reg_emplace(ActiveEnt ent, Args &&... args)
    {
        return m_registry.emplace<T>(ent, args...);
    };

    /**
     *
     */
    void update_physics();

    /**
     * Update the m_transformWorld of entities with CompTransform and
     * CompHierarchy. Intended for physics interpolation
     */
    void update_hierarchy_transforms();

    /**
     * Request a floating origin mass translation. Multiple calls to this are
     * accumolated and are applied on the next physics update
     * @param amount
     */
    void floating_origin_translate(Vector3 const& amount);

    Vector3 const& floating_origin_get_total() { return m_floatingOriginTranslate; }
    bool floating_origin_in_progress() { return m_floatingOriginInProgress; }

    /**
     * @brief draw
     * @param camera
     */
    void draw(ActiveEnt camera);

    void on_hierarchy_construct(entt::registry& reg, ActiveEnt ent);
    void on_hierarchy_destruct(entt::registry& reg, ActiveEnt ent);

    // replace with something like get_systen<NewtonWorld>
    //SysNewton& debug_get_newton() { return m_newton; }

    template<class T>
    T& get_system();

private:

    //std::vector<std::vector<ActiveEnt> > m_hierLevels;
    entt::basic_registry<ActiveEnt> m_registry;
    ActiveEnt m_root;
    bool m_hierarchyDirty;

    Vector3 m_floatingOriginTranslate;
    bool m_floatingOriginInProgress;

    //UserInputHandler &m_userInput;

    // TODO: base class and a list for Systems
    SysPhysics m_physics;
    SysMachineUserControl m_machineUserControl;
    SysWire m_wire;

};

}
