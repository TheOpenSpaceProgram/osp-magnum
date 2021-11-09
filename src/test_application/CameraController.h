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

namespace testapp
{

struct ACompCameraController
{
    using EButtonControlIndex = osp::input::EButtonControlIndex;

    ACompCameraController(osp::input::UserInputHandler &rInput)
     : m_controls(&rInput)
     , m_rmb(           m_controls.button_subscribe("ui_rmb"))
     , m_up(            m_controls.button_subscribe("ui_up"))
     , m_dn(            m_controls.button_subscribe("ui_dn"))
     , m_lf(            m_controls.button_subscribe("ui_lf"))
     , m_rt(            m_controls.button_subscribe("ui_rt"))
     , m_switch(        m_controls.button_subscribe("game_switch"))
     , m_throttleMax(   m_controls.button_subscribe("vehicle_thr_max"))
     , m_throttleMin(   m_controls.button_subscribe("vehicle_thr_min"))
     , m_throttleMore(  m_controls.button_subscribe("vehicle_thr_more"))
     , m_throttleLess(  m_controls.button_subscribe("vehicle_thr_less"))
     , m_selfDestruct(  m_controls.button_subscribe("vehicle_self_destruct"))
     , m_pitchUp(       m_controls.button_subscribe("vehicle_pitch_up"))
     , m_pitchDn(       m_controls.button_subscribe("vehicle_pitch_dn"))
     , m_yawLf(         m_controls.button_subscribe("vehicle_yaw_lf"))
     , m_yawRt(         m_controls.button_subscribe("vehicle_yaw_rt"))
     , m_rollLf(        m_controls.button_subscribe("vehicle_roll_lf"))
     , m_rollRt(        m_controls.button_subscribe("vehicle_roll_rt"))
    { }

    osp::universe::Satellite m_selected{entt::null};
    osp::Vector3 m_orbitPos{0.0f, 0.0f, 1.0f};
    float m_orbitDistance{20.0f};

    // Max distance from the origin to trigger a floating origin translation
    int m_originDistanceThreshold{256};

    // When switching vehicle, Move 20% closer to the new vehicle each frame
    float m_travelSpeed{0.2f};

    // Throttle change per second for incremental throttle controls
    float m_throttleRate{0.5f};

    osp::input::ControlSubscriber m_controls;

    // Mouse inputs
    EButtonControlIndex m_rmb;

    // Camera button controls

    EButtonControlIndex m_up;
    EButtonControlIndex m_dn;
    EButtonControlIndex m_lf;
    EButtonControlIndex m_rt;
    EButtonControlIndex m_switch;

    // Vehicle button controls

    EButtonControlIndex m_throttleMax;
    EButtonControlIndex m_throttleMin;
    EButtonControlIndex m_throttleMore;
    EButtonControlIndex m_throttleLess;

    EButtonControlIndex m_selfDestruct;

    EButtonControlIndex m_pitchUp;
    EButtonControlIndex m_pitchDn;
    EButtonControlIndex m_yawLf;
    EButtonControlIndex m_yawRt;
    EButtonControlIndex m_rollLf;
    EButtonControlIndex m_rollRt;
};

class SysCameraController
{
public:

    static std::pair<osp::active::ActiveEnt, ACompCameraController&>
    get_camera_controller(osp::active::ActiveScene& rScene);

    static bool try_switch_vehicle(osp::active::ActiveScene& rScene,
                                   ACompCameraController& rCamCtrl);

    /**
     * @brief Update that deals with modifying the vehicle
     *
     * @param rScene [ref] Scene with root containing ACompCameraController
     */
    static void update_vehicle(osp::active::ActiveScene& rScene);

    /**
     * @brief Read user inputs, and write controls to MCompUserControl
     *
     * @param rScene [ref] Scene with root containing ACompCameraController
     */
    static void update_controls(osp::active::ActiveScene& rScene);

    /**
     * @brief Move the scene origin and ActiveArea to follow the target vehicle
     *
     * @param rScene [ref] Scene with root containing ACompCameraController
     */
    static void update_area(osp::active::ActiveScene& rScene);

    /**
     * @brief Deal with positioning and controlling the camera
     *
     * @param rScene [ref] Scene with root containing ACompCameraController
     */
    static void update_view(osp::active::ActiveScene& rScene);

    static osp::active::ActiveEnt find_vehicle_from_sat(
            osp::active::ActiveScene& rScene,
            osp::universe::Satellite sat);

};

}
