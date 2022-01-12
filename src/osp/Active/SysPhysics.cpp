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
        acomp_storage_t<ACompHierarchy> const& hierarchy,
        ActiveEnt ent)
{
    ActiveEnt prevEnt;
    ActiveEnt currEnt = ent;
    ACompHierarchy const *pCurrHier = nullptr;

    do
    {
        if( ! hierarchy.contains(currEnt) )
        {
            return entt::null;
        }

        pCurrHier = &hierarchy.get(currEnt);

        prevEnt = std::exchange(currEnt, pCurrHier->m_parent);
    }
    while (pCurrHier->m_level != gc_heir_physics_level);

    return prevEnt;
}

Matrix4 SysPhysics::calc_transform_rel_rigidbody_ancestor(
        acomp_storage_t<ACompHierarchy> const& hierarchy,
        acomp_storage_t<ACompTransform> const& transforms,
        ActiveEnt ent)
{
    ActiveEnt prevEnt;
    ActiveEnt currEnt = ent;
    ACompHierarchy const *pCurrHier = nullptr;
    Matrix4 transformOut;

    do
    {
        pCurrHier = &hierarchy.get(currEnt);

        // Record the local transformation of the current node relative to its parent
        if (pCurrHier->m_level > gc_heir_physics_level)
        {
            if (transforms.contains(currEnt))
            {
                transformOut = transforms.get(currEnt).m_transform * transformOut;
            }
        }

        prevEnt = std::exchange(currEnt, pCurrHier->m_parent);
    }
    while (pCurrHier->m_level != gc_heir_physics_level);

    return transformOut;
}
