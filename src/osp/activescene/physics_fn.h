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

#include "physics.h"
#include "basic.h"

namespace osp::active
{

struct ACompTransform;

class SysPhysics
{
public:

    static void calculate_subtree_mass_center(
            ACompTransformStorage_t const&          rTf,
            ACtxPhysics&                            rCtxPhys,
            ACtxSceneGraph&                         rScnGraph,
            ActiveEnt                               root,
            Vector3&                                rMassPos,
            float&                                  rTotalMass,
            Matrix4 const&                          currentTf = {});

    static void calculate_subtree_mass_inertia(
            ACompTransformStorage_t const&          rTf,
            ACtxPhysics&                            rCtxPhys,
            ACtxSceneGraph&                         rScnGraph,
            ActiveEnt                               root,
            Matrix3&                                rInertiaTensor,
            Matrix4 const&                          currentTf = {});

    template<typename IT_T, typename ITB_T>
    static void update_delete_phys(ACtxPhysics& rCtxPhys, IT_T const& first, ITB_T const& last);

};

template<typename IT_T, typename ITB_T>
void SysPhysics::update_delete_phys(ACtxPhysics& rCtxPhys, IT_T const& first, ITB_T const& last)
{
    rCtxPhys.m_mass.remove(first, last);
}


}
