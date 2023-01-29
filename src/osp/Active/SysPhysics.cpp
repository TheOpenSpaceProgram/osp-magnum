#include "SysPhysics.h"
#include "SysSceneGraph.h"

using namespace osp;
using namespace osp::active;

void SysPhysics::update_subtree_mass_inertia(ACtxPhysics& rCtxPhys, ACtxSceneGraph& rScnGraph, ActiveEnt ent, ACompMass& rHierMass)
{
#if 0
    rCtxPhys.m_hierMassDirty.reset(std::size_t(ent));

    ACompMass hierMassTotal;

    for (ActiveEnt const childEnt : SysSceneGraph::children(rScnGraph, ent))
    {


        if (rCtxPhys.m_hierMass.contains(childEnt))
        {
            if (rCtxPhys.m_hierMassDirty.test(std::size_t(childEnt)))
            {
                //update_subtree_mass_inertia
            }

            phys::transform_inertia_tensor()
        }


        if (rCtxPhys.m_ownMass)
    }
#endif
}
