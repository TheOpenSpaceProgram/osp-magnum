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
#include "DebugMapCamera.h"
#include <osp/Active/ActiveScene.h>
#include <osp/Universe.h>
#include <adera/SysMap.h>
#include <Magnum/Math/Quaternion.h>

using namespace testapp;
using osp::active::ActiveEnt;
using osp::active::ActiveScene;
// for the 0xrrggbb_rgbf and angle literals
using namespace Magnum::Math::Literals;

DebugMapCameraController::DebugMapCameraController(ActiveScene& rScene, ActiveEnt ent)
    : DebugObject(rScene, ent)
    , m_selected(entt::null)
    , m_orbitPos(0.0, 0.0, 1.0)
    , m_orbitDistance(20.0f)
    , m_userInput(rScene.get_user_input())
    , m_mouseMotion(m_userInput.mouse_get())
    , m_scrollInput(m_userInput.scroll_get())
    , m_rmb(m_userInput.config_get("ui_rmb"))
    , m_switchNext(m_userInput.config_get("game_switch_next"))
    , m_switchPrev(m_userInput.config_get("game_switch_prev"))
    , m_update(rScene.get_update_order(),
        "dbg_map_camera", "", "physics", [this](ActiveScene&) { this->update(); })
{}

void DebugMapCameraController::update()
{

    auto& rUniReg = m_scene.get_application().get_universe().get_reg();
    bool targetValid = rUniReg.valid(m_selected);

    if (!try_switch_focus()) { return; }

    // Camera controls
    using osp::Matrix4;
    using osp::Vector3s;
    using osp::Vector3;
    using osp::active::ACompTransform;
    using adera::active::SysMap;
    using osp::Quaternion;
    using osp::universe::UCompTransformTraj;

    Matrix4 &xform = m_scene.reg_get<ACompTransform>(m_ent).m_transform;
    Vector3s const& v3sPos = rUniReg.get<UCompTransformTraj>(m_selected).m_position;
    Vector3 tgtPos = SysMap::universe_to_render_space(v3sPos);

    Quaternion mcFish(Magnum::Math::IdentityInit);
    if (m_rmb.trigger_hold())
    {
        // 180 degrees per second
        auto keyRotDelta = 180.0_degf * m_scene.get_time_delta_fixed();

        float yaw = 0.0f;
        float pitch = 0.0f;
        if (m_rmb.trigger_hold())
        {
            yaw += static_cast<float>(-m_mouseMotion.dxSmooth());
            pitch += static_cast<float>(-m_mouseMotion.dySmooth());
        }

        // 100 degrees per step
        constexpr auto rotRate = 1.0_degf;

        // rotate it
        mcFish = Quaternion::rotation(yaw * rotRate, xform.up())
            * Quaternion::rotation(pitch * rotRate, xform.right());
    }

    Vector3 posRelative = xform.translation() - tgtPos;

    // set camera orbit distance
    constexpr float distSensitivity = 5e4f;
    m_orbitDistance += distSensitivity * static_cast<float>(-m_scrollInput.dy());

    // Clamp orbit distance to avoid producing a degenerate m_orbitPos vector
    constexpr float minDist = 2000.0f;
    if (m_orbitDistance < minDist) { m_orbitDistance = minDist; }

    m_orbitPos = m_orbitPos.normalized() * m_orbitDistance;
    m_orbitPos = mcFish.transformVector(m_orbitPos);

    xform.translation() = tgtPos + m_orbitPos;

    // look at target
    xform = Matrix4::lookAt(xform.translation(), tgtPos, xform[1].xyz());
}

bool DebugMapCameraController::try_switch_focus()
{
    using osp::universe::UCompTransformTraj;

    auto& rUni = m_scene.get_application().get_universe();
    auto& rUniReg = rUni.get_reg();

    if (m_switchNext.triggered() || m_switchPrev.triggered())
    {
        auto satView = rUniReg.view<UCompTransformTraj>();
        auto itr = satView.find(m_selected);

        if (m_switchNext.triggered())
        {
            if (itr == satView.end() || itr == satView.begin())
            {
                // None selected, or at end: loop back to start
                m_selected = satView.back();
            }
            else
            {
                // Select next
                m_selected = *(--itr);
            }
        }
        else if (m_switchPrev.triggered())
        {
            if (itr == satView.end() || itr == --(satView.end()))
            {
                // None selected, or at front: wrap to end
                m_selected = satView.front();
            }
            else
            {
                // Select previous
                m_selected = *(++itr);
            }
        }

        auto& focus = rUniReg.get<adera::active::ACompMapFocus>(rUni.sat_root());
        focus.m_sat = m_selected;
        focus.m_dirty = true;
    }

    return rUniReg.valid(m_selected);
}
