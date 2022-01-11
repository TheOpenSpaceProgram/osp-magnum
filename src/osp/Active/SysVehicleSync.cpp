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

#if 0

#include "SysVehicleSync.h"

#include "SysAreaAssociate.h"
#include "SysHierarchy.h"
#include "SysPhysics.h"
#include "SysVehicle.h"
#include "drawing.h"

#include "../Satellites/SatActiveArea.h"
#include "../Satellites/SatVehicle.h"

#include <osp/logging.h>

using namespace osp::active;

using osp::universe::Universe;
using osp::universe::Satellite;

using osp::universe::CoordinateSpace;

using osp::universe::UCompActiveArea;
using osp::universe::UCompVehicle;

// for the 0xrrggbb_rgbf literalsm
using namespace Magnum::Math::Literals;

ActiveEnt SysVehicleSync::activate(ActiveScene &rScene, Universe &rUni,
                                   Satellite areaSat, Satellite tgtSat)
{
    OSP_LOG_INFO("Loading a vehicle");

    auto &loadMeVehicle = rUni.get_reg().get<universe::UCompVehicle>(tgtSat);
    auto &tgtPosTraj = rUni.get_reg().get<universe::UCompTransformTraj>(tgtSat);

    // make sure there is vehicle data to load
    if (loadMeVehicle.m_blueprint.empty())
    {
        // no vehicle data to load
        return entt::null;
    }

    BlueprintVehicle &vehicleData = *(loadMeVehicle.m_blueprint);
    ActiveEnt root = rScene.hier_get_root();

    // Create the root entity to add parts to

    ActiveEnt vehicleEnt = SysHierarchy::create_child(rScene, root, "Vehicle");

    rScene.reg_emplace<ACompActivatedSat>(vehicleEnt, tgtSat);

    ACompVehicle& vehicleComp = rScene.reg_emplace<ACompVehicle>(vehicleEnt);
    rScene.reg_emplace<ACompDrawTransform>(vehicleEnt);
    rScene.reg_emplace<ACompVehicleInConstruction>(
                    vehicleEnt, loadMeVehicle.m_blueprint);

    // Convert position of the satellite to position in scene
    Vector3 const positionInScene = rUni.sat_calc_pos_meters(areaSat, tgtSat).value();

    ACompTransform& rVehicleTransform = rScene.get_registry()
                                        .emplace<ACompTransform>(vehicleEnt);
    rVehicleTransform.m_transform
            = Matrix4::from(tgtPosTraj.m_rotation.toMatrix(), positionInScene);
    rScene.reg_emplace<ACompFloatingOrigin>(vehicleEnt);

    // Create the parts

    // Unique part prototypes used in the vehicle

    // Access with [blueprintParts.m_partIndex]

    std::vector<DependRes<PrototypePart> >& partsUsed = vehicleData.m_prototypes;


    // All the parts in the vehicle
    std::vector<BlueprintPart> &blueprintParts = vehicleData.m_blueprints;


    rScene.get_registry().reserve(rScene.get_registry().capacity()
                                   + vehicleData.m_machines.size());

    // Keep track of parts
    vehicleComp.m_parts.reserve(blueprintParts.size());

    // Loop through list of blueprint parts, and initialize each of them into
    // the ActiveScene
    for (BlueprintPart const& partBp : blueprintParts)
    {
        DependRes<PrototypePart>& partDepends =
                partsUsed[partBp.m_protoIndex];

        // Check if the part prototype this depends on still exists
        if (partDepends.empty())
        {
            return entt::null;
        }

        PrototypePart &proto = *partDepends;

        // Instantiate the part
        ActiveEnt partEntity = SysVehicle::part_instantiate(
                    rScene, proto, partBp, vehicleEnt);

        vehicleComp.m_parts.push_back(partEntity);

        auto& partPart = rScene.reg_emplace<ACompPart>(partEntity);
        partPart.m_vehicle = vehicleEnt;

        // Part now exists

        ACompTransform& partTransform = rScene.get_registry()
                                            .get<ACompTransform>(partEntity);

        // set the transformation
        partTransform.m_transform
                = Matrix4::from(partBp.m_rotation.toMatrix(),
                              partBp.m_translation)
                * Matrix4::scaling(partBp.m_scale);
    }


    // temporary: make the whole thing a single rigid body
    rScene.reg_emplace<ACompPhysBody>(vehicleEnt);
    rScene.reg_emplace<ACompPhysDynamic>(vehicleEnt);
    rScene.reg_emplace<ACompPhysLinearVel>(vehicleEnt);
    rScene.reg_emplace<ACompPhysAngularVel>(vehicleEnt);
    rScene.reg_emplace<ACompShape>(vehicleEnt, phys::EShape::Combined);
    rScene.reg_emplace<ACompSolidCollider>(vehicleEnt);

    return vehicleEnt;
}
#endif

#if 0
void SysVehicleSync::update_universe_sync(ACtxAreaLink& rLink)
{
    auto &rSync = rScene.get_registry().ctx<SyncVehicles>();

    auto const& area = rUni.get_reg().get<UCompActiveArea>(pLink->m_areaSat);

    // Delete vehicles that have gone too far from the ActiveArea range

    auto viewTf = rScene.get_registry().view<ACompTransform>();
    CoordinateSpace &rCapture = rUni.coordspace_get(area.m_captureSpace);
    CoordinateSpace &rRelease = rUni.coordspace_get(area.m_releaseSpace);
    universe::CoordspaceTransform releaseCoordTf
            = rUni.coordspace_transform(rCapture, rRelease).value();

    for (Satellite sat : pLink->m_leave)
    {
        if (!rUni.get_reg().all_of<UCompVehicle>(sat))
        {
            continue;
        }

        // Get vehicle entity by Satellite
        auto vehicleIt = rSync.m_inArea.find(sat);
        ActiveEnt vehicle = vehicleIt->second;

        // Get position in scene, and transform to release coordspace
        auto const& vehicleTf = viewTf.get<ACompTransform>(vehicle);
        universe::Vector3g posInScene(vehicleTf.m_transform.translation()
                                       * universe::gc_units_per_meter);
        universe::Vector3g posReleased = releaseCoordTf(posInScene);

        // Handoff satellite to release coordinate space
        auto const& index = rUni.get_reg().get<universe::UCompCoordspaceIndex>(sat);
        rCapture.remove(index.m_myIndex);
        rRelease.add(sat, posReleased, {});

        // Mark entity for deletion
        SysHierarchy::mark_delete_cut(rScene, vehicle);
        rScene.get_registry().remove<ACompActivatedSat>(vehicle);

        // Remove Satellite-Entity association
        rSync.m_inArea.erase(vehicleIt);
    }

    // Update transforms of already-activated vehicle satellites

    auto viewVehicles = rScene.get_registry().view<ACompVehicle,
                                                   ACompActivatedSat>();

    for (ActiveEnt vehicleEnt : viewVehicles)
    {
        auto const &vehicleTf = viewTf.get<ACompTransform>(vehicleEnt);
        auto const &vehicleSat = viewVehicles.get<ACompActivatedSat>(vehicleEnt);

        // Position in scene is 1:1 with position in Capture CoordinateSpace
        universe::Vector3g const pos{vehicleTf.m_transform.translation()
                                      * universe::gc_units_per_meter};

        // Queue move. This will be applied during universe update
        rCapture.command({vehicleSat.m_sat, CoordinateSpace::ECmdOp::Set,
                          CoordinateSpace::CmdPosition{pos}});
    }

    // Activate nearby vehicle satellites that have just entered the ActiveArea

    for (Satellite sat : pLink->m_enter)
    {
        if (!rUni.get_reg().all_of<UCompVehicle>(sat))
        {
            continue;
        }

        // Move vehicle to Active Area's capture coordinate space
        universe::SatActiveArea::capture(rUni, pLink->m_areaSat, {&sat, 1});

        ActiveEnt ent = activate(rScene, rUni, pLink->m_areaSat, sat);
        rSync.m_inArea[sat] = ent;
    }
}

#endif
