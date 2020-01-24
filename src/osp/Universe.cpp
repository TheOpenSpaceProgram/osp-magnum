#include "Universe.h"
#include "Satellites/SatActiveArea.h"

namespace osp
{

Universe::Universe()
{
    // put something here eventually
}

Satellite& Universe::create_sat()
{
    m_satellites.emplace_back();
    return m_satellites.back();
}

}
