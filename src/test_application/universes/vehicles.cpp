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

#include "vehicles.h"

#include <osp/Satellites/SatVehicle.h>

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
using osp::universe::UCompTransformTraj;
using osp::universe::UCompVehicle;

using osp::BlueprintVehicle;
using osp::PrototypePart;


/**
 * Utility: computes the displacement between two parts, relative to the
 * specified sub-object (e.g. an attachment node).
 *
 * @param attachTo [in] The parent part
 * @param attachToName [in] The name of the parent's object/attach point
 * @param toAttach [in] The child part
 * @param toAttachName [in] The name of the child's object/attach point
 */
Vector3 part_offset(
    PrototypePart const& attachTo, std::string_view attachToName,
    PrototypePart const& toAttach, std::string_view toAttachName);

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
        std::vector<int> &rRcsPorts, Vector3 pos, Quaternion rot);


osp::universe::Satellite testapp::debug_add_deterministic_vehicle(
        Universe& uni, Package& pkg, std::string_view name)
{
    // Begin blueprint
    BlueprintVehicle blueprint;

    // Part to add
    DependRes<PrototypePart> rocket = pkg.get<PrototypePart>("part_stomper");
    blueprint.add_part(rocket, Vector3(0.0f), Quaternion(), Vector3(1.0f));

    // Wire throttle control
    // from (output): a MachineUserControl m_woThrottle
    // to    (input): a MachineRocket m_wiThrottle
    blueprint.add_wire(0, 0, 1,
        0, 1, 2);

    // Wire attitude control to gimbal
    // from (output): a MachineUserControl m_woAttitude
    // to    (input): a MachineRocket m_wiGimbal
    blueprint.add_wire(0, 0, 0,
        0, 1, 0);

    // Save blueprint
    DependRes<BlueprintVehicle> depend =
        pkg.add<BlueprintVehicle>(std::string{name}, std::move(blueprint));

    // Create new satellite
    Satellite sat = uni.sat_create();

    // Set name
    auto& posTraj = uni.get_reg().get<UCompTransformTraj>(sat);
    posTraj.m_name = name;

    // Make the satellite into a vehicle
    SatVehicle::add_vehicle(uni, sat, std::move(depend));

    return sat;
}

osp::universe::Satellite testapp::debug_add_random_vehicle(
        osp::universe::Universe& uni, osp::Package& pkg,
        std::string_view name)
{

    // Start making the blueprint
    BlueprintVehicle blueprint;

    // Part to add, very likely a spamcan
    DependRes<PrototypePart> victim = pkg.get<PrototypePart>("part_spamcan");

    // Add 12 parts
    for (int i = 0; i < 12; i ++)
    {
        // Generate random vector
        Vector3 randomvec(std::rand() % 64 - 32,
                          std::rand() % 64 - 32,
                          (i - 6) * 12);

        randomvec /= 64.0f;

        // Add a new [victim] part
        blueprint.add_part(victim, randomvec,
                           Quaternion(), Vector3(1, 1, 1));
        //std::cout << "random: " <<  << "\n";
    }

    // Wire throttle control
    // from (output): a MachineUserControl m_woThrottle
    // to    (input): a MachineRocket m_wiThrottle
    blueprint.add_wire(0, 0, 1,
                       0, 1, 2);

    // Wire attitude control to gimbal
    // from (output): a MachineUserControl m_woAttitude
    // to    (input): a MachineRocket m_wiGimbal
    blueprint.add_wire(0, 0, 0,
                       0, 1, 0);

    // put blueprint in package
    DependRes<BlueprintVehicle> depend =
        pkg.add<BlueprintVehicle>(std::string{name}, std::move(blueprint));

    // Create the Satellite containing a SatVehicle

    // Create blank satellite
    Satellite sat = uni.sat_create();

    // Set the name
    auto &posTraj = uni.get_reg().get<UCompTransformTraj>(sat);
    posTraj.m_name = name;

    // Make the satellite into a vehicle
    SatVehicle::add_vehicle(uni, sat, std::move(depend));

    return sat;

}

Vector3 part_offset(PrototypePart const& attachTo,
    std::string_view attachToName, PrototypePart const& toAttach,
    std::string_view toAttachName)
{
    Vector3 oset1{0.0f};
    Vector3 oset2{0.0f};

    for (osp::PCompName const& name : attachTo.m_partName)
    {
        if (name.m_name == attachToName)
        {
            oset1 = attachTo.m_partTransform[name.m_entity].m_translation;
            break;
        }
    }

    for (osp::PCompName const& name : toAttach.m_partName)
    {
        if (name.m_name == toAttachName)
        {
            oset2 = toAttach.m_partTransform[name.m_entity].m_translation;
            break;
        }
    }

    return oset1 - oset2;
}

void blueprint_add_rcs_block(
        BlueprintVehicle &rBlueprint, DependRes<PrototypePart> rRcs,
        std::vector<int> &rRcsPorts, Vector3 pos, Quaternion rot)
{
    using namespace Magnum::Math::Literals;

    Vector3 constexpr scl{1};
    Vector3 constexpr zAxis{0, 0, 1};
    int hackypartnum = rBlueprint.get_blueprints().size();

    rBlueprint.add_part(rRcs, pos, Quaternion::rotation(90.0_degf, zAxis) * rot, scl);
    //rBlueprint.add_part(rRcs, pos, rot, scl);
    rBlueprint.add_part(rRcs, pos, Quaternion::rotation(-90.0_degf, zAxis) * rot, scl);

    for (int i = 0; i < 2; i ++)
    {
        rRcsPorts.push_back(hackypartnum + i);
    }
}

osp::universe::Satellite testapp::debug_add_part_vehicle(
    osp::universe::Universe& uni, osp::Package& pkg,
    std::string_view name)
{
    using namespace Magnum::Math::Literals;
    using Magnum::Rad;

    // Start making the blueprint
    BlueprintVehicle blueprint;

    // Parts
    DependRes<PrototypePart> capsule = pkg.get<PrototypePart>("part_phCapsule");
    DependRes<PrototypePart> fuselage = pkg.get<PrototypePart>("part_phFuselage");
    DependRes<PrototypePart> engine = pkg.get<PrototypePart>("part_phEngine");
    DependRes<PrototypePart> rcs = pkg.get<PrototypePart>("part_phLinRCS");

    Vector3 cfOset = part_offset(*capsule, "attach_bottom_capsule",
        *fuselage, "attach_top_fuselage");
    Vector3 feOset = part_offset(*fuselage, "attach_bottom_fuselage",
        *engine, "attach_top_eng");


    Quaternion idRot;
    Vector3 scl{1};
    Magnum::Rad qtrTurn(-90.0_degf);
    Quaternion rotY_090 = Quaternion::rotation(qtrTurn, Vector3{0, 1, 0});
    Quaternion rotZ_090 = Quaternion::rotation(qtrTurn, Vector3{0, 0, 1});
    Quaternion rotZ_180 = Quaternion::rotation(2 * qtrTurn, Vector3{0, 0, 1});
    Quaternion rotZ_270 = Quaternion::rotation(3 * qtrTurn, Vector3{0, 0, 1});

    blueprint.add_part(capsule, Vector3{0}, idRot, scl);

    auto& fuselageBP = blueprint.add_part(fuselage, cfOset, idRot, scl);
    //fuselageBP.m_machines[1].m_config.emplace("resourcename", "lzdb:fuel");
    //fuselageBP.m_machines[1].m_config.emplace("fuellevel", 0.5);

    auto& engBP = blueprint.add_part(engine, cfOset + feOset, idRot, scl);

    // Add a shit ton of RCS rings

    Rad rcsRingStep     = 90.0_degf;
    float rcsZMin       = -2.0f;
    float rcsZMax       = 2.1f;
    float rcsZStep      = 4.0f;
    float rcsRadius     = 1.1f;

    std::vector<int> rcsPorts;

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

    enum Parts
    {
        CAPSULE = 0,
        FUSELAGE = 1,
        ENGINE = 2
    };

    std::cout << "Part vehicle has " << blueprint.get_blueprints().size()
              << " Parts!\n";

    // Wire throttle control
    // from (output): a MachineUserControl m_woThrottle
    // to    (input): a MachineRocket m_wiThrottle
    blueprint.add_wire(
        Parts::CAPSULE, 0, 1,
        Parts::ENGINE, 0, 2);

    // Wire attitude contrl to gimbal
    // from (output): a MachineUserControl m_woAttitude
    // to    (input): a MachineRocket m_wiGimbal
    blueprint.add_wire(
        Parts::CAPSULE, 0, 0,
        Parts::ENGINE, 0, 0);

    // Pipe fuel tank to rocket engine
    // from (output): fuselage MachineContainer m_outputs;
    // to    (input): entine MachineRocket m_resourcesLines[0]
    blueprint.add_wire(Parts::FUSELAGE, 0, 0,
        Parts::ENGINE, 0, 3);

    for (auto port : rcsPorts)
    {
        // Attitude control -> RCS Control
        blueprint.add_wire(Parts::CAPSULE, 0, 0,
            port, 0, 0);
        // RCS Control -> RCS Rocket
        blueprint.add_wire(port, 0, 0,
            port, 1, 2);
        // Fuselage tank -> RCS Rocket
        blueprint.add_wire(Parts::FUSELAGE, 0, 0,
            port, 1, 3);
    }

    // Put blueprint in package
    auto depend = pkg.add<BlueprintVehicle>(std::string{name}, std::move(blueprint));

    Satellite sat = uni.sat_create();

    // Set the name
    auto& posTraj = uni.get_reg().get<osp::universe::UCompTransformTraj>(sat);
    posTraj.m_name = name;

    // Make the satellite into a vehicle
    SatVehicle::add_vehicle(uni, sat, std::move(depend));

    return sat;
}
