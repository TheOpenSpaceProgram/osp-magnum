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

using namespace osp::active;


void SysFFGravity::update_force(
        acomp_view_t<ACompFFGravity const> viewGrav,
        acomp_view_t<ACompTransform const> viewTf,
        acomp_view_t<ACompPhysDynamic const> viewDyn,
        acomp_storage_t<ACompPhysNetForce>& rNetForce)
{

    auto const viewFields = viewGrav | viewTf;
    auto const viewMasses = viewDyn | viewTf;

    for (ActiveEnt fieldEnt : viewFields)
    {
        auto const &fieldFFGrav = viewFields.get<ACompFFGravity const>(fieldEnt);
        auto const &fieldTransform = viewFields.get<ACompTransform const>(fieldEnt);

        for (ActiveEnt massEnt : viewMasses)
        {
            if (fieldEnt == massEnt)
            {
                continue;
            }

            auto const &massBody = viewMasses.get<const ACompPhysDynamic>(massEnt);
            auto const &massTransform = viewMasses.get<const ACompTransform>(massEnt);

            Vector3 const relativePos = fieldTransform.m_transform.translation()
                                      - massTransform.m_transform.translation();
            float const r = relativePos.length();
            Vector3 const force = (relativePos * fieldFFGrav.m_Gmass
                                               * massBody.m_totalMass)
                                   / (r * r * r);


            ACompPhysNetForce &rEntNetForce = rNetForce.contains(massEnt)
                                            ? rNetForce.get(massEnt)
                                            : rNetForce.emplace(massEnt);
            rEntNetForce += force;

        }
    }
}
