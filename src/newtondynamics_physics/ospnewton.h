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

#include <osp/Active/activetypes.h>
#include <osp/Active/SysPhysics.h>

#include <Newton.h>

namespace ospnewton
{

struct DeleterNewtonBody
{
    void operator() (NewtonBody const* pCollision)
    { NewtonDestroyBody(pCollision); }
};

using ACompNwtBody_t = std::unique_ptr<NewtonBody const, DeleterNewtonBody>;

struct DeleterNewtonCollision
{
    void operator() (NewtonCollision const* pCollision)
    { NewtonDestroyCollision(pCollision); }
};

using ACompNwtCollider_t = std::unique_ptr<NewtonCollision const, DeleterNewtonCollision>;

/**
 * @brief Represents an instance of a Newton physics world in the scane
 */
struct ACtxNwtWorld
{

    // TODO: not compiling, std::hardware_destructive_interference_size not found?
    struct alignas(64) PerThread
    {
        using SetTfPair_t = std::pair<osp::active::ActiveEnt,
                                      NewtonBody const* const>;

        // transformations to set recorded in cb_set_transform
        std::vector<SetTfPair_t> m_setTf;
    };

    struct Deleter
    {
        void operator() (NewtonWorld* pNwtWorld) { NewtonDestroy(pNwtWorld); }
    };

    ACtxNwtWorld(int threadCount)
     : m_nwtWorld(NewtonCreate())
     , m_perThread(threadCount)
    {
        NewtonWorldSetUserData(m_nwtWorld.get(), this);
    }

    osp::active::ActiveScene *m_pScene;

    std::unique_ptr<NewtonWorld, Deleter> m_nwtWorld;

    osp::active::acomp_storage_t<ACompNwtBody_t> m_nwtBodies;
    osp::active::acomp_storage_t<ACompNwtCollider_t> m_nwtColliders;

    entt::basic_view<osp::active::ActiveEnt, entt::exclude_t<>,
                     osp::active::ACompPhysNetForce> m_viewForce;
    entt::basic_view<osp::active::ActiveEnt, entt::exclude_t<>,
                     osp::active::ACompPhysNetTorque> m_viewTorque;

    std::vector<PerThread> m_perThread;
};




}
