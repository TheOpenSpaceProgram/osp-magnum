/**
 * Open Space Program
 * Copyright © 2019-2021 Open Space Program Project
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "../scene_universe.h"

#include "common.h"

#include <osp/CoordinateSpaces/CartesianSimple.h>
#include <osp/Satellites/SatActiveArea.h>

#include <osp/Resource/blueprints.h>

using namespace testapp;

using osp::universe::Satellite;
using osp::universe::Universe;
using osp::universe::coordspace_index_t;

using osp::universe::SatActiveArea;
using osp::universe::UCompActiveArea;

using osp::universe::CoordinateSpace;
using osp::universe::CoordspaceCartesianSimple;

using osp::universe::UCompVehicle;

std::unique_ptr<UniverseScene> testapp::setup_universe_scene()
{
    return std::make_unique<UniverseScene>();
}

std::function<void(UniverseScene&)> testapp::generate_simple_universe_update(
        osp::universe::coordspace_index_t cartesian)
{
    return [cartesian] (UniverseScene &rUniScn) {

        Universe &rUni = rUniScn.m_universe;

        bool hasArea = !rUniScn.m_activation.m_activeArea.empty();

        Satellite areaSat = hasArea ? rUniScn.m_activation.m_activeArea.at(0)
                                    : entt::null;
        auto *pArea = hasArea ? &rUniScn.m_activation.m_activeArea.get(areaSat)
                              : nullptr;

        {
            CoordinateSpace &rMainSpace = rUni.coordspace_get(cartesian);
            auto *pMainData = entt::any_cast<CoordspaceCartesianSimple>(&rMainSpace.m_data);

            rUni.coordspace_update_sats(cartesian);
            CoordspaceCartesianSimple::update_exchange(rUni.sat_coordspaces(), rMainSpace, *pMainData);
            rMainSpace.exchange_done();

            CoordspaceCartesianSimple::update_views(rMainSpace, *pMainData);
        }

        if (hasArea)
        {
            CoordinateSpace &rCaptureSpace = rUni.coordspace_get(pArea->m_captureSpace);
            auto *pCaptureData = entt::any_cast<CoordspaceCartesianSimple>(&rCaptureSpace.m_data);

            rUni.coordspace_update_sats(pArea->m_captureSpace);
            CoordspaceCartesianSimple::update_exchange(rUni.sat_coordspaces(), rCaptureSpace, *pCaptureData);
            rCaptureSpace.exchange_done();

            CoordspaceCartesianSimple::update_views(rCaptureSpace, *pCaptureData);
        }

        // Trajectory functions and other movement goes here

        if (hasArea)
        {
            // Update moved satellites in capture space
            SatActiveArea::update_capture(rUni, pArea->m_captureSpace);

            // Move the area itself
            SatActiveArea::move(rUni, areaSat, *pArea);

            // Scan for nearby satellites
            SatActiveArea::scan_radius(rUni, areaSat, *pArea,
                                       rUniScn.m_activation.m_activationRadius);
        }
    };
}


Satellite testapp::active_area_create(
        UniverseScene &rUniScn, osp::universe::coordspace_index_t targetIndex)
{
    using namespace osp::universe;

    Universe &rUni = rUniScn.m_universe;

    // create a Satellite
    Satellite areaSat = rUni.sat_create();

    // assign sat as an ActiveArea
    UCompActiveArea &rArea = rUniScn.m_activation.m_activeArea.emplace(areaSat);

    // captured satellites will be put into the target coordspace when released
    rArea.m_releaseSpace = targetIndex;

    // Create the "ActiveArea Domain" Coordinte Space
    // This is a CoordinateSpace that acts like a layer overtop of the target
    // CoordinateSpace. The ActiveArea is free to roam around in this space
    // unaffected by the target coordspace's trajectory function.
    {
        // Make the Domain CoordinateSpace identical to the target CoordinateSpace
        CoordinateSpace const &targetCoord = rUni.coordspace_get(targetIndex);

        // Make the Domain CoordinateSpace identical to the target CoordinateSpace
        auto const& [domainIndex, rDomainCoord] = rUni.coordspace_create(targetCoord.m_parentSat);
        rUni.coordspace_update_depth(domainIndex);
        rDomainCoord.m_pow2scale = targetCoord.m_pow2scale;

        rDomainCoord.m_data.emplace<CoordspaceCartesianSimple>();
        auto *pDomainData = entt::any_cast<CoordspaceCartesianSimple>(&rDomainCoord.m_data);

        // Add ActiveArea to the Domain Coordinate space
        rDomainCoord.add(areaSat, {}, {});
        rUni.coordspace_update_sats(domainIndex);
        pDomainData->update_exchange(rUni.sat_coordspaces(), rDomainCoord, *pDomainData);
        pDomainData->update_views(rDomainCoord, *pDomainData);
    }

    // Create the "ActiveArea Capture" CoordinateSpace
    // This is a coordinate space for Satellites captured inside of the
    // ActiveArea and can be modified in the ActiveScene, such as Vehicles.
    {
        auto const& [captureIndex, rCaptureSpace] = rUni.coordspace_create(areaSat);
        rUni.coordspace_update_depth(captureIndex);
        rCaptureSpace.m_data.emplace<CoordspaceCartesianSimple>();

        // Make the ActiveArea aware of the capture space's existance
        rArea.m_captureSpace = captureIndex;
    }

    return areaSat;
}

void testapp::active_area_destroy(
        UniverseScene &rUniScn, osp::universe::Satellite areaSat)
{
    using namespace osp::universe;

    Universe &rUni = rUniScn.m_universe;

    UCompActiveArea &rArea = rUniScn.m_activation.m_activeArea.get(areaSat);
    CoordinateSpace &rRelease = rUni.coordspace_get(rArea.m_releaseSpace);
    CoordinateSpace &rCapture = rUni.coordspace_get(rArea.m_captureSpace);
    auto viewSats = rCapture.ccomp_view<CCompSat>();
    auto viewPos = rCapture.ccomp_view_tuple<CCompX, CCompY, CCompZ>();

    CoordspaceTransform transform = rUni.coordspace_transform(rCapture, rRelease).value();

    for (uint32_t i = 0; i < viewSats.size(); i ++)
    {

        auto const posLocal = make_from_ccomp<Vector3g>(*viewPos, i);
        Vector3g pos = transform(posLocal);
        rCapture.remove(i);
        rRelease.add(viewSats[i], pos, {});
    }

}

UCompVehicle& testapp::add_vehicle(
        UniverseScene &rUniScn, osp::universe::Satellite sat,
        osp::DependRes<osp::BlueprintVehicle> blueprint)
{
    rUniScn.m_activation.m_activatable.emplace(sat);
    rUniScn.m_activation.m_activationRadius.emplace(sat);

    return rUniScn.m_solids.m_vehicles.emplace(sat, std::move(blueprint));
}

using planeta::universe::UCompPlanet;
using planeta::universe::SatPlanet;

UCompPlanet& testapp::add_planet(
        UniverseScene &rUniScn, Satellite sat, double radius, float mass,
        float activateRadius, float resolutionSurfaceMax, float resolutionScreenMax)
{
    rUniScn.m_activation.m_activatable.emplace(sat);
    rUniScn.m_activation.m_activationRadius.emplace(sat);

    return rUniScn.m_solids.m_planets.emplace(
                sat, radius, resolutionSurfaceMax, resolutionScreenMax, mass);
}


