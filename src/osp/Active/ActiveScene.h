#pragma once

#include <vector>

#include <Magnum/Math/Color.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/Shaders/Phong.h>

#include <Newton.h>

#include <entt/entt.hpp>

#include "../../types.h"

namespace osp
{


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
    entt::entity m_parent{entt::null};
    entt::entity m_siblingNext{entt::null};
    entt::entity m_siblingPrev{entt::null};

    // as a parent
    unsigned m_childCount{0};
    entt::entity m_childFirst{entt::null};

    // transform relative to parent

};

struct CompDrawableDebug
{
    Magnum::GL::Mesh* m_mesh;
    Magnum::Shaders::Phong* m_shader;
    Magnum::Color4 m_color;
};


//using CompHierarchyGroup = entt::basic_group<entt::entity, entt::exclude_t<>,
//                                             entt::get_t<>, CompHierarchy>;

//template <typename PhysicsEngine>
class ActiveScene
{

public:
    ActiveScene();
    ~ActiveScene();

    entt::entity hier_get_root() { return m_root; }
    entt::entity hier_create_child(entt::entity parent,
                                   std::string const& name = "Innocent Entity");
    void hier_set_parent_child(entt::entity parent, entt::entity child);

    entt::registry& get_registry() { return m_registry; }

    void update_physics();

    /**
     * Update the m_transformWorld of entities with CompTransform and
     * CompHierarchy. Intended for physics interpolation
     */
    void update_hierarchy_transforms();

    void draw_meshes(entt::entity camera);

    void on_hierarchy_construct(entt::registry& reg, entt::entity ent);
    void on_hierarchy_destruct(entt::registry& reg, entt::entity ent);

private:

    //std::vector<std::vector<entt::entity> > m_hierLevels;
    entt::registry m_registry;
    bool m_hierarchyDirty;

    entt::entity m_root;

    Vector3 m_floatingOriginTranslate;

    //CompHierarchyGroup m_group;

    // Newton dynamics stuff
    // note: can be null for physics-less view-only areas
    NewtonWorld* const m_nwtWorld;
};

}
