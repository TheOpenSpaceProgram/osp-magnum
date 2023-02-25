#include "SysPhysics.h"
#include "SysSceneGraph.h"

using namespace osp;
using namespace osp::active;

void SysPhysics::calculate_subtree_mass_center(
        acomp_storage_t<ACompTransform> const&  rTf,
        ACtxPhysics&                            rCtxPhys,
        ACtxSceneGraph&                         rScnGraph,
        ActiveEnt                               root,
        Vector3&                                rMassPos,
        float&                                  rTotalMass,
        Matrix4 const&                          currentTf)
{
    for (ActiveEnt const child : SysSceneGraph::children(rScnGraph, root))
    {
        Matrix4 const childTf = currentTf * rTf.get(child).m_transform;

        if (rCtxPhys.m_mass.contains(child))
        {
            ACompMass const& childMass = rCtxPhys.m_mass.get(child);

            Matrix3 inertiaTensor{};
            inertiaTensor[0][0] = childMass.m_inertia.x();
            inertiaTensor[1][1] = childMass.m_inertia.y();
            inertiaTensor[2][2] = childMass.m_inertia.z();

            rTotalMass  += childMass.m_mass;
            rMassPos    += childTf.translation() * childMass.m_mass;
        }

        if (rCtxPhys.m_hasColliders.test(std::size_t(child)))
        {
            calculate_subtree_mass_center(rTf, rCtxPhys, rScnGraph, child, rMassPos, rTotalMass, childTf);
        }
    }
}


void SysPhysics::calculate_subtree_mass_inertia(
        acomp_storage_t<ACompTransform> const&  rTf,
        ACtxPhysics&                            rCtxPhys,
        ACtxSceneGraph&                         rScnGraph,
        ActiveEnt                               root,
        Matrix3&                                rInertiaTensor,
        Matrix4 const&                          currentTf)
{
    for (ActiveEnt const child : SysSceneGraph::children(rScnGraph, root))
    {
        Matrix4 const childTf = currentTf * rTf.get(child).m_transform;

        if (rCtxPhys.m_mass.contains(child))
        {
            ACompMass const& childMass = rCtxPhys.m_mass.get(child);

            Matrix3 inertiaTensor{};
            inertiaTensor[0][0] = childMass.m_inertia.x();
            inertiaTensor[1][1] = childMass.m_inertia.y();
            inertiaTensor[2][2] = childMass.m_inertia.z();

            Vector3 const offset = childTf.translation() + childMass.m_offset * childTf.scaling();

            rInertiaTensor += phys::transform_inertia_tensor(inertiaTensor, childMass.m_mass, offset, childTf.rotation());
        }

        if (rCtxPhys.m_hasColliders.test(std::size_t(child)))
        {
            calculate_subtree_mass_inertia(rTf, rCtxPhys, rScnGraph, child, rInertiaTensor, childTf);
        }
    }
}
