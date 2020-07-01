#include "DebugObject.h"

#include "ActiveScene.h"

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


DebugCameraController::DebugCameraController(ActiveScene &scene, ActiveEnt ent) :
        DebugObject(scene, ent),
        m_tracking(entt::null),
        m_updatePhysicsPost(scene.get_update_order(), "dbg_cam", "", "physics",
                std::bind(&DebugCameraController::update_physics_post, this)),
        m_userInput(scene.get_user_input()),
        m_up(m_userInput.config_get("ui_up")),
        m_dn(m_userInput.config_get("ui_dn")),
        m_lf(m_userInput.config_get("ui_lf")),
        m_rt(m_userInput.config_get("ui_rt"))
{
    m_distance = 20.0f;
}

void DebugCameraController::update_physics_post()
{
    float yaw = m_lf.trigger_hold() - m_rt.trigger_hold();
    float pitch = m_up.trigger_hold() - m_dn.trigger_hold();

    // 180 degrees per second
    auto rotDelta = 180.0_degf * m_scene.get_time_delta_fixed();

    //Quaternion quatYaw({0, 0.1f * yaw, 0});

    Matrix4 &tf = m_scene.reg_get<CompTransform>(m_ent).m_transform;
    Matrix4 const& tfTgt = m_scene.reg_get<CompTransform>(m_tracking).m_transform;

    // set constant distance
    tf.translation() = tfTgt.translation() + (tf.translation() - tfTgt.translation()).normalized() * m_distance;

    // rotate it
    tf = tf * Matrix4::translation({0, 0, m_distance})
            * Matrix4::rotationY(rotDelta * yaw)
            * Matrix4::rotationX(rotDelta * pitch)
            * Matrix4::translation({0, 0, -m_distance});

    // look at target
    tf = Matrix4::lookAt(tf.translation(), tfTgt.translation(), tf[1].xyz());
}

void DebugCameraController::view_track(ActiveEnt ent)
{
    m_tracking = ent;
}
