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

#include <longeron/id_management/registry.hpp>

#include <newtondynamics_physics/ospnewton.h>

namespace testapp::activestate
{

using namespace osp::active;


/**
 * @brief Storage needed to synchronize with a Universe
 */
struct ACtxUniverseSync
{
};

} // namespace testapp::scenestate

namespace testapp::flight
{

/**
 * @brief An entire damn flight scene lol
 */
struct FlightScene
{
    lgrn::IdRegistry<osp::active::ActiveEnt>     m_activeIds;

    osp::active::ACtxBasic          m_basic;
    osp::active::ACtxDrawing        m_drawing;

    osp::active::ACtxPhysics        m_physics;

    std::unique_ptr<ospnewton::ACtxNwtWorld>    m_nwtWorld;
};

} // namespace testapp::flight
