#pragma once

#include <Magnum/Magnum.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/Math/Quaternion.h>


#include <string>
#include <array>
#include <vector>

namespace osp
{

enum class ObjectType
{
    NONE,       // Normal old object
    MESH,       // It has a mesh
    COLLIDER//,   // It's a collider
    //ATTACHMENT  //
};

enum class ColliderType
{
    CUBE,
    CYLINDER,
    MESH

    // add more here
};

//const uint32_t gc_OBJ_MESH      = 1 << 2;
//const uint32_t gc_OBJ_COLLIDER  = 1 << 3;


struct DrawableData
{

    // index to a resource path (aka: name) string in m_strings
    unsigned m_mesh;
    //std::string m_mesh;
    //unsigned m_material;
};

struct ColliderData
{
    ColliderType m_type;
    unsigned m_meshData;
};

struct ObjectPrototype
{
    unsigned m_parentIndex;
    unsigned m_childCount;
    //unsigned m_mesh;

    uint32_t m_bitmask;

    std::string m_name;

    std::vector<std::string> m_strings;

    //Magnum::Matrix4 m_transform;
    Magnum::Vector3 m_translation;
    Magnum::Quaternion m_rotation;
    Magnum::Vector3 m_scale;

    ObjectType m_type;

    union
    {
        DrawableData m_drawable;
        ColliderData m_collider;

    };

    // Put more OSP-specific data in here
};


class PartPrototype
{


public:
    PartPrototype();

    std::vector<ObjectPrototype>& get_objects() { return m_objects; }
    std::vector<ObjectPrototype> const& get_objects() const { return m_objects; }

private:
    //std::string name; use path

    std::vector<ObjectPrototype> m_objects;

    // std::vector <machines>

};

}
