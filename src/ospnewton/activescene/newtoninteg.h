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

#include "forcefactors.h"

#include <osp/activescene/basic.h>
#include <osp/core/id_map.h>

#include <Newton.h>

#include <longeron/id_management/registry_stl.hpp>
#include <longeron/id_management/id_set_stl.hpp>

#include <entt/core/any.hpp>


namespace ospnewton
{

template<auto FUNC_T>
struct NwtDeleter
{
    void operator() (auto const* pNwtType)
    {
        FUNC_T(pNwtType);
    }
};

using NwtBodyPtr_t = std::unique_ptr< NewtonBody, NwtDeleter<NewtonDestroyBody> >;

using NwtColliderPtr_t = std::unique_ptr<NewtonCollision, NwtDeleter<NewtonDestroyCollision> >;

//using ForceFactor_t = void (*)(
//        ActiveEnt, ViewProjMatrix const&, UserData_t) noexcept;

using BodyId = uint32_t;

using ColliderStorage_t = osp::Storage_t<osp::active::ActiveEnt, NwtColliderPtr_t>;

/**
 * @brief Represents an instance of a Newton physics world in the scane
 */
struct ACtxNwtWorld
{

    struct ForceFactorFunc
    {
        using UserData_t = std::array<void*, 6u>;
        using Func_t = void (*)(NewtonBody const* pBody, BodyId BodyId, ACtxNwtWorld const&, UserData_t, osp::Vector3&, osp::Vector3&) noexcept;

        Func_t      m_func;
        UserData_t  m_userData;
    };

    struct Deleter
    {
        void operator() (NewtonWorld* pNwtWorld) { NewtonDestroy(pNwtWorld); }
    };

    ACtxNwtWorld(int threadCount)
     : m_world(NewtonCreate())
    {
        NewtonWorldSetUserData(m_world.get(), this);
    }

    // note: important that m_nwtBodies and m_nwtColliders are destructed
    //       before m_nwtWorld
    std::unique_ptr<NewtonWorld, Deleter>           m_world;

    lgrn::IdRegistryStl<BodyId>                     m_bodyIds;
    std::vector<NwtBodyPtr_t>                       m_bodyPtrs;
    std::vector<ForceFactors_t>                     m_bodyFactors;
    lgrn::IdSetStl<BodyId>                          m_bodyDirty;

    std::vector<osp::active::ActiveEnt>             m_bodyToEnt;
    osp::IdMap_t<osp::active::ActiveEnt, BodyId>    m_entToBody;

    std::vector<ForceFactorFunc>                    m_factors;

    ColliderStorage_t                               m_colliders;

    osp::active::ACompTransformStorage_t            *m_pTransform;
};


}
