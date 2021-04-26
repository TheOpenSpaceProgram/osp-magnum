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
#include "SysNewton.h"

#include "ActiveScene.h"

#include "adera/ShipResources.h"

#include <Newton.h>

using osp::active::SysNewton;

using osp::active::ActiveScene;
using osp::active::ActiveEnt;

using osp::active::ACompNwtBody;
using osp::active::ACompNwtWorld;
using osp::active::ACompTransform;

using namespace osp;

// Callback called for every Rigid Body (even static ones) on NewtonUpdate
void cb_force_torque(const NewtonBody* pBody, dFloat timestep, int threadIndex)
{
    // Get Scene from Newton World
    ActiveScene* scene = static_cast<ActiveScene*>(
                NewtonWorldGetUserData(NewtonBodyGetWorld(pBody)));

    // Get ACompNwtBody. Every NewtonBody created here is associated with an
    // entity that contains one.
    auto *pBodyComp = static_cast<ACompNwtBody*>(NewtonBodyGetUserData(pBody));
    auto &rTransformComp = scene->reg_get<ACompTransform>(pBodyComp->m_entity);

    // Check if transform has been set externally
    if (rTransformComp.m_transformDirty)
    {

        // Set matrix
        NewtonBodySetMatrix(pBody, rTransformComp.m_transform.data());

        rTransformComp.m_transformDirty = false;
    }

    // TODO: deal with changing inertia, mass or stuff

    // Apply force and torque
    NewtonBodySetForce(pBody, pBodyComp->m_netForce.data());
    NewtonBodySetTorque(pBody, pBodyComp->m_netTorque.data());

    // Reset accumolated net force and torque for next frame
    pBodyComp->m_netForce = {0.0f, 0.0f, 0.0f};
    pBodyComp->m_netTorque = {0.0f, 0.0f, 0.0f};
}

ACompNwtBody::ACompNwtBody(ACompNwtBody&& move) noexcept
 : DataRigidBody(std::move(move))
 , m_body(std::exchange(move.m_body, nullptr))
 , m_entity(std::exchange(move.m_entity, entt::null))
{
    if (m_body != nullptr)
    {
        NewtonBodySetUserData(m_body, this);
    }
}

ACompNwtBody& ACompNwtBody::operator=(ACompNwtBody&& move) noexcept
{
    m_body = std::exchange(move.m_body, nullptr);
    m_entity = std::exchange(move.m_entity, entt::null);

    if (m_body != nullptr)
    {
        NewtonBodySetUserData(m_body, this);
    }

    return *this;
}

void SysNewton::add_functions(ActiveScene &rScene)
{
    SPDLOG_LOGGER_INFO(rScene.get_application().get_logger(),
                      "Initing Sysnewton");    rScene.debug_update_add(rScene.get_update_order(), "physics", "wire", "",
                            &SysNewton::update_world);    //NewtonWorldSetUserData(m_nwtWorld, this);

    // Connect signal handlers to destruct Newton objects when components are
    // deleted.

    rScene.get_registry().on_destroy<ACompNwtBody>()
                    .connect<&SysNewton::on_body_destruct>();

    rScene.get_registry().on_destroy<ACompCollider>()
                    .connect<&SysNewton::on_shape_destruct>();

    rScene.get_registry().on_destroy<ACompNwtWorld>()
                    .connect<&SysNewton::on_world_destruct>();
}

void SysNewton::update_world(ActiveScene& rScene)
{
    // Try getting the newton world from the scene root.
    // It is very unlikely that multiple physics worlds are ever needed
    auto *worldComp = rScene.get_registry().try_get<ACompNwtWorld>(
                                 rScene.hier_get_root());

    if (worldComp == nullptr)
    {
        return; // No physics world component
    }

    if (worldComp->m_nwtWorld == nullptr)
    {
        // Create world if not yet initialized
        worldComp->m_nwtWorld = NewtonCreate();
        NewtonWorldSetUserData(worldComp->m_nwtWorld, &rScene);
    }

    NewtonWorld const* nwtWorld = worldComp->m_nwtWorld;

    auto viewBody = rScene.get_registry().view<ACompNwtBody>();
    auto viewBodyTransform = rScene.get_registry().view<ACompNwtBody,
                                                         ACompTransform>();

    // Iterate just the entities with ACompNwtBody
    for (ActiveEnt ent : viewBody)
    {
        auto &entBody = viewBodyTransform.get<ACompNwtBody>(ent);

        // temporary: delete if something is dirty
        if (entBody.m_colliderDirty)
        {
            NewtonDestroyBody(std::exchange(entBody.m_body, nullptr));
            entBody.m_colliderDirty = false;
        }

        // Initialize body if not done so yet;
        if (entBody.m_body == nullptr)
        {
            create_body(rScene, ent, nwtWorld);
        }

        // Recompute inertia and center of mass if rigidbody has been modified
        if (entBody.m_inertiaDirty)
        {
            compute_rigidbody_inertia(rScene, ent);
            SPDLOG_LOGGER_TRACE(m_scene.get_application().get_logger(), "Updating RB : new CoM Z = {}",
                              entBody.m_centerOfMassOffset.z());
        }

    }

    // Update the world
    NewtonUpdate(nwtWorld, rScene.get_time_delta_fixed());

    // Apply transform changes after the Newton world(s) updates
    for (ActiveEnt ent : viewBodyTransform)
    {
        auto &entBody      = viewBodyTransform.get<ACompNwtBody>(ent);
        auto &entTransform = viewBodyTransform.get<ACompTransform>(ent);

        if (entBody.m_body)
        {
            // Get new transform matrix from newton
            NewtonBodyGetMatrix(entBody.m_body,
                                entTransform.m_transform.data());
        }
    }
}

void SysNewton::find_colliders_recurse(ActiveScene& rScene, ActiveEnt ent,
                                       Matrix4 const &transform,
                                       NewtonWorld const* nwtWorld,
                                       NewtonCollision *rCompound)
{
    ActiveEnt nextChild = ent;

    // iterate through siblings of ent
    while(nextChild != entt::null)
    {
        auto const &childHeir = rScene.reg_get<ACompHierarchy>(nextChild);
        auto const *childTransform = rScene.get_registry().try_get<ACompTransform>(nextChild);

        if (childTransform != nullptr)
        {
            auto* childCollide = rScene.reg_try_get<ACompCollider>(nextChild);

            Matrix4 childMatrix = transform * childTransform->m_transform;

            if (childCollide != nullptr)
            {
                NewtonCollision *collision = childCollide->m_collision;

                if (collision == nullptr)
                {
                    // Newton collider has not yet been created from component

                    // TODO: care about collision shape. for now, everything is a
                    //       sphere
                    collision = NewtonCreateSphere(nwtWorld, 0.5f, 0, NULL);
                    childCollide->m_collision = collision;
                }

                //Matrix4 nextTransformNorm = nextTransform.

                // Set transform relative to root body
                Matrix4 f = Matrix4::translation(childMatrix.translation());
                NewtonCollisionSetMatrix(collision, f.data());

                // Add body to compound collision
                NewtonCompoundCollisionAddSubCollision(rCompound, collision);
            }
            else
            {
                //return;
            }

            find_colliders_recurse(rScene, childHeir.m_childFirst, childMatrix,
                                   nwtWorld, rCompound);
        }

        nextChild = childHeir.m_siblingNext;
    }
}

void SysNewton::create_body(ActiveScene& rScene, ActiveEnt entity,
                            NewtonWorld const* nwtWorld)
{
    auto const& entHier = rScene.reg_get<ACompHierarchy>(entity);
    auto& entTransform = rScene.reg_get<ACompTransform>(entity);

    auto& entBody  = rScene.reg_get<ACompNwtBody>(entity);
    auto* entCollider = rScene.reg_try_get<ACompCollider>(entity);
    auto* entShape = rScene.reg_try_get<ACompShape>(entity);

    if ((entCollider == nullptr) || (entShape == nullptr))
    {
        // Entity must have a collision shape to define the collision volume
        return;
    }

    switch (entShape->m_shape)
    {
    case phys::ECollisionShape::COMBINED:
    {
        // Combine collision shapes from descendants

        NewtonCollision* rCompound
                = NewtonCreateCompoundCollision(nwtWorld, 0);

        NewtonCompoundCollisionBeginAddRemove(rCompound);
        find_colliders_recurse(rScene, entHier.m_childFirst, Matrix4{},
                               nwtWorld, rCompound);
        NewtonCompoundCollisionEndAddRemove(rCompound);

        // TODO: deal with mass and stuff

        if (entBody.m_body != nullptr)
        {
            NewtonBodySetCollision(entBody.m_body, rCompound);
        }
        else
        {
            // Pass pointer to default (identity) matrix data for Newton to copy
            entBody.m_body = NewtonCreateDynamicBody(nwtWorld, rCompound,
                Matrix4{}.data());
        }

        NewtonDestroyCollision(rCompound);

        // Update center of mass, moments of inertia
        compute_rigidbody_inertia(rScene, entity);

        break;
    }
    case phys::ECollisionShape::TERRAIN:
    {
        // Get NewtonTreeCollision generated from elsewhere
        // such as SysPlanetA::debug_create_chunk_collider

        if (entCollider->m_collision)
        {
            if (entBody.m_body)
            {
                NewtonBodySetCollision(entBody.m_body, entCollider->m_collision);
            }
            else
            {
                entBody.m_body = NewtonCreateDynamicBody(nwtWorld,
                    entCollider->m_collision, Matrix4().data());
            }
        }
        else
        {
            // make a collision shape somehow
        }
    }
    default:
        break;
    }

    entTransform.m_controlled = true;
    entBody.m_entity = entity;

    // Set position/rotation
    NewtonBodySetMatrix(entBody.m_body, entTransform.m_transform.data());

    // Set damping to 0, as default is 0.1
    // reference frame may be moving and air pressure stuff
    NewtonBodySetLinearDamping(entBody.m_body, 0.0f);

    // Make it easier to rotate
    NewtonBodySetAngularDamping(entBody.m_body,
                                Vector3(1.0f, 1.0f, 1.0f).data());

    // Set callback for updating position of entity and everything else
    NewtonBodySetForceAndTorqueCallback(entBody.m_body, cb_force_torque);

    // Set user data
    //NwtUserData *data = new NwtUserData(entity, m_scene);
    NewtonBodySetUserData(entBody.m_body, &entBody);

    // don't leak memory
    //NewtonDestroyCollision(ball);
}

void SysNewton::compute_rigidbody_inertia(ActiveScene& rScene, ActiveEnt entity)
{
    auto& entHier = rScene.reg_get<ACompHierarchy>(entity);
    auto& entBody = rScene.reg_get<ACompNwtBody>(entity);

    // Compute moments of inertia, mass, and center of mass
    auto const& [inertia, centerOfMass] = compute_hier_inertia(rScene, entity);
    entBody.m_centerOfMassOffset = centerOfMass.xyz();

    // Set inertia and mass
    entBody.m_mass = centerOfMass.w();
    entBody.m_inertia.x() = inertia[0][0];  // Ixx
    entBody.m_inertia.y() = inertia[1][1];  // Iyy
    entBody.m_inertia.z() = inertia[2][2];  // Izz

    NewtonBodySetMassMatrix(entBody.m_body, entBody.m_mass,
        entBody.m_inertia.x(),
        entBody.m_inertia.y(),
        entBody.m_inertia.z());
    NewtonBodySetCentreOfMass(entBody.m_body, centerOfMass.xyz().data());

    entBody.m_inertiaDirty = false;
    SPDLOG_LOGGER_TRACE(m_scene.get_application().get_logger(), "New mass: {}",
                        entBody.m_mass);
}

ACompNwtWorld* SysNewton::try_get_physics_world(ActiveScene &rScene)
{
    return rScene.get_registry().try_get<ACompNwtWorld>(rScene.hier_get_root());
}

std::pair<ActiveEnt, ACompNwtBody*> SysNewton::find_rigidbody_ancestor(
        ActiveScene& rScene, ActiveEnt ent)
{
    ActiveEnt prevEnt, currEnt = ent;
    ACompHierarchy *pCurrHier = nullptr;

    do
    {
       pCurrHier = rScene.get_registry().try_get<ACompHierarchy>(currEnt);

        if (!pCurrHier)
        {
            return {entt::null, nullptr};
        }

        prevEnt = currEnt;
        currEnt = pCurrHier->m_parent;
    }
    while (pCurrHier->m_level != gc_heir_physics_level);

    auto *body = rScene.get_registry().try_get<ACompRigidBody_t>(prevEnt);

    return {prevEnt, body};
}

Matrix4 SysNewton::find_transform_rel_rigidbody_ancestor(ActiveScene& rScene, ActiveEnt ent)
{
    ActiveEnt prevEnt, currEnt = ent;
    ACompHierarchy *pCurrHier = nullptr;
    Matrix4 transform;


    do
    {
        pCurrHier = &rScene.get_registry().get<ACompHierarchy>(currEnt);

        // Record the local transformation of the current node relative to its parent
        if (pCurrHier->m_level > gc_heir_physics_level)
        {
            auto const *localTransform = rScene.get_registry().try_get<ACompTransform>(currEnt);
            if (localTransform != nullptr)
            {
                transform = localTransform->m_transform * transform;
            }
        }

        prevEnt = currEnt;
        currEnt = pCurrHier->m_parent;
    } while (pCurrHier->m_level != gc_heir_physics_level);

    // Fail if a rigidbody ancestor is not found
    assert(rScene.get_registry().has<ACompNwtBody>(prevEnt));

    return transform;
}

osp::active::ACompRigidbodyAncestor* SysNewton::try_get_or_find_rigidbody_ancestor(
    ActiveScene& rScene, ActiveEnt childEntity)
{
    auto& reg = rScene.get_registry();
    auto* pCompAncestor = reg.try_get<ACompRigidbodyAncestor>(childEntity);

    // Perform first-time initialization of rigidbody ancestor component
    if (pCompAncestor == nullptr)
    {
        auto [ent, compBody] = find_rigidbody_ancestor(rScene, childEntity);
        if (compBody == nullptr)
        {
            // childEntity has no rigidbody ancestor
            return nullptr;
        }

        reg.emplace<ACompRigidbodyAncestor>(childEntity);
    }
    // childEntity now has an ACompRigidbodyAncestor

    auto& rRbAncestor = reg.get<ACompRigidbodyAncestor>(childEntity);
    ActiveEnt& rAncestor = rRbAncestor.m_ancestor;

    // Ancestor entity already valid
    if (reg.valid(rAncestor))
    {
        return &rRbAncestor;
    }

    // Rigidbody ancestor not set yet
    auto [bodyEnt, compBody] = find_rigidbody_ancestor(rScene, childEntity);

    if (compBody == nullptr)
    {
        SPDLOG_LOGGER_WARN(rScene.get_application().get_logger(),
                          "No rigid body!");
        return nullptr;
    }

    // Initialize ACompRigidbodyAncestor
    rAncestor = bodyEnt;
    rRbAncestor.m_relTransform =
        find_transform_rel_rigidbody_ancestor(rScene, childEntity);
    //TODO: this transformation may change and need recalculating

    return &rRbAncestor;
}

void SysNewton::body_apply_force(ACompRigidBody_t &body, Vector3 force) noexcept
{
    body.m_netForce += force;
}

void SysNewton::body_apply_accel(ACompRigidBody_t &body, Vector3 accel) noexcept
{
    body_apply_force(body, accel * body.m_mass);
}

void SysNewton::body_apply_torque(ACompRigidBody_t &body, Vector3 torque) noexcept
{
    body.m_netTorque += torque;
}

/* Since masses are usually stored in the rigidbody's children instead of the
 * rigidbody root, the function provides the option to ignore the root entity's
 * mass (this is the default). Otherwise, the root of each call will be double
 * counted since it will also be included in both the loop and the root of the
 * next call. This makes it possible for a rigidbody to have a convenient
 * root-level mass component set by some external system without interfering
 * with physics calculations that depend on mass components to determine the
 * mechanical behaviors of the rigidbody.
 */
template <SysNewton::EIncludeRootMass INCLUDE_ROOT_MASS>
Vector4 SysNewton::compute_hier_CoM(ActiveScene& rScene, ActiveEnt root)
{
    Vector3 localCoM{0.0f};
    float localMass = 0.0f;

    /* Include the root entity's mass. Skipped by default to avoid
     * double-counting in recursion, and because some subhierarchies
     * may have a total mass stored at their root for easy external access
     */
    if constexpr (INCLUDE_ROOT_MASS)
    {
        auto const* rootMass = rScene.reg_try_get<ACompMass>(root);
        if (rootMass != nullptr) { localMass += rootMass->m_mass; }
    }

    for (ActiveEnt nextChild = rScene.reg_get<ACompHierarchy>(root).m_childFirst;
        nextChild != entt::null;)
    {
        auto const &childHier = rScene.reg_get<ACompHierarchy>(nextChild);
        auto const *childTransform = rScene.get_registry().try_get<ACompTransform>(nextChild);
        auto const *massComp = rScene.get_registry().try_get<ACompMass>(nextChild);
        Matrix4 childMatrix{Magnum::Math::IdentityInit};

        if (childTransform != nullptr)
        {
            childMatrix = childTransform->m_transform;
        }
        // Else, assume identity transformation relative to parent

        if (massComp != nullptr)
        {
            float childMass = rScene.reg_get<ACompMass>(nextChild).m_mass;

            Vector3 offset = childMatrix.translation();

            localCoM += childMass * offset;
            localMass += childMass;
        }

        // Recursively call this function to include grandchild masses
        Vector4 subCoM = compute_hier_CoM(rScene, nextChild);
        Vector3 childCoMOffset = childMatrix.translation() + subCoM.xyz();
        localCoM += subCoM.w() * childCoMOffset;
        localMass += subCoM.w();

        nextChild = childHier.m_siblingNext;
    }

    if (!(localMass > 0.0f))
    {
        // Massless subhierarchy, return zero contribution
        return Vector4{0.0f};
    }

    // Weighted sum of positions is divided by total mass to arrive at final CoM
    return {localCoM / localMass, localMass};
}

std::pair<Matrix3, Vector4> SysNewton::compute_hier_inertia(ActiveScene& rScene,
    ActiveEnt entity)
{
    Matrix3 I{0.0f};
    Vector4 centerOfMass =
        compute_hier_CoM<EIncludeRootMass::Include>(rScene, entity);

    auto& reg = rScene.get_registry();

    // Sum inertias of children
    for (ActiveEnt nextChild = reg.get<ACompHierarchy>(entity).m_childFirst;
        nextChild != entt::null;)
    {
        // Use child transformation to transform child inertia and add to sum
        auto const *childTransform = reg.try_get<ACompTransform>(nextChild);
        Matrix4 const childTransformMat = (childTransform != nullptr)
            ? childTransform->m_transform : Matrix4{Magnum::Math::IdentityInit};

        /* Special case:
         * Ordinarily, a child without a transformation means that branch of the
         * hierarchy doesn't have any physics-related data and can be skipped.
         * However, to minimize the number of transformations needed for
         * entities like machines which are associated with an immediate parent
         * and would have an identity transformation, the ACompTransform may be
         * omitted. A child that has no transformation but still has a mass will
         * be assumed to have an identity transformation relative to its parent
         * and processed as usual.
         */
        if ((childTransform != nullptr) || reg.has<ACompMass>(nextChild))
        {
            // Compute the inertia of the child subhierarchy
            auto const& [childInertia, childCoM] = compute_hier_inertia(rScene, nextChild);
            Matrix3 const rotation = childTransformMat.rotation();
            // Offset is the vector between the ship center of mass and the child CoM
            Vector3 const offset = (childTransformMat.translation() + childCoM.xyz()) - centerOfMass.xyz();
            I += phys::transform_inertia_tensor(
                childInertia, childCoM.w(), offset, rotation);
        }
        nextChild = reg.get<ACompHierarchy>(nextChild).m_siblingNext;
    }

    // Include entity's own inertia, if it has a mass and volume from which to compute it
    auto const* mass = reg.try_get<ACompMass>(entity);
    auto const* shape = reg.try_get<ACompShape>(entity);
    if ((mass != nullptr) && (shape != nullptr))
    {
        auto const* compTransform = reg.try_get<ACompTransform>(entity);

        Vector3 principalAxes;
        if (compTransform != nullptr)
        {
            // Transform used for scale; identity translation between root and itself
            const Matrix4& transform = rScene.reg_get<ACompTransform>(entity).m_transform;
            principalAxes = phys::collider_inertia_tensor(
                shape->m_shape, transform.scaling(), mass->m_mass);
        }
        else
        {
            /* If entity has both a mass and shape but no transform, it can be
             * assumed to be a leaf which has an identity transform relative to
             * its immediate parent. Thus, the parent's transformation is used
             * to calculate the leaf's scale.
             */
            ActiveEnt parent = reg.get<ACompHierarchy>(entity).m_parent;
            Matrix4 const& parentTransform = reg.get<ACompTransform>(parent).m_transform;
            principalAxes = phys::collider_inertia_tensor(
                shape->m_shape, parentTransform.scaling(), mass->m_mass);
        }

        /* We assume that all primitive shapes have diagonal inertia tensors in
         * their default orientation. The moments of inertia about these
         * principal axes thus form the eigenvalues of the inertia tensor.
         */
        Matrix3 localInertiaTensor{};
        localInertiaTensor[0][0] = principalAxes.x();
        localInertiaTensor[1][1] = principalAxes.y();
        localInertiaTensor[2][2] = principalAxes.z();

        I += localInertiaTensor;
    }

    return {I, centerOfMass};
}

void SysNewton::on_body_destruct(ActiveReg_t& reg, ActiveEnt ent)
{
    NewtonBody const *body = reg.get<ACompNwtBody>(ent).m_body;
    if (body != nullptr)
    {
        NewtonDestroyBody(body); // make sure the Newton body is destroyed
    }
}

void SysNewton::on_shape_destruct(ActiveReg_t& reg, ActiveEnt ent)
{
    NewtonCollision const *shape = reg.get<ACompCollider>(ent).m_collision;
    if (shape != nullptr)
    {
        NewtonDestroyCollision(shape); // make sure the shape is destroyed
    }
}

void SysNewton::on_world_destruct(ActiveReg_t& reg, ActiveEnt ent)
{


    NewtonWorld const *world = reg.get<ACompNwtWorld>(ent).m_nwtWorld;
    if (world != nullptr)
    {
        // delete all collision shapes first to prevent crash
        auto collisionView = reg.view<ACompCollider>();
        for (ActiveEnt ent : collisionView)
        {
            auto &collider = collisionView.get<ACompCollider>(ent);
            if (collider.m_collision != nullptr)
            {
                NewtonDestroyCollision(std::exchange(collider.m_collision, nullptr));
            }
        }

        // delete all rigid bodies too
        auto bodyView = reg.view<ACompNwtBody>();
        for (ActiveEnt ent : bodyView)
        {
            auto &body = bodyView.get<ACompNwtBody>(ent);
            if (body.m_body != nullptr)
            {
                NewtonDestroyBody(std::exchange(body.m_body, nullptr));
            }
        }

        NewtonDestroy(world); // make sure the world is destroyed
    }
}

NewtonCollision* SysNewton::newton_create_tree_collision(
        const NewtonWorld *newtonWorld, int shapeId)
{
    return NewtonCreateTreeCollision(newtonWorld, shapeId);
}

void SysNewton::newton_tree_collision_add_face(
        const NewtonCollision* treeCollision, int vertexCount,
        const float* vertexPtr, int strideInBytes, int faceAttribute)
{
    NewtonTreeCollisionAddFace(treeCollision, vertexCount, vertexPtr,
                               strideInBytes, faceAttribute);
}

void SysNewton::newton_tree_collision_begin_build(
        const NewtonCollision* treeCollision)
{
    NewtonTreeCollisionBeginBuild(treeCollision);
}

void SysNewton::newton_tree_collision_end_build(
        const NewtonCollision* treeCollision, int optimize)
{
    NewtonTreeCollisionEndBuild(treeCollision, optimize);
}

