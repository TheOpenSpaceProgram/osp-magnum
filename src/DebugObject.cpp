#include "DebugObject.h"

#include <osp/Active/ActiveScene.h>
#include <osp/Active/SysVehicle.h>

#include <adera/Machines/UserControl.h>

using adera::active::machines::MachineUserControl;

using namespace osp;

using namespace Magnum::Math::Literals;

//DebugObject::DebugObject(ActiveScene &scene, ActiveEnt ent) :
//    m_scene(scene),
//    m_ent(ent)
//{

//}

//SysDebugObject::SysDebugObject(ActiveScene &scene) :
//        m_scene(scene)
//{

//}


DebugCameraController::DebugCameraController(active::ActiveScene &scene,
                                             active::ActiveEnt ent) :
        DebugObject(scene, ent),
        m_orbiting(entt::null),
        m_orbitPos(0, 0, 1),
        m_updatePhysicsPost(scene.get_update_order(), "dbg_cam", "physics", "",
                std::bind(&DebugCameraController::update_physics_post, this)),
        m_userInput(scene.get_user_input()),
        m_up(m_userInput.config_get("ui_up")),
        m_dn(m_userInput.config_get("ui_dn")),
        m_lf(m_userInput.config_get("ui_lf")),
        m_rt(m_userInput.config_get("ui_rt")),
        m_switch(m_userInput.config_get("game_switch"))
{
    m_orbitDistance = 20.0f;
}

void DebugCameraController::update_physics_post()
{

    bool targetValid = m_scene.get_registry().valid(m_orbiting);

    if (m_switch.triggered())
    {
        std::cout << "switch to new vehicle\n";

        auto view = m_scene.get_registry().view<active::CompVehicle>();
        auto it = view.find(m_orbiting);

        if (targetValid)
        {
            // disable the first MachineUserControl because switching away
            active::ActiveEnt firstPart = view.get(m_orbiting).m_parts[0];
            m_scene.reg_get<MachineUserControl>(firstPart).m_enable = false;
        }

        if (it == view.end() || it == view.begin())
        {
            // no vehicle selected, or last vehicle is selected (loop around)
            m_orbiting = view.back();
        }
        else
        {
            // pick the next vehicle
            m_orbiting = *(--it);
            std::cout << "next\n";
        }

        targetValid = m_scene.get_registry().valid(m_orbiting);

        if (targetValid)
        {
            // enable the first MachineUserControl
            active::ActiveEnt firstPart = view.get(m_orbiting).m_parts[0];

            m_scene.reg_get<MachineUserControl>(firstPart).m_enable = true;
        }

        // pick next entity, or first entity in scene
        //ActiveEnt search = targetValid
        //        ? m_scene.reg_get<ACompHierarchy>(m_orbiting).m_siblingNext
        //        : m_scene.reg_get<ACompHierarchy>(m_scene.hier_get_root())
        //                 .m_childFirst;

        // keep looping until a vehicle is found
        //while ((!m_scene.get_registry().has<CompVehicle>(search)))
        //{
        //    search = m_scene.reg_get<ACompHierarchy>(search).m_siblingNext;
        //}
    }

    if (!targetValid)
    {
        return;
    }

    float yaw =  m_rt.trigger_hold() - m_lf.trigger_hold();
    float pitch = m_dn.trigger_hold() - m_up.trigger_hold();

    // 180 degrees per second
    auto rotDelta = 180.0_degf * m_scene.get_time_delta_fixed();

    Quaternion quatYaw({0, 0.1f * yaw, 0});

    Matrix4 &tf = m_scene.reg_get<active::ACompTransform>(m_ent).m_transform;
    Matrix4 const& tfTgt = m_scene.reg_get<active::ACompTransform>(m_orbiting).m_transform;

    Vector3 posRelative = tf.translation() - tfTgt.translation();

    // set constant distance
    m_orbitPos = m_orbitPos.normalized() * m_orbitDistance;

    // rotate it
    Quaternion mcFish = Quaternion::rotation(yaw * rotDelta, tf.up())
                      * Quaternion::rotation(pitch * rotDelta, tf.right());

    m_orbitPos = mcFish.transformVector(m_orbitPos);

    tf.translation() = tfTgt.translation() + m_orbitPos;

    //tf = tf * Matrix4::translation({0, 0, m_distance})
    //        * Matrix4::rotationY(rotDelta * yaw)
    //        * Matrix4::rotationX(rotDelta * pitch)
    //        * Matrix4::translation({0, 0, -m_distance});

    // look at target
    tf = Matrix4::lookAt(tf.translation(), tfTgt.translation(), tf[1].xyz());
}

void DebugCameraController::view_orbit(active::ActiveEnt ent)
{
    m_orbiting = ent;
}

