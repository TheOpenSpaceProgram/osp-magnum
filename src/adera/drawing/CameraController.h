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

#include <osp/activescene/basic.h>
#include <osp/core/math_types.h>
#include <osp/util/UserInputHandler.h>

#include <optional>

namespace adera
{

struct CameraCommands
{
    float           zoom;
    osp::Rad        yaw;
    osp::Rad        pitch;
    osp::Vector3    moveRelative;
};

struct ACtxCameraButtons
{
    using EButtonControlIndex = osp::input::EButtonControlIndex;

    ACtxCameraButtons(osp::input::UserInputHandler &rInput)
     : m_controls(&rInput)
     , m_btnOrbit(m_controls.button_subscribe("cam_orbit"))
     , m_btnRotUp(m_controls.button_subscribe("ui_up"))
     , m_btnRotDn(m_controls.button_subscribe("ui_dn"))
     , m_btnRotLf(m_controls.button_subscribe("ui_lf"))
     , m_btnRotRt(m_controls.button_subscribe("ui_rt"))
     , m_btnMovFd(m_controls.button_subscribe("cam_fd"))
     , m_btnMovBk(m_controls.button_subscribe("cam_bk"))
     , m_btnMovLf(m_controls.button_subscribe("cam_lf"))
     , m_btnMovRt(m_controls.button_subscribe("cam_rt"))
     , m_btnMovUp(m_controls.button_subscribe("cam_up"))
     , m_btnMovDn(m_controls.button_subscribe("cam_dn"))
    { }

    [[nodiscard]] CameraCommands read_button_inputs(float deltaTime) const;

    osp::input::ControlSubscriber m_controls;

    // Camera rotation buttons

    EButtonControlIndex m_btnOrbit;
    EButtonControlIndex m_btnRotUp;
    EButtonControlIndex m_btnRotDn;
    EButtonControlIndex m_btnRotLf;
    EButtonControlIndex m_btnRotRt;

    // Camera movement buttons

    EButtonControlIndex m_btnMovFd;
    EButtonControlIndex m_btnMovBk;
    EButtonControlIndex m_btnMovLf;
    EButtonControlIndex m_btnMovRt;
    EButtonControlIndex m_btnMovUp;
    EButtonControlIndex m_btnMovDn;

}; // struct ACtxCameraButtons

struct ACtxCameraController
{
    void apply(CameraCommands commands);

    void update_transform();

    osp::Matrix4                    m_transform;
    std::optional<osp::Vector3>     m_target            {osp::Vector3{}};

    osp::Quaternion                 m_rot;  ///< derived from up, yaw, and pitch

    osp::Quaternion                 m_refFrameRot; ///<

    //osp::Vector3                    m_up                {0.0f, 1.0f, 0.0f};
    osp::Rad                        m_yaw               {0.0f};
    osp::Rad                        m_pitch             {0.0f}; ///< 0 to pi

    float                           m_orbitDistance     {20.0f};
    float                           m_orbitDistanceMin  {5.0f};
    float                           m_moveSpeed         {1.0f};

}; // struct ACtxCameraController


}
