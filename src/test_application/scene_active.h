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

#include "CameraController.h"

#include <osp/Active/activetypes.h>
#include <osp/Active/basic.h>
#include <osp/Active/physics.h>
#include <osp/Active/drawing.h>
#include <osp/Active/machines.h>

#include <osp/Active/SysAreaAssociate.h>
#include <osp/Active/SysVehicle.h>
#include <osp/Active/SysVehicleSync.h>

#include <osp/id_registry.h>

#include <adera/Machines/Container.h>
#include <adera/Machines/RCSController.h>
#include <adera/Machines/Rocket.h>
#include <adera/Machines/UserControl.h>

// Forward declare physics engine world
namespace ospnewton { struct ACtxNwtWorld; }

namespace testapp
{

namespace scenestate
{

using namespace osp::active;
using namespace adera::active::machines;

using ActiveIds_t = osp::IdRegistry<ActiveEnt>;
using MachineIds_t = osp::IdRegistry<MachineEnt>;

/**
 * @brief Storage for basic components from osp/basic.h
 */
struct Basic
{
    acomp_storage_t<ACompTransform>             m_transform;
    acomp_storage_t<ACompTransformControlled>   m_transformControlled;
    acomp_storage_t<ACompTransformMutable>      m_transformMutable;
    acomp_storage_t<ACompFloatingOrigin>        m_floatingOrigin;
    acomp_storage_t<ACompDelete>                m_delete;
    acomp_storage_t<ACompName>                  m_name;
    acomp_storage_t<ACompHierarchy>             m_hierarchy;
    acomp_storage_t<ACompMass>                  m_mass;
    acomp_storage_t<ACompCamera>                m_camera;
};

/**
 * @brief Storage for Physics components from osp/Active/physics.h
 */
struct Physics
{
    acomp_storage_t<ACompPhysBody>              m_physBody;
    acomp_storage_t<ACompPhysDynamic>           m_physDynamic;
    acomp_storage_t<ACompPhysLinearVel>         m_physLinearVel;
    acomp_storage_t<ACompPhysAngularVel>        m_physAngularVel;
    acomp_storage_t<ACompPhysNetForce>          m_physNetForce;
    acomp_storage_t<ACompPhysNetTorque>         m_physNetTorque;
    acomp_storage_t<ACompRigidbodyAncestor>     m_rigidbodyAncestor;
    acomp_storage_t<ACompShape>                 m_shape;
    acomp_storage_t<ACompSolidCollider>         m_solidCollider;
};

/**
 * @brief Storage for Drawing components from osp/Active/drawing.h
 */
struct Drawing
{
    acomp_storage_t<ACompMaterial>              m_material;
    acomp_storage_t<ACompRenderingAgent>        m_renderAgent;
    acomp_storage_t<ACompPerspective3DView>     m_perspective3dView;
    acomp_storage_t<ACompOpaque>                m_opaque;
    acomp_storage_t<ACompTransparent>           m_transparent;
    acomp_storage_t<ACompVisible>               m_visible;
    acomp_storage_t<ACompDrawTransform>         m_drawTransform;
};

/**
 * @brief Storage for Vehicle components from osp/Active/drawing.h
 */
struct Vehicles
{
    acomp_storage_t<ACompMachines>              m_machines;
    acomp_storage_t<ACompVehicle>               m_vehicle;
    acomp_storage_t<ACompVehicleInConstruction> m_vehicleInConstruction;
    acomp_storage_t<ACompPart>                  m_part;
};

/**
 * @brief Storage for wiring and various machine components
 */
struct Machines
{
    mcomp_storage_t<MCompContainer>             m_container;
    mcomp_storage_t<MCompRCSController>         m_rcsController;
    mcomp_storage_t<MCompRocket>                m_rocket;
    mcomp_storage_t<MCompUserControl>           m_userControl;

    ACtxWireNodes<adera::wire::AttitudeControl> m_wireAttitudeControl;
    ACtxWireNodes<adera::wire::Percent>         m_wirePercent;
};

/**
 * @brief Storage needed to synchronize with a Universe
 */
struct UniverseSync
{
    ACtxSyncVehicles    m_syncVehicles;

    ACtxAreaLink        m_areaLink;
};

struct TestApp
{
    acomp_storage_t<ACompCameraController> m_cameraController;
};

}

/**
 * @brief An entire damn flight scene lol
 */
struct FlightScene
{
    scenestate::ActiveIds_t         m_activeIds;
    scenestate::MachineIds_t        m_machineIds;

    scenestate::Basic               m_basic;
    scenestate::Physics             m_physics;
    scenestate::Drawing             m_drawing;
    scenestate::Vehicles            m_vehicles;
    scenestate::Machines            m_machines;

    std::unique_ptr<ospnewton::ACtxNwtWorld>    m_nwtWorld;
};

}
