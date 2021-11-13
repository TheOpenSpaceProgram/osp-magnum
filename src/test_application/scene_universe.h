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

#include <osp/Universe.h>

#include <osp/Satellites/SatActiveArea.h>
#include <osp/Satellites/SatVehicle.h>

namespace testapp
{

namespace unistate
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

}

struct UniverseScene
{
    osp::universe::Universe     m_universe;

    unistate::Activation        m_activation;
    unistate::Solids            m_solids;
};

}
