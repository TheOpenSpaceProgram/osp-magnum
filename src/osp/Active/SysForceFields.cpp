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
#include "SysForceFields.h"

#include "ActiveScene.h"

using namespace osp::active;

const std::string SysFFGravity::smc_name = "FFGravity";

SysFFGravity::SysFFGravity(ActiveScene &scene) :
        m_scene(scene),
        m_physics(scene.dynamic_system_find<SysPhysics>()),
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
