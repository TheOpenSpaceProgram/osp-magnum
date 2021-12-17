#include "SysPhysics.h"
#include "basic.h"

#include <osp/Resource/PackageRegistry.h>

using osp::active::SysPhysics;
using osp::active::ActiveEnt;
using osp::active::ACompPhysBody;

using osp::Matrix4;
using osp::Matrix3;
using osp::Vector4;

ActiveEnt SysPhysics::find_rigidbody_ancestor(
        acomp_view_t<ACompHierarchy> viewHier,
        ActiveEnt ent)
{
    ActiveEnt prevEnt;
    ActiveEnt currEnt = ent;
    ACompHierarchy *pCurrHier = nullptr;

    do
    {
        if( ! viewHier.contains(currEnt) )
        {
            return entt::null;
        }

        pCurrHier = &viewHier.get<ACompHierarchy>(currEnt);

        prevEnt = std::exchange(currEnt, pCurrHier->m_parent);
    }
    while (pCurrHier->m_level != gc_heir_physics_level);

//    if ( ! rCtxPhys.m_physBody.contains(prevEnt))
//    {
//        return entt::null; // no Physics body!
//    }

    return prevEnt;
}

Matrix4 SysPhysics::find_transform_rel_rigidbody_ancestor(
        acomp_view_t<ACompHierarchy> viewHier,
        acomp_view_t<ACompTransform> viewTf,
        ActiveEnt ent)
{
    ActiveEnt prevEnt;
    ActiveEnt currEnt = ent;
    ACompHierarchy *pCurrHier = nullptr;
    Matrix4 transform;

    do
    {
        pCurrHier = &viewHier.get<ACompHierarchy>(currEnt);

        // Record the local transformation of the current node relative to its parent
        if (pCurrHier->m_level > gc_heir_physics_level)
        {
            if (viewTf.contains(currEnt))
            {
                transform = viewTf.get<ACompTransform>(currEnt).m_transform * transform;
            }
        }

        prevEnt = std::exchange(currEnt, pCurrHier->m_parent);
    } while (pCurrHier->m_level != gc_heir_physics_level);

    // Fail if a rigidbody ancestor is not found
    //assert(rCtxPhys.m_physBody.contains(prevEnt));

    return transform;
}


osp::active::ACompRigidbodyAncestor* SysPhysics::try_get_or_find_rigidbody_ancestor(
        acomp_view_t<ACompHierarchy> viewHier,
        acomp_view_t<ACompTransform> viewTf,
        acomp_storage_t<ACompRigidbodyAncestor>& rRbAncestor,
        ActiveEnt ent)
{
    ACompRigidbodyAncestor *pEntRbAncestor;

    // Perform first-time initialization of rigidbody ancestor component
    if ( ! rRbAncestor.contains(ent))
    {
        find_rigidbody_ancestor(viewHier, ent);

        pEntRbAncestor = &rRbAncestor.emplace(ent);
    }
    else
    {
        pEntRbAncestor = &rRbAncestor.get(ent);
    }
    // childEntity now has an ACompRigidbodyAncestor

    ActiveEnt& rAncestor = pEntRbAncestor->m_ancestor;

    // Ancestor entity already valid
    if (rAncestor != entt::null)
    {
        return pEntRbAncestor;
    }

    // Rigidbody ancestor not set yet
    ActiveEnt bodyEnt = find_rigidbody_ancestor(viewHier, ent);

    // Initialize ACompRigidbodyAncestor
    rAncestor = bodyEnt;
    pEntRbAncestor->m_relTransform = find_transform_rel_rigidbody_ancestor(
                viewHier, viewTf, ent);
    //TODO: this transformation may change and need recalculating

    return pEntRbAncestor;
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
Vector4 SysPhysics::compute_hier_CoM(
        acomp_view_t<ACompHierarchy> const viewHier,
        acomp_view_t<ACompTransform> const viewTf,
        acomp_view_t<ACompMass> const viewMass, ActiveEnt root)
{
    Vector3 localCoM{0.0f};
    float localMass = 0.0f;

    /* Include the root entity's mass. Skipped by default to avoid
     * double-counting in recursion, and because some subhierarchies
     * may have a total mass stored at their root for easy external access
     */
    if constexpr (INCLUDE_ROOT_MASS)
    {
        if (viewMass.contains(root))
        {
            localMass += viewMass.get<ACompMass>(root).m_mass;
        }
    }

    for (ActiveEnt nextChild = viewHier.get<ACompHierarchy>(root).m_childFirst;
        nextChild != entt::null;)
    {
        auto const &childHier = viewHier.get<ACompHierarchy>(nextChild);
        Matrix4 childMatrix{Magnum::Math::IdentityInit};

        if (viewTf.contains(nextChild))
        {
            childMatrix = viewTf.get<ACompTransform>(nextChild).m_transform;
        }
        // Else, assume identity transformation relative to parent

        if (viewMass.contains(nextChild))
        {
            float childMass = viewMass.get<ACompMass>(root).m_mass;

            Vector3 offset = childMatrix.translation();

            localCoM += childMass * offset;
            localMass += childMass;
        }

        // Recursively call this function to include grandchild masses
        Vector4 subCoM = compute_hier_CoM(viewHier, viewTf, viewMass, nextChild);
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

std::pair<Matrix3, Vector4> SysPhysics::compute_hier_inertia(
        acomp_view_t<ACompHierarchy> const viewHier,
        acomp_view_t<ACompTransform> const viewTf,
        acomp_view_t<ACompMass> const viewMass,
        acomp_view_t<ACompShape> const viewShape,
        ActiveEnt entity)
{
    Matrix3 I{0.0f};
    Vector4 centerOfMass = compute_hier_CoM<EIncludeRootMass::Include>(viewHier, viewTf, viewMass, entity);

    // Sum inertias of children
    for (ActiveEnt nextChild = viewHier.get<ACompHierarchy>(entity).m_childFirst;
        nextChild != entt::null;)
    {
        // Use child transformation to transform child inertia and add to sum
        bool childHasTf = viewTf.contains(nextChild);

        Matrix4 const childTransformMat = childHasTf
                ? viewTf.get<ACompTransform>(nextChild).m_transform
                : Matrix4{Magnum::Math::IdentityInit};

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
        if (childHasTf || viewMass.contains(nextChild))
        {
            // Compute the inertia of the child subhierarchy
            auto const& [childInertia, childCoM] = compute_hier_inertia(
                    viewHier, viewTf, viewMass, viewShape, nextChild);

            Matrix3 const rotation = childTransformMat.rotation();
            // Offset is the vector between the ship center of mass and the child CoM
            Vector3 const offset = (childTransformMat.translation() + childCoM.xyz()) - centerOfMass.xyz();

            I += phys::transform_inertia_tensor(
                childInertia, childCoM.w(), offset, rotation);
        }
        nextChild = viewHier.get<ACompHierarchy>(nextChild).m_siblingNext;
    }

    // Include entity's own inertia, if it has a mass and volume from which to compute it
    if (viewMass.contains(entity) && viewShape.contains(entity))
    {
        auto const &mass = viewMass.get<ACompMass>(entity);
        auto const &shape = viewShape.get<ACompShape>(entity);
        //auto const &compTransform = reg.try_get<ACompTransform>(entity);

        Vector3 principalAxes;
        if (viewTf.contains(entity))
        {
            // Transform used for scale; identity translation between root and itself
            const Matrix4& transform = viewTf.get<ACompTransform>(entity).m_transform;
            principalAxes = phys::collider_inertia_tensor(
                    shape.m_shape, transform.scaling(), mass.m_mass);
        }
        else
        {
            /* If entity has both a mass and shape but no transform, it can be
             * assumed to be a leaf which has an identity transform relative to
             * its immediate parent. Thus, the parent's transformation is used
             * to calculate the leaf's scale.
             */
            ActiveEnt parent = viewHier.get<ACompHierarchy>(entity).m_parent;
            Matrix4 const& parentTransform = viewTf.get<ACompTransform>(parent).m_transform;
            principalAxes = phys::collider_inertia_tensor(
                shape.m_shape, parentTransform.scaling(), mass.m_mass);
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
