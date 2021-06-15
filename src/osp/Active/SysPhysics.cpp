#include "SysPhysics.h"
#include "ActiveScene.h"

using osp::active::SysPhysics;
using osp::active::ActiveEnt;
using osp::active::ACompRigidBody;

using osp::Matrix4;
using osp::Matrix3;
using osp::Vector4;

std::pair<ActiveEnt, ACompRigidBody*> SysPhysics::find_rigidbody_ancestor(
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

    auto *pBody = rScene.get_registry().try_get<ACompRigidBody>(prevEnt);

    return {prevEnt, pBody};
}

Matrix4 SysPhysics::find_transform_rel_rigidbody_ancestor(ActiveScene& rScene, ActiveEnt ent)
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
    assert(rScene.get_registry().all_of<ACompRigidBody>(prevEnt));

    return transform;
}


osp::active::ACompRigidbodyAncestor* SysPhysics::try_get_or_find_rigidbody_ancestor(
    ActiveScene& rScene, ActiveEnt childEntity)
{
    ActiveReg_t &rReg = rScene.get_registry();
    auto *pCompAncestor = rReg.try_get<ACompRigidbodyAncestor>(childEntity);

    // Perform first-time initialization of rigidbody ancestor component
    if (pCompAncestor == nullptr)
    {
        auto [ent, pCompBody] = find_rigidbody_ancestor(rScene, childEntity);
        if (pCompBody == nullptr)
        {
            // childEntity has no rigidbody ancestor
            return nullptr;
        }

        rReg.emplace<ACompRigidbodyAncestor>(childEntity);
    }
    // childEntity now has an ACompRigidbodyAncestor

    auto& rRbAncestor = rReg.get<ACompRigidbodyAncestor>(childEntity);
    ActiveEnt& rAncestor = rRbAncestor.m_ancestor;

    // Ancestor entity already valid
    if (rReg.valid(rAncestor))
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

/* Since masses are usually stored in the rigidbody's children instead of the
 * rigidbody root, the function provides the option to ignore the root entity's
 * mass (this is the default). Otherwise, the root of each call will be double
 * counted since it will also be included in both the loop and the root of the
 * next call. This makes it possible for a rigidbody to have a convenient
 * root-level mass component set by some external system without interfering
 * with physics calculations that depend on mass components to determine the
 * mechanical behaviors of the rigidbody.
 */
template <SysPhysics::EIncludeRootMass INCLUDE_ROOT_MASS>
Vector4 SysPhysics::compute_hier_CoM(ActiveScene& rScene, ActiveEnt root)
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

std::pair<Matrix3, Vector4> SysPhysics::compute_hier_inertia(ActiveScene& rScene,
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
        if ((childTransform != nullptr) || reg.all_of<ACompMass>(nextChild))
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

void SysPhysics::body_apply_force(ACompRigidBody &body, Vector3 force) noexcept
{
    body.m_netForce += force;
}

void SysPhysics::body_apply_accel(ACompRigidBody &body, Vector3 accel) noexcept
{
    body_apply_force(body, accel * body.m_mass);
}

void SysPhysics::body_apply_torque(ACompRigidBody &body, Vector3 torque) noexcept
{
    body.m_netTorque += torque;
}
