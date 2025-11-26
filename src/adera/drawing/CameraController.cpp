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

using Magnum::Rad;
using Magnum::Math::clamp;

using osp::Quaternion;
using osp::Vector3;

adera::CameraCommands adera::ACtxCameraButtons::read_button_inputs(float deltaTime) const
{
    Rad yaw     = 0.0_degf;
    Rad pitch   = 0.0_degf;

    Rad const keyRotDelta = 180.0_degf * deltaTime; // 180 degrees per second

    // Direction buttons for rotation (default: Arrow keys)
    yaw   += (  float(m_controls.button_held(m_btnRotRt))
              - float(m_controls.button_held(m_btnRotLf)) ) * keyRotDelta;
    pitch += (  float(m_controls.button_held(m_btnRotDn))
              - float(m_controls.button_held(m_btnRotUp)) ) * keyRotDelta;

    // Mouse rotation
    if (m_controls.button_held(m_btnOrbit))
    {
        // 1 degrees per step
        constexpr Rad const mouseRotDelta = 1.0_degf;

        yaw   -= m_controls.get_input_handler()->mouse_state().m_smoothDelta.x() * mouseRotDelta;
        pitch -= m_controls.get_input_handler()->mouse_state().m_smoothDelta.y() * mouseRotDelta;
    }

    // Direction buttons for translation (default: WASD)
    auto const moveRelative = deltaTime * Vector3(
        float(m_controls.button_held(m_btnMovRt)) - float(m_controls.button_held(m_btnMovLf)),
        float(m_controls.button_held(m_btnMovUp)) - float(m_controls.button_held(m_btnMovDn)),
        float(m_controls.button_held(m_btnMovBk)) - float(m_controls.button_held(m_btnMovFd))
    );

    constexpr float zoomSensitivity = 0.3f;
    auto const scroll = float(m_controls.get_input_handler()->scroll_state().offset.y());

    return CameraCommands{
        .zoom   = -scroll * zoomSensitivity,
        .yaw    = yaw,
        .pitch  = pitch,
        .moveRelative = moveRelative
    };
}


void adera::ACtxCameraController::apply(adera::CameraCommands commands)
{
    m_yaw   = m_yaw + commands.yaw;
    m_pitch = clamp<Rad>(m_pitch + commands.pitch, 0.0_degf, 180.0_degf);

    m_orbitDistance = std::max(m_orbitDistance + m_orbitDistance * commands.zoom, m_orbitDistanceMin);

    //Vector3 const up = m_up.isZero() ? m_transform.up() : m_up;

    m_rot = m_refFrameRot * Quaternion::rotation(m_yaw, Vector3{0.0f, 0.0f, 1.0f}) * Quaternion::rotation(m_pitch, Vector3{1.0f, 0.0f, 0.0f});

    if (m_target.has_value())
    {
        // "* m_orbitDistance" to move faster when zoomed out
        m_target.value() += m_rot.transformVector(commands.moveRelative) * m_orbitDistance;
    }
};


void adera::ACtxCameraController::update_transform()
{
    Magnum::Matrix3x3 const rotMatrix = m_rot.toMatrix();

    Vector3 const pos = m_target.has_value()
                      ? (m_target.value() + rotMatrix[2] * m_orbitDistance)
                      : m_transform[3].xyz();

    m_transform = osp::Matrix4::from(rotMatrix, pos);
};

