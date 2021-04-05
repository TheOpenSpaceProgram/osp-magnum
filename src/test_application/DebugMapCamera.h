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

#include <osp/Active/activetypes.h>
#include <osp/Universe.h>
#include <osp/UserInputHandler.h>
#include <osp/types.h>
#include "DebugObject.h"

namespace testapp
{

class DebugMapCameraController : public DebugObject<DebugMapCameraController>
{
public:
    DebugMapCameraController(osp::active::ActiveScene& rScene,
                             osp::active::ActiveEnt ent);
    ~DebugMapCameraController() = default;

    void update();

    bool try_switch_focus();

private:
    osp::universe::Satellite m_selected;
    osp::Vector3 m_orbitPos;
    float m_orbitDistance;

    osp::active::UpdateOrderHandle_t m_update;
    osp::UserInputHandler& m_userInput;
    // Mouse inputs
    osp::MouseMovementHandle m_mouseMotion;
    osp::ScrollInputHandle m_scrollInput;
    osp::ButtonControlHandle m_rmb;
    // Keyboard input
    osp::ButtonControlHandle m_switchNext;
    osp::ButtonControlHandle m_switchPrev;
};

} // namespace testapp
