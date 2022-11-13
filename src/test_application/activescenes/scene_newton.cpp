/**
 * Open Space Program
 * Copyright © 2019-2022 Open Space Program Project
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
#include "scene_newton.h"
#include "identifiers.h"

#include <osp/Active/basic.h>
#include <osp/Active/drawing.h>
#include <osp/Active/physics.h>
#include <osp/Active/SysSceneGraph.h>
#include <osp/Active/SysPrefabInit.h>
#include <osp/Active/SysRender.h>

#include <osp/Resource/ImporterData.h>
#include <osp/Resource/resources.h>
#include <osp/Resource/resourcetypes.h>

#include <newtondynamics_physics/ospnewton.h>
#include <newtondynamics_physics/SysNewton.h>

using namespace osp;
using namespace osp::active;
using osp::restypes::gc_importer;
using osp::phys::EShape;
using ospnewton::ACtxNwtWorld;
using Corrade::Containers::arrayView;

namespace testapp::scenes
{


Session setup_newton_physics(
        Builder_t&                  rBuilder,
        ArrayView<entt::any> const  topData,
        Tags&                       rTags,
        Session const&              scnCommon,
        Session const&              physics)
{
    OSP_SESSION_UNPACK_DATA(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_TAGS(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(physics,    TESTAPP_PHYSICS);
    OSP_SESSION_UNPACK_TAGS(physics,    TESTAPP_PHYSICS);

    Session newton;
    OSP_SESSION_ACQUIRE_DATA(newton, topData, TESTAPP_NEWTON);

    top_emplace< ACtxNwtWorld >(topData, idNwt, 2);

    using ospnewton::SysNewton;

    newton.task() = rBuilder.task().assign({tgSceneEvt, tgSyncEvt, tgPhysReq, tgPhysBodyReq}).data(
            "Update Entities with Newton colliders",
            TopDataIds_t{             idPhys,              idNwt },
            wrap_args([] (ACtxPhysics& rPhys, ACtxNwtWorld& rNwt) noexcept
    {
        //SysNewton::update_colliders(
        //        rPhys, rNwt, std::exchange(rPhysIn.m_colliderDirty, {}));
    }));

    newton.task() = rBuilder.task().assign({tgSceneEvt, tgDelTotalReq, tgPhysBodyDel}).data(
            "Delete Newton components",
            TopDataIds_t{              idNwt,                   idDelTotal},
            wrap_args([] (ACtxNwtWorld& rNwt, EntVector_t const& rDelTotal) noexcept
    {
        SysNewton::update_delete (rNwt, std::cbegin(rDelTotal), std::cend(rDelTotal));
    }));

    newton.task() = rBuilder.task().assign({tgTimeEvt, tgTransformMod, tgHierMod, tgPhysMod}).data(
            "Update Newton world",
            TopDataIds_t{           idBasic,             idPhys,              idNwt,           idDeltaTimeIn },
            wrap_args([] (ACtxBasic& rBasic, ACtxPhysics& rPhys, ACtxNwtWorld& rNwt, float const deltaTimeIn) noexcept
    {
//        auto const physIn = osp::ArrayView<ACtxPhysInputs>(&rPhysIn, 1);
//        SysNewton::update_world(
//                rPhys, rNwt, deltaTimeIn, physIn, rBasic.m_scnGraph,
//                rBasic.m_transform, rBasic.m_transformControlled,
//                rBasic.m_transformMutable);
    }));

    top_emplace< ACtxNwtWorld >(topData, idNwt, 2);

    return newton;
}




} // namespace testapp::scenes


