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

namespace osp::active
{

struct ACompTransform;

class SysPhysics
{
public:

    enum EIncludeRootMass { Ignore, Include };

    // TODO: rewrite hierarchy inertia calculations for new mass/inertia system

    template<typename IT_T>
    static void update_delete_phys(
            ACtxPhysics &rCtxPhys, IT_T first, IT_T const& last);

    template<typename IT_T>
    static void update_delete_shapes(
            ACtxPhysics &rCtxPhys, IT_T first, IT_T const& last);

    template<typename IT_T>
    static void update_delete_hier_body(
            ACtxHierBody &rCtxHierBody, IT_T first, IT_T const& last);

};

template<typename IT_T>
void SysPhysics::update_delete_phys(
        ACtxPhysics &rCtxPhys, IT_T first, IT_T const& last)
{
    while (first != last)
    {
        ActiveEnt const ent = *first;

        if (rCtxPhys.m_physBody.contains(ent))
        {
            rCtxPhys.m_physBody         .remove(ent);
            rCtxPhys.m_physDynamic      .remove(ent);
            rCtxPhys.m_physLinearVel    .remove(ent);
            rCtxPhys.m_physAngularVel   .remove(ent);
        }

        std::advance(first, 1);
    }
}

template<typename IT_T>
void SysPhysics::update_delete_shapes(
        ACtxPhysics &rCtxPhys, IT_T first, IT_T const& last)
{
    rCtxPhys.m_hasColliders.remove(first, last);

    while (first != last)
    {
        ActiveEnt const ent = *first;

        if (rCtxPhys.m_shape.contains(ent))
        {
            rCtxPhys.m_shape.remove(ent);
            rCtxPhys.m_solid.remove(ent);
        }

        std::advance(first, 1);
    }
}

template<typename IT_T>
void SysPhysics::update_delete_hier_body(
        ACtxHierBody &rCtxHierBody, IT_T first, IT_T const& last)
{
    rCtxHierBody.m_ownDyn.remove(first, last);
    rCtxHierBody.m_totalDyn.remove(first, last);
}


}
