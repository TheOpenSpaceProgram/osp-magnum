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

#include <newtondynamics_physics/ospnewton.h>

namespace testapp::activestate
{

using namespace osp::active;
using namespace adera::active::machines;

/**
 * @brief Storage for wiring and various machine components
 */
struct ACtxMachines
{
    acomp_storage_t<ACompMachines>              m_machines;

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
struct ACtxUniverseSync
{
    ACtxSyncVehicles    m_syncVehicles;
    ACtxAreaLink        m_areaLink;
};

} // namespace testapp::scenestate

namespace testapp::flight
{

/**
 * @brief An entire damn flight scene lol
 */
struct FlightScene
{
    osp::IdRegistry<osp::active::ActiveEnt>     m_activeIds;
    osp::IdRegistry<osp::active::MachineEnt>    m_machineIds;

    osp::active::ACtxBasic          m_basic;
    osp::active::ACtxDrawing        m_drawing;
    activestate::ACtxMachines       m_machines;

    osp::active::ACtxPhysics        m_physics;
    activestate::ACtxVehicle        m_vehicles;

    std::unique_ptr<ospnewton::ACtxNwtWorld>    m_nwtWorld;
};

} // namespace testapp::flight
