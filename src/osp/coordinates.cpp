#include "coordinates.h"

#include <Corrade/Containers/ArrayViewStl.h>

using namespace osp::universe;

void CoordspaceSimple::update_views(CoordinateSpace& rSpace,
                                    CoordspaceSimple& rData)
{

    size_t const ccompCount = std::max({
                ccomp_id<CCompSat>(),
                ccomp_id<CCompX>(), ccomp_id<CCompY>(), ccomp_id<CCompZ>()});

    size_t const satCount = rData.m_satellites.size();

    rSpace.m_components.resize(ccompCount + 1);

    rSpace.m_components[ccomp_id<CCompSat>()] = {
                rData.m_satellites, satCount,
                sizeof(Satellite)};

    rSpace.m_components[ccomp_id<CCompX>()] = {
            rData.m_positions, &rData.m_positions[0].x(),
            satCount, sizeof (Vector3g)};

    rSpace.m_components[ccomp_id<CCompY>()] = {
            rData.m_positions, &rData.m_positions[0].y(),
            satCount, sizeof (Vector3g)};

    rSpace.m_components[ccomp_id<CCompZ>()] = {
            rData.m_positions, &rData.m_positions[0].z(),
            satCount, sizeof (Vector3g)};
}
