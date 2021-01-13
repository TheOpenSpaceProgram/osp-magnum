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

#include <string>
#include <array>
#include <vector>
#include <variant>

#include "../types.h"

namespace osp
{

enum class ObjectType
{
    NONE,       // Normal old object
    MESH,       // It has a mesh
    COLLIDER//,   // It's a collider
    //ATTACHMENT  //
};

enum class ECollisionShape : uint8_t
{
    NONE,
    COMBINED,
    SPHERE,
    BOX,
    CAPSULE,
    CYLINDER,
    //MESH,
    CONVEX_HULL,
    TERRAIN
};

//const uint32_t gc_OBJ_MESH      = 1 << 2;
//const uint32_t gc_OBJ_COLLIDER  = 1 << 3;


struct DrawableData
{

    // index to a PrototypePart.m_meshDataUsed
    unsigned m_mesh;
    std::vector<unsigned> m_textures;
    //std::string m_mesh;
    //unsigned m_material;
};

struct ColliderData
{
    ECollisionShape m_type;
    unsigned m_meshData;
};

struct PrototypeObject
{
    unsigned m_parentIndex;
    unsigned m_childCount;
    //unsigned m_mesh;

    uint32_t m_bitmask;

    std::string m_name;



    //Magnum::Matrix4 m_transform;
    // maybe not use magnum types here
    Magnum::Vector3 m_translation;
    Magnum::Quaternion m_rotation;
    Magnum::Vector3 m_scale;

    ObjectType m_type;

    std::variant<DrawableData, ColliderData> m_objectData;

    // Put more OSP-specific data in here
};


struct PrototypeMachine
{
    std::string m_type;

    // TODO: some sort of data
};

/**
 * Describes everything on how to construct a part, loaded directly from a file
 * or something
 */
class PrototypePart
{

public:
    PrototypePart();

    constexpr std::vector<PrototypeObject>& get_objects()
    { return m_objects; }
    constexpr std::vector<PrototypeObject> const& get_objects() const
    { return m_objects; }


    constexpr std::vector<PrototypeMachine>& get_machines()
    { return m_machines; }
    constexpr std::vector<PrototypeMachine> const& get_machines() const
    { return m_machines; }

    constexpr float& get_mass() noexcept
    { return m_mass; }
    constexpr float get_mass() const noexcept
    { return m_mass; }

    constexpr std::vector<std::string>& get_strings()
    { return m_strings; }

private:
    //std::string name; use path

    std::vector<PrototypeObject> m_objects;

    std::vector<PrototypeMachine> m_machines;

    //std::vector<DependRes<MeshData3D> > m_meshDataUsed;

    // TODO: more OSP information

    float m_mass;

    // std::vector <machines>

    std::vector<std::string> m_strings;

};

}
