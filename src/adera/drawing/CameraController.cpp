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
#include "CameraController.h"

#include <Magnum/Magnum.h>

#include <osp/util/logging.h>

// for the 0xrrggbb_rgbf and angle literals
using namespace Magnum::Math::Literals;

using adera::SysCameraController;
using adera::ACtxCameraController;

using Magnum::Rad;

using osp::input::ControlSubscriber;

using osp::Matrix4;
using osp::Quaternion;
using osp::Vector3;

void SysCameraController::update_view(ACtxCameraController& rCtrl, float const delta)
{
    // Process control inputs

    ControlSubscriber const& controls = rCtrl.m_controls;

    Rad yaw = 0.0_degf;
    Rad pitch = 0.0_degf;

    // Arrow key rotation

    Rad const keyRotDelta = 180.0_degf * delta; // 180 degrees per second

    yaw   += (  float(controls.button_held(rCtrl.m_btnRotRt))
              - float(controls.button_held(rCtrl.m_btnRotLf)) ) * keyRotDelta;
    pitch += (  float(controls.button_held(rCtrl.m_btnRotDn))
              - float(controls.button_held(rCtrl.m_btnRotUp)) ) * keyRotDelta;

    // Mouse rotation, if right mouse button is down

    if (rCtrl.m_controls.button_held(rCtrl.m_btnOrbit))
    {
        // 1 degrees per step
        constexpr Rad const mouseRotDelta = 1.0_degf;

        yaw -= controls.get_input_handler()->mouse_state().m_smoothDelta.x()
                * mouseRotDelta;
        pitch -= controls.get_input_handler()->mouse_state().m_smoothDelta.y()
                  * mouseRotDelta;
    }

    auto const scroll = float(rCtrl.m_controls.get_input_handler()
                              ->scroll_state().offset.y());

    Vector3 const up
            = rCtrl.m_up.isZero() ? rCtrl.m_transform.up() : rCtrl.m_up;

    // Prevent pitch overshoot if up is defined
    if ( ! rCtrl.m_up.isZero())
    {
        using Magnum::Math::angle;
        using Magnum::Math::clamp;

        Rad const currentPitch = angle(rCtrl.m_up, -rCtrl.m_transform.backward());
        Rad const nextPitch = currentPitch - pitch;

        // Limit from 1 degree (looking down) to 179 degrees (looking up)
        Rad const clampped  = clamp<Rad>(nextPitch, 1.0_degf, 179.0_degf);
        Rad const overshoot = clampped - nextPitch;

        pitch -= overshoot;
    }

    if (rCtrl.m_target.has_value())
    {
        // Orbit around target

        // Scroll to move in/out
        constexpr float const distSensitivity = 0.3f;
        rCtrl.m_orbitDistance
                -= rCtrl.m_orbitDistance * distSensitivity * scroll;
        rCtrl.m_orbitDistance = std::max(rCtrl.m_orbitDistance, rCtrl.m_orbitDistanceMin);

        // Convert requested rotation to quaternion
        Quaternion const rotationDelta
                = Quaternion::rotation(yaw, up)
                * Quaternion::rotation(pitch, rCtrl.m_transform.right());

        Vector3 const translation
                = rCtrl.m_target.value()
                + rotationDelta.transformVector(
                    rCtrl.m_transform.backward() * rCtrl.m_orbitDistance);

        // look at target
        rCtrl.m_transform = Matrix4::lookAt(translation, rCtrl.m_target.value(), up);
    }
    else
    {
        // TODO: No target, Rotate in place
    }
}

void SysCameraController::update_move(
        ACtxCameraController& rCtrl,
        float const delta, bool const moveTarget)
{
    ControlSubscriber const& controls = rCtrl.m_controls;

    Vector3 const command(
        float(    controls.button_held(rCtrl.m_btnMovRt))
         - float( controls.button_held(rCtrl.m_btnMovLf)),
        float(    controls.button_held(rCtrl.m_btnMovUp))
         - float( controls.button_held(rCtrl.m_btnMovDn)),
        float(    controls.button_held(rCtrl.m_btnMovBk))
         - float( controls.button_held(rCtrl.m_btnMovFd))
    );

    Vector3 const translation
            = (   rCtrl.m_transform.right()    * command.x()
                + rCtrl.m_transform.up()       * command.y()
                + rCtrl.m_transform.backward() * command.z())
            * delta * rCtrl.m_moveSpeed * rCtrl.m_orbitDistance;

    rCtrl.m_transform.translation() += translation;

    if (moveTarget)
    {
        rCtrl.m_target.value() += translation;
    }
}
