#include <iostream>
#include <iterator>

#include "Universe.h"
#include "Satellites/SatActiveArea.h"

using namespace osp::universe;

Universe::Universe()
{
    m_root = sat_create();
}

Satellite Universe::sat_create()
{
    Satellite sat = m_registry.create();
    m_registry.emplace<ucomp::PositionTrajectory>(sat);
    m_registry.emplace<ucomp::Type>(sat);
    return sat;
}

void Universe::sat_remove(Satellite sat)
{
    m_registry.destroy(sat);
}
