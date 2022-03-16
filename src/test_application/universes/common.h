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

#pragma once

#include <planet-a/Satellites/SatPlanet.h>

#include <osp/Resource/resourcetypes.h>

#include <osp/universetypes.h>
#include <osp/Satellites/SatVehicle.h>

#include <functional>

namespace osp { struct BlueprintVehicle; }

namespace testapp::unistate
{

using namespace osp::universe;

struct Activation
{
    ucomp_storage_t< UCompActivatable >         m_activatable;
    ucomp_storage_t< UCompActivationAlways >    m_activationAlways;
    ucomp_storage_t< UCompActivationRadius >    m_activationRadius;
    ucomp_storage_t< UCompActiveArea >          m_activeArea;
};

using namespace planeta::universe;

struct Solids
{
    ucomp_storage_t< UCompVehicle >             m_vehicles;
    ucomp_storage_t< UCompPlanet >              m_planets;
};

} // namespace testapp::unistate

namespace testapp
{

struct UniverseScene
{
    osp::universe::Universe     m_universe;

    unistate::Activation        m_activation;
    unistate::Solids            m_solids;
};

using universe_update_t = std::function<void(UniverseScene&)>;

std::unique_ptr<UniverseScene> setup_universe_scene();

/**
 * @brief Generate an update function for a universe consisting of just a single
 *        CoordspaceCartesianSimple coordinate space and no movement
 *
 * @param cartesian [in] Index to cartesian coordinate space
 *
 * @return Universe update function
 */
std::function<void(UniverseScene&)> generate_simple_universe_update(
        osp::universe::coordspace_index_t cartesian);

osp::universe::Satellite active_area_create(
        UniverseScene &rUniScn, osp::universe::coordspace_index_t targetIndex);

void active_area_destroy(UniverseScene &rUniScn, osp::universe::Satellite areaSat);

osp::universe::UCompVehicle& add_vehicle(
        UniverseScene &rUniScn, osp::universe::Satellite sat,
        osp::ResIdOwner_t blueprint);

planeta::universe::UCompPlanet& add_planet(
        UniverseScene &rUniScn, osp::universe::Satellite sat,
        double radius, float mass,
                float activateRadius, float resolutionSurfaceMax, float resolutionScreenMax);

} // namespace testapp
