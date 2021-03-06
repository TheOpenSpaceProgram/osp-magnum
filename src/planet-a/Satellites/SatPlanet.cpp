/**
 * Open Space Program
 * Copyright © 2019-2020 Open Space Program Project
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

#include "SatPlanet.h"

using osp::universe::Universe;
using osp::universe::Satellite;
using osp::universe::UCompActivatable;
using osp::universe::UCompActivationRadius;

using planeta::universe::UCompPlanet;
using planeta::universe::SatPlanet;

UCompPlanet& SatPlanet::add_planet(
        osp::universe::Universe& rUni, Satellite sat, double radius, float mass,
        float resolutionSurfaceMax, float resolutionScreenMax)
{
    bool typeSetSuccess = rUni.sat_type_try_set(
                sat, rUni.sat_type_find_index(SatPlanet::smc_name));
    assert(typeSetSuccess);

    rUni.get_reg().emplace<UCompActivatable>(sat);
    rUni.get_reg().emplace<UCompActivationRadius>(sat, float(radius));

    return rUni.get_reg().emplace<UCompPlanet>(
                sat, radius, resolutionSurfaceMax, resolutionScreenMax, mass);
}
