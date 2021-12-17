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

#include "common.h"

#include <osp/Universe.h>
#include <osp/Resource/Package.h>
#include <string_view>

namespace testapp
{

/**
 * Creates a vehicle satellite and adds a part_stomper to it, being very
 * deterministic indeed. This will find "part_stomper" from the specified
 * package, and use it to construct a BlueprintVehicle. This BlueprintVehicle
 * is saved to the package.
 *
 * @param uni [in,out] Universe to create the vehicle in
 * @param pkg [in,out] Package to search for parts and save the blueprint
 * @param name [in] Name of new vehicle used for blueprint and satellite
 * @return Satellite containing UCompVehicle
 */
osp::universe::Satellite debug_add_deterministic_vehicle(
        UniverseScene& rUniScn, osp::Package& rPkg,
        std::string_view name);

/**
 * Creates a vehicle satellite and adds a random mess of 10 part_spamcans to it.
 * This will find "part_spamcan" from the specified package and use it to
 * construct a BlueprintVehicle. This BlueprintVehicle is saved to the package.
 * A new Satellite created in the specified universe will be returned,
 * containing a UCompVehicle with its blueprint set to the new vehicle.
 *
 * @param uni [in,out] Universe to create the vehicle in
 * @param pkg [in,out] Package to search for parts and save the blueprint
 * @param name [in] Name of new vehicle used for blueprint and satellite
 * @return Satellite containing UCompVehicle
 */
osp::universe::Satellite debug_add_random_vehicle(
        UniverseScene& rUniScn, osp::Package& rPkg,
        std::string_view name);

/**
 * Creates a vehicle satellite and adds a multi-part rocket to it.
 * This will fetch the parts "ph_capsule", "ph_fuselage", and "ph_engine" (ph
 * here stands for "placeholder") and link them into a single BlueprintVehicle.
 *
 * @param uni [in,out] Universe to create the vehicle in
 * @param pkg [in,out] Package to search for parts and save the blueprint
 * @param name [in] Name of the vehicle used for blueprint and satellite
 * @return Satellite containing UCompVehicle
 */
osp::universe::Satellite debug_add_part_vehicle(
    UniverseScene& rUniScn, osp::Package& rPkg,
    std::string_view name);

// TODO: put test with creating a universe with just vehicles

}
