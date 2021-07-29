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
#pragma once

#include "SatActiveArea.h"

#include "../Universe.h"

#include "../Resource/Resource.h"
#include "../Resource/blueprints.h"


namespace osp::universe
{

struct UCompVehicle
{
    DependRes<BlueprintVehicle> m_blueprint;
};

class SatVehicle
{
public:

    static constexpr std::string_view smc_name = "Vehicle";

    /**
     * Set the type of a Satellite and add a UCompVehicle to it
     * @param rUni      [out] Universe containing satellite
     * @param sat       [in] Satellite add a vehicle to
     * @param blueprint [in] Vehicle data to open when activated
     * @return Reference to UCompVehicle created
     */
    static UCompVehicle& add_vehicle(
        osp::universe::Universe& rUni, osp::universe::Satellite sat,
        DependRes<BlueprintVehicle> blueprint);



};

}
