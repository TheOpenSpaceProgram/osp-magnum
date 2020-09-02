#include "Universe.h"

#include "Satellites/SatActiveArea.h"

#include <iostream>
#include <iterator>

using namespace osp::universe;
using namespace osp;

Universe::Universe()
{
    m_root = sat_create();
}

Satellite Universe::sat_create()
{
    Satellite sat = m_registry.create();
    m_registry.emplace<UCompTransformTraj>(sat);
    m_registry.emplace<UCompType>(sat);
    return sat;
}

void Universe::sat_remove(Satellite sat)
{
    m_registry.destroy(sat);
}


Vector3s Universe::sat_calc_pos(Satellite referenceFrame, Satellite target)
{
    auto view = m_registry.view<UCompTransformTraj>();

    // TODO: maybe do some checks to make sure they have the components
    auto &framePosTraj = view.get<UCompTransformTraj>(referenceFrame);
    auto &targetPosTraj = view.get<UCompTransformTraj>(target);

    if (framePosTraj.m_parent == targetPosTraj.m_parent)
    {
        // Same parent, easy calculation
        return targetPosTraj.m_position - framePosTraj.m_position;
    }
    // TODO: calculation for different parents

    return {0, 0, 0};
}

Vector3 Universe::sat_calc_pos_meters(Satellite referenceFrame, Satellite target)
{
    // 1024 units = 1 meter. this can change
    return Vector3(sat_calc_pos(referenceFrame, target)) / 1024.0f;
}


