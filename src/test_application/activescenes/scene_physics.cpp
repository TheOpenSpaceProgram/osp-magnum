/**
 * Open Space Program
 * Copyright © 2019-2022 Open Space Program Project
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
#include "scene_physics.h"

#include <osp/Active/basic.h>
#include <osp/Active/drawing.h>
#include <osp/Active/physics.h>
#include <osp/Active/SysHierarchy.h>


using osp::active::ActiveEnt;


namespace testapp::scenes
{

void PhysicsData::cleanup(CommonTestScene& rScene)
{
    auto &rScnPhys = rScene.get<PhysicsData>();

    for (auto && [_, rOwner] : std::exchange(rScnPhys.m_shapeToMesh, {}))
    {
        rScene.m_drawing.m_meshRefCounts.ref_release(std::move(rOwner));
    }

    for (auto && [_, rOwner] : std::exchange(rScnPhys.m_namedMeshs, {}))
    {
        rScene.m_drawing.m_meshRefCounts.ref_release(std::move(rOwner));
    }
}

ActiveEnt add_solid_quick(
        CommonTestScene& rScene, ActiveEnt parent, osp::phys::EShape shape,
        osp::Matrix4 transform, int material, float mass)
{
    using namespace osp::active;

    auto &rScnPhys = rScene.get<PhysicsData>();

    // Make entity
    ActiveEnt ent = rScene.m_activeIds.create();

    // Add mesh
    rScene.m_drawing.m_mesh.emplace(
            ent, rScene.m_drawing.m_meshRefCounts.ref_add(rScnPhys.m_shapeToMesh.at(shape)) );
    rScene.m_drawing.m_meshDirty.push_back(ent);

    // Add material to cube
    MaterialData &rMaterial = rScene.m_drawing.m_materials[material];
    rMaterial.m_comp.emplace(ent);
    rMaterial.m_added.push_back(ent);

    // Add transform
    rScene.m_basic.m_transform.emplace(ent, ACompTransform{transform});

    // Add opaque and visible component
    rScene.m_drawing.m_opaque.emplace(ent);
    rScene.m_drawing.m_visible.emplace(ent);

    // Add physics stuff
    rScnPhys.m_physics.m_shape.emplace(ent, shape);
    rScnPhys.m_physics.m_solid.emplace(ent);
    osp::Vector3 const inertia
            = osp::phys::collider_inertia_tensor(shape, transform.scaling(), mass);
    rScnPhys.m_hierBody.m_ownDyn.emplace( ent, ACompSubBody{ inertia, mass } );

    rScnPhys.m_physIn.m_colliderDirty.push_back(ent);

    // Add to hierarchy
    SysHierarchy::add_child(rScene.m_basic.m_hierarchy, parent, ent);

    return ent;
}

ActiveEnt add_rigid_body_quick(
        CommonTestScene &rScene, osp::Vector3 position, osp::Vector3 velocity,
        float mass, osp::phys::EShape shape, osp::Vector3 size)
{
    using namespace osp::active;

    auto &rScnPhys = rScene.get<PhysicsData>();

    // Root is needed to act as the rigid body entity
    // Scale of root entity must be (1, 1, 1). Descendents that act as colliders
    // are allowed to have different scales
    ActiveEnt root = rScene.m_activeIds.create();

    // Add transform
    rScene.m_basic.m_transform.emplace(
            root, ACompTransform{osp::Matrix4::translation(position)});

    // Add root entity to hierarchy
    SysHierarchy::add_child(rScene.m_basic.m_hierarchy, rScene.m_hierRoot, root);

    // Create collider / drawable to the root entity
    add_solid_quick(
            rScene, root, shape,
            osp::Matrix4::scaling(size),
            rScene.m_matVisualizer, 0.0f);

    // Make ball root a dynamic rigid body
    rScnPhys.m_physics.m_hasColliders.emplace(root);
    rScnPhys.m_physics.m_physBody.emplace(root);
    rScnPhys.m_physics.m_physLinearVel.emplace(root);
    rScnPhys.m_physics.m_physAngularVel.emplace(root);
    osp::active::ACompPhysDynamic &rDyn = rScnPhys.m_physics.m_physDynamic.emplace(root);
    rDyn.m_totalMass = 1.0f;

    // Set velocity
    rScnPhys.m_physIn.m_setVelocity.emplace_back(root, velocity);

    return root;
}

} // namespace testapp::scenes


