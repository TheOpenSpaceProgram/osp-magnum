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

#include "flight.h"

#include "scene_active.h"

#include <osp/Active/ActiveScene.h>
#include <osp/Active/SysHierarchy.h>
#include <osp/Active/SysRender.h>
#include <osp/Active/SysPhysics.h>
#include <osp/Active/SysVehicle.h>
#include <osp/Active/SysVehicleSync.h>
#include <osp/Active/SysWire.h>
#include <osp/Active/SysForceFields.h>
#include <osp/Active/SysAreaAssociate.h>

#include <osp/CoordinateSpaces/CartesianSimple.h>

#include <osp/logging.h>

#include <adera/Shaders/PlumeShader.h>

#include <osp/Shaders/Phong.h>
#include <osp/Shaders/MeshVisualizer.h>
#include <osp/Shaders/FullscreenTriShader.h>
#include <Magnum/Shaders/MeshVisualizerGL.h>

#include <newtondynamics_physics/SysNewton.h>
#include <newtondynamics_physics/ospnewton.h>

using namespace testapp;

using osp::active::acomp_storage_t;

std::unique_ptr<FlightScene> testapp::setup_flight_scene()
{
    std::unique_ptr<FlightScene> pScene = std::make_unique<FlightScene>();

    pScene->m_nwtWorld = std::make_unique<ospnewton::ACtxNwtWorld>(1);

    return std::move(pScene);
}

void testapp::update_flight_scene(ActiveApplication& rApp, FlightScene &rScene)
{

}

