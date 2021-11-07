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

#include "scene_util.h"

#include "CameraController.h"

#include <osp/Active/activetypes.h>
#include <osp/Active/basic.h>
#include <osp/Active/physics.h>
#include <osp/Active/drawing.h>
#include <osp/Active/machines.h>

#include <osp/Active/SysVehicle.h>

#include <osp/id_registry.h>

#include <adera/Machines/Container.h>
#include <adera/Machines/RCSController.h>
#include <adera/Machines/Rocket.h>
#include <adera/Machines/UserControl.h>


using osp::Scene;
using osp::active::acomp_storage_t;

void testapp::setup_scene_active(Scene& rScene, SceneMeta& rMeta)
{
    using namespace osp::active;

    // Create ID registry for managine entities
    rMeta.set< osp::IdRegistry<ActiveEnt> >(rScene);

    // Create storages for basic components from osp/basic.h
    rMeta.set< acomp_storage_t<ACompTransform> >(rScene);
    rMeta.set< acomp_storage_t<ACompTransformControlled> >(rScene);
    rMeta.set< acomp_storage_t<ACompTransformMutable> >(rScene);
    rMeta.set< acomp_storage_t<ACompFloatingOrigin> >(rScene);
    rMeta.set< acomp_storage_t<ACompDelete> >(rScene);
    rMeta.set< acomp_storage_t<ACompName> >(rScene);
    rMeta.set< acomp_storage_t<ACompHierarchy> >(rScene);
    rMeta.set< acomp_storage_t<ACompMass> >(rScene);
    rMeta.set< acomp_storage_t<ACompCamera> >(rScene);
}

void testapp::setup_scene_physics(Scene& rScene, SceneMeta& rMeta)
{
    using namespace osp::active;

    rMeta.set< ACtxPhysics >(rScene);

    // Create storages for physics components from osp/Active/physics.h
    rMeta.set< acomp_storage_t<ACompPhysBody> >(rScene);
    rMeta.set< acomp_storage_t<ACompPhysDynamic> >(rScene);
    rMeta.set< acomp_storage_t<ACompPhysLinearVel> >(rScene);
    rMeta.set< acomp_storage_t<ACompPhysAngularVel> >(rScene);
    rMeta.set< acomp_storage_t<ACompPhysNetForce> >(rScene);
    rMeta.set< acomp_storage_t<ACompPhysNetTorque> >(rScene);
    rMeta.set< acomp_storage_t<ACompRigidbodyAncestor> >(rScene);
    rMeta.set< acomp_storage_t<ACompShape> >(rScene);
    rMeta.set< acomp_storage_t<ACompSolidCollider> >(rScene);
}

void testapp::setup_scene_drawable(Scene& rScene, SceneMeta& rMeta)
{
    using namespace osp::active;

    //rMeta.set< ACtxRenderGroups >(rScene);

    // Create storages for drawing components from osp/Active/drawing.h
    rMeta.set< acomp_storage_t<ACompMaterial> >(rScene);
    rMeta.set< acomp_storage_t<ACompRenderingAgent> >(rScene);
    rMeta.set< acomp_storage_t<ACompPerspective3DView> >(rScene);
    rMeta.set< acomp_storage_t<ACompOpaque> >(rScene);
    rMeta.set< acomp_storage_t<ACompTransparent> >(rScene);
    rMeta.set< acomp_storage_t<ACompVisible> >(rScene);
    rMeta.set< acomp_storage_t<ACompDrawTransform> >(rScene);
}

void testapp::setup_scene_vehicles(Scene& rScene, SceneMeta& rMeta)
{
    using namespace osp::active;

    // Create storages for vehicle components from osp/Active/SysVehicle.h
    rMeta.set< acomp_storage_t<ACompMachines> >(rScene);
    rMeta.set< acomp_storage_t<ACompVehicle> >(rScene);
    rMeta.set< acomp_storage_t<ACompVehicleInConstruction> >(rScene);
    rMeta.set< acomp_storage_t<ACompPart> >(rScene);
}

void testapp::setup_scene_machines(Scene& rScene, SceneMeta& rMeta)
{
    using namespace osp::active;
    using namespace adera::active::machines;

    // Machines don't have anything in common with ActiveEnt, hence
    // they should be their own entity type
    rMeta.set< osp::IdRegistry<MachineEnt> >(rScene);

    // Machines
    rMeta.set< mcomp_storage_t<MCompContainer> >(rScene);
    rMeta.set< mcomp_storage_t<MCompRCSController> >(rScene);
    rMeta.set< mcomp_storage_t<MCompRocket> >(rScene);
    rMeta.set< mcomp_storage_t<MCompUserControl> >(rScene);

    // Setup Wiring
    rMeta.set< ACtxWireNodes<adera::wire::AttitudeControl> >(rScene);
    rMeta.set< ACtxWireNodes<adera::wire::Percent> >(rScene);

}

void testapp::setup_scene_flight(Scene& rScene, SceneMeta& rMeta)
{

    // There's only 1 camera controller but too bad!
    rMeta.set< acomp_storage_t<ACompCameraController> >(rScene);

    {
        using namespace osp::active;

        // Create storages for physics components from osp/physics.h

    }
}
