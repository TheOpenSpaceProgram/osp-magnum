#pragma once

#include <osp/Universe.h>
#include <osp/Resource/Package.h>

using osp::Vector2;
using osp::Vector3;
using osp::Matrix4;
using osp::Quaternion;

/**
 *
 * @param uni [in,out]
 * @param pkg [in,out]
 * @param name [in]
 * @return Satellite containing UCompVehicle
 */
osp::universe::Satellite debug_add_deterministic_vehicle(
        osp::universe::Universe& uni, osp::Package& pkg,
        std::string const & name);

/**
 * Creates a vehicle satellite and adds a random mess of 10 part_spamcans to it.
 * This will find "part_spamcan" from the specified package and use it to
 * construct a BlueprintVehicle. This BlueprintVehicle is saved to the package.
 * A new Satellite created in the specified universe will be returned,
 * containing a UCompVehicle with its blueprint set to the new vehicle.
 *
 * @param uni [in,out]
 * @param pkg [in,out]
 * @param name [in]
 * @return Satellite containing UCompVehicle
 */
osp::universe::Satellite debug_add_random_vehicle(
        osp::universe::Universe& uni, osp::Package& pkg,
        std::string const & name);
