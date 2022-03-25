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

#if 0

#include "vehicles.h"
#include "../VehicleBuilder.h"

#include <adera/ShipResources.h>
#include <adera/Machines/Container.h>
#include <adera/Machines/RCSController.h>
#include <adera/Machines/Rocket.h>
#include <adera/Machines/UserControl.h>

#include <osp/Active/SysWire.h>
#include <osp/Satellites/SatVehicle.h>

#include <iostream>

using namespace testapp;

using osp::Vector2;
using osp::Vector3;
using osp::Matrix4;
using osp::Quaternion;

using osp::Package;
using osp::DependRes;

using osp::universe::Universe;
using osp::universe::Satellite;
using osp::universe::SatVehicle;
using osp::universe::UCompVehicle;

using osp::BlueprintVehicle;
using osp::BlueprintMachine;
using osp::PrototypePart;

using adera::wire::AttitudeControl;
using adera::wire::Percent;

using adera::active::machines::MCompContainer;
using adera::active::machines::MCompRCSController;
using adera::active::machines::MCompRocket;
using adera::active::machines::MCompUserControl;


/**
 * Adds multiple specified parts (RCS thrusters) in the same position, but
 * rotated 90 degrees of each other along the Z axis for attitude control.
 *
 * @param rBlueprint [out] Vehicle blueprint to add RCS block to
 * @param rRcs       [in] Prototype of RCS thruster
 * @param rRcsPorts  [out] Vector to put indices of created thrusters into
 * @param pos        [in] Position of block
 * @param rot        [in] Rotation of block
 */
void blueprint_add_rcs_block(
        BlueprintVehicle &rBlueprint, DependRes<PrototypePart> rRcs,
        std::vector<part_t> &rRcsPorts, Vector3 pos, Quaternion rot);


osp::universe::Satellite testapp::debug_add_deterministic_vehicle(
        UniverseScene& rUniScn, osp::Package& rPkg, std::string_view name)
{

    // Begin blueprint
    VehicleBuilder blueprint;

    // Part to add
    DependRes<PrototypePart> rocket = rPkg.get<PrototypePart>("part_stomper");
    blueprint.part_add(rocket, Vector3(0.0f), Quaternion(), Vector3(1.0f));

    // Wire throttle control
    // from (output): a MCompUserControl m_woThrottle
    // to    (input): a MCompRocket m_wiThrottle
    //blueprint.add_wire(0, 0, 1,
    //    0, 1, 2);

    // Wire attitude control to gimbal
    // from (output): a MCompUserControl m_woAttitude
    // to    (input): a MCompRocket m_wiGimbal
    //blueprint.add_wire(0, 0, 0,
    //    0, 1, 0);

    // Save blueprint
    DependRes<BlueprintVehicle> depend = rPkg.add<BlueprintVehicle>(
                std::string{name}, blueprint.export_move());

    // Create new satellite
    Satellite sat = rUniScn.m_universe.sat_create();

    // Set name
    //auto& posTraj = uni.get_reg().get<UCompTransformTraj>(sat);
    //posTraj.m_name = name;

    // Make the satellite into a vehicle
    //add_vehicle(uni, sat, std::move(depend));

    return sat;
}

osp::universe::Satellite testapp::debug_add_random_vehicle(
        UniverseScene& rUniScn, osp::Package& rPkg,
        std::string_view name)
{

    // Start making the blueprint
    VehicleBuilder blueprint;

    // Part to add, very likely a spamcan
    DependRes<PrototypePart> victim = rPkg.get<PrototypePart>("part_spamcan");

    // Add 12 parts
    for (int i = 0; i < 12; i ++)
    {
        // Generate random vector
        Vector3 randomvec(std::rand() % 64 - 32,
                          std::rand() % 64 - 32,
                          (i - 6) * 12);

        randomvec /= 64.0f;

        // Add a new [victim] part
        blueprint.part_add(victim, randomvec,
                           Quaternion(), Vector3(1, 1, 1));
        //std::cout << "random: " <<  << "\n";
    }

    // Wire throttle control
    // from (output): a MCompUserControl m_woThrottle
    // to    (input): a MCompRocket m_wiThrottle
    //blueprint.add_wire(0, 0, 1,
    //                   0, 1, 2);

    // Wire attitude control to gimbal
    // from (output): a MCompUserControl m_woAttitude
    // to    (input): a MCompRocket m_wiGimbal
    //blueprint.add_wire(0, 0, 0,
    //                   0, 1, 0);

    // put blueprint in package
    DependRes<BlueprintVehicle> depend = rPkg.add<BlueprintVehicle>(
                std::string{name}, blueprint.export_move());

    // Create the Satellite containing a SatVehicle

    // Create blank satellite
    Satellite sat = rUniScn.m_universe.sat_create();

    // Set the name
    //auto &posTraj = uni.get_reg().get<UCompTransformTraj>(sat);
    //posTraj.m_name = name;

    // Make the satellite into a vehicle
    add_vehicle(rUniScn, sat, std::move(depend));

    return sat;

}

void blueprint_add_rcs_block(
        VehicleBuilder &rBlueprint, DependRes<PrototypePart> rRcs,
        std::vector<part_t> &rRcsPorts, Vector3 pos, Quaternion rot)
{
    using namespace Magnum::Math::Literals;

    Vector3 constexpr scl{1};
    Vector3 constexpr zAxis{0, 0, 1};

    rRcsPorts.push_back(rBlueprint.part_add(rRcs, pos, Quaternion::rotation(90.0_degf, zAxis) * rot, scl));
    //rBlueprint.add_part(rRcs, pos, rot, scl);
    rRcsPorts.push_back(rBlueprint.part_add(rRcs, pos, Quaternion::rotation(-90.0_degf, zAxis) * rot, scl));
}

osp::universe::Satellite testapp::debug_add_part_vehicle(
    UniverseScene& rUniScn, osp::Package& rPkg,
    std::string_view name)
{
    using namespace Magnum::Math::Literals;
    using Magnum::Rad;

    // Start making the blueprint
    VehicleBuilder blueprint;

    // Parts
    DependRes<PrototypePart> capsule = rPkg.get<PrototypePart>("part_phCapsule");
    DependRes<PrototypePart> fuselage = rPkg.get<PrototypePart>("part_phFuselage");
    DependRes<PrototypePart> engine = rPkg.get<PrototypePart>("part_phEngine");
    DependRes<PrototypePart> rcs = rPkg.get<PrototypePart>("part_phLinRCS");

    Vector3 cfOset = VehicleBuilder::part_offset(*capsule, "attach_bottom_capsule",
        *fuselage, "attach_top_fuselage");
    Vector3 feOset = VehicleBuilder::part_offset(*fuselage, "attach_bottom_fuselage",
        *engine, "attach_top_eng");


    Quaternion idRot;
    Vector3 scl{1};
    Magnum::Rad qtrTurn(-90.0_degf);
    Quaternion rotY_090 = Quaternion::rotation(qtrTurn, Vector3{0, 1, 0});
    Quaternion rotZ_090 = Quaternion::rotation(qtrTurn, Vector3{0, 0, 1});
    Quaternion rotZ_180 = Quaternion::rotation(2 * qtrTurn, Vector3{0, 0, 1});
    Quaternion rotZ_270 = Quaternion::rotation(3 * qtrTurn, Vector3{0, 0, 1});

    part_t partCapsule = blueprint.part_add(capsule, Vector3{0}, idRot, scl);

    part_t partFusalage = blueprint.part_add(fuselage, cfOset, idRot, scl);
    BlueprintMachine* fusalageMach= blueprint.machine_find_ptr<MCompContainer>(partFusalage);
    fusalageMach->m_config.emplace("resourcename", "lzdb:fuel");
    fusalageMach->m_config.emplace("fuellevel", 0.5);

    part_t partEngine = blueprint.part_add(engine, cfOset + feOset, idRot, scl);

    // Add a shit ton of RCS rings

    Rad rcsRingStep     = 90.0_degf;
    float rcsZMin       = -2.0f;
    float rcsZMax       = 2.1f;
    float rcsZStep      = 4.0f;
    float rcsRadius     = 1.1f;

    std::vector<part_t> rcsPorts;

    for (float z = rcsZMin; z < rcsZMax; z += rcsZStep)
    {
        Vector3 rcsOset = cfOset + Vector3{rcsRadius, 0.0f, z};

        // Add RCS rings from 0 to 360 degrees
        for (Rad ang = 0.0_degf; ang < Rad(360.0_degf); ang += rcsRingStep)
        {
            Quaternion rotZ = Quaternion::rotation(ang, Vector3{0, 0, 1});
            blueprint_add_rcs_block(blueprint, rcs, rcsPorts,
                                    rotZ.transformVector(rcsOset),
                                    rotZ * rotY_090);
        }
    }


    std::cout << "Part vehicle has " << blueprint.part_count()
              << " Parts!\n";

    // Wire throttle control
    // from (output): a MCompUserControl m_woThrottle
    // to    (input): a MCompRocket m_wiThrottle
    blueprint.wire_connect_signal<Percent>(
        partCapsule, blueprint.machine_find<MCompUserControl>(partCapsule), MCompUserControl::smc_woThrottle,
        partEngine, blueprint.machine_find<MCompRocket>(partEngine), MCompRocket::smc_wiThrottle);

    // Wire attitude contrl to gimbal
    // from (output): a MCompUserControl m_woAttitude
    // to    (input): a MCompRocket m_wiGimbal
    //blueprint.add_wire(
    //    Parts::CAPSULE, 0, 0,
    //    Parts::ENGINE, 0, 0);

    // Pipe fuel tank to rocket engine
    // from (output): fuselage MCompContainer m_outputs;
    // to    (input): entine MCompRocket m_resourcesLines[0]
    //blueprint.add_wire(Parts::FUSELAGE, 0, 0,
    //    Parts::ENGINE, 0, 3);

    mach_t const partCapsuleUsrCtrl = blueprint.machine_find<MCompUserControl>(partCapsule);

    for (auto partRCS : rcsPorts)
    {
        mach_t const partRCSRocket = blueprint.machine_find<MCompRocket>(partRCS);
        mach_t const partRCSCtrl = blueprint.machine_find<MCompRCSController>(partRCS);

        // Attitude control -> RCS Control
        blueprint.wire_connect_signal<AttitudeControl>(
                partCapsule, partCapsuleUsrCtrl, MCompUserControl::smc_woAttitude,
                partRCS, partRCSCtrl, MCompRCSController::smc_wiCommandOrient);

        // RCS Control -> RCS Rocket
        blueprint.wire_connect_signal<Percent>(
                partRCS, partRCSCtrl, MCompRCSController::m_woThrottle,
                partRCS, partRCSRocket, MCompRocket::smc_wiThrottle);

        // Fuselage tank -> RCS Rocket
        //blueprint.add_wire(Parts::FUSELAGE, 0, 0,
        //    port, 1, 3);
    }

    // Put blueprint in package
    auto depend = rPkg.add<BlueprintVehicle>(std::string{name},
                                            blueprint.export_move());

    Satellite sat = rUniScn.m_universe.sat_create();

    // Set the name
    //auto& posTraj = uni.get_reg().get<osp::universe::UCompTransformTraj>(sat);
    //posTraj.m_name = name;

    // Make the satellite into a vehicle
    add_vehicle(rUniScn, sat, std::move(depend));

    return sat;
}

#endif
