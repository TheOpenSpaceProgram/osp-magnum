#include "PlanetGeometryA.h"

namespace osp
{

void PlanetGeometryA::initialize(float radius)
{
    m_icoTree = std::make_shared<IcoSphereTree>();
    m_icoTree->initialize(radius);
}


}
