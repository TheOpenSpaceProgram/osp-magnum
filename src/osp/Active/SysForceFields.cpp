#include "SysForceFields.h"

#include "ActiveScene.h"

using namespace osp::active;

SysFFGravity::SysFFGravity(ActiveScene &scene) :
        m_scene(scene),
        m_physics(scene.get_system<SysPhysics>()),
        m_updateForce(scene.get_update_order(), "ff_gravity", "", "physics",
                        std::bind(&SysFFGravity::update_force, this))
{

}

void SysFFGravity::update_force()
{

    auto viewFields = m_scene.get_registry()
            .view<ACompFFGravity, ACompTransform>();

    auto viewMasses = m_scene.get_registry()
            .view<ACompRigidBody, ACompTransform>();

    for (ActiveEnt fieldEnt : viewFields)
    {
        auto &fieldFFGrav = viewFields.get<ACompFFGravity>(fieldEnt);
        auto &fieldTransform = viewFields.get<ACompTransform>(fieldEnt);

        for (ActiveEnt massEnt : viewMasses)
        {
            if (fieldEnt == massEnt)
            {
                continue;
            }

            auto &massBody = viewMasses.get<ACompRigidBody>(massEnt);
            auto &massTransform = viewMasses.get<ACompTransform>(massEnt);

            Vector3 relativePos = fieldTransform.m_transform.translation()
                                - massTransform.m_transform.translation();
            float r = relativePos.length();
            Vector3 accel = relativePos * fieldFFGrav.m_Gmass / (r * r * r);
            //               massBody.m_bodyData.m_mass / r;
            // gm1/r = a

            m_physics.body_apply_accel(massBody, accel);
        }
    }
}
