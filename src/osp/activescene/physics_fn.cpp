/**
 * Open Space Program
 * Copyright © 2019-2023 Open Space Program Project
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
#include "physics_fn.h"
#include "basic_fn.h"

using namespace osp;
using namespace osp::active;

void SysPhysics::calculate_subtree_mass_center(
        ACompTransformStorage_t const&          rTf,
        ACtxPhysics&                            rCtxPhys,
        ACtxSceneGraph&                         rScnGraph,
        ActiveEnt                               root,
        Vector3&                                rMassPos,
        float&                                  rTotalMass,
        Matrix4 const&                          currentTf)
{
    for (ActiveEnt const child : SysSceneGraph::children(rScnGraph, root))
    {
        Matrix4 const childTf = currentTf * rTf.get(child).m_transform;

        if (rCtxPhys.m_mass.contains(child))
        {
            ACompMass const& childMass = rCtxPhys.m_mass.get(child);

            Matrix3 inertiaTensor{};
            inertiaTensor[0][0] = childMass.m_inertia.x();
            inertiaTensor[1][1] = childMass.m_inertia.y();
            inertiaTensor[2][2] = childMass.m_inertia.z();

            rTotalMass  += childMass.m_mass;
            rMassPos    += childTf.translation() * childMass.m_mass;
        }

        if (rCtxPhys.m_hasColliders.contains(child))
        {
            calculate_subtree_mass_center(rTf, rCtxPhys, rScnGraph, child, rMassPos, rTotalMass, childTf);
        }
    }
}


void SysPhysics::calculate_subtree_mass_inertia(
        ACompTransformStorage_t const&          rTf,
        ACtxPhysics&                            rCtxPhys,
        ACtxSceneGraph&                         rScnGraph,
        ActiveEnt                               root,
        Matrix3&                                rInertiaTensor,
        Matrix4 const&                          currentTf)
{
    for (ActiveEnt const child : SysSceneGraph::children(rScnGraph, root))
    {
        Matrix4 const childTf = currentTf * rTf.get(child).m_transform;

        if (rCtxPhys.m_mass.contains(child))
        {
            ACompMass const& childMass = rCtxPhys.m_mass.get(child);

            Matrix3 inertiaTensor{};
            inertiaTensor[0][0] = childMass.m_inertia.x();
            inertiaTensor[1][1] = childMass.m_inertia.y();
            inertiaTensor[2][2] = childMass.m_inertia.z();

            Vector3 const offset = childTf.translation() + childMass.m_offset * childTf.scaling();

            rInertiaTensor += transform_inertia_tensor(inertiaTensor, childMass.m_mass, offset, childTf.rotation());
        }

        if (rCtxPhys.m_hasColliders.contains(child))
        {
            calculate_subtree_mass_inertia(rTf, rCtxPhys, rScnGraph, child, rInertiaTensor, childTf);
        }
    }
}
