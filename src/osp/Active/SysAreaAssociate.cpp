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
#include "SysAreaAssociate.h"

#include "ActiveScene.h"

using osp::active::SysAreaAssociate;

using osp::active::ACompAreaLink;

using osp::universe::Universe;
using osp::universe::Satellite;

using osp::universe::UCompActivatable;
using osp::universe::UCompActivationRadius;
using osp::universe::UCompActiveArea;

using osp::universe::Vector3g;

void SysAreaAssociate::update_clear(ActiveScene& rScene)
{
    ACompAreaLink *pArea = try_get_area_link(rScene);

    if (pArea == nullptr)
    {
        return;
    }

    Universe &rUni = pArea->m_rUniverse;
    Satellite areaSat = pArea->m_areaSat;
    auto &areaUComp = rUni.get_reg().get<UCompActiveArea>(areaSat);

    // Clear enter/leave event vectors

    areaUComp.m_enter.clear();
    areaUComp.m_leave.clear();
}

ACompAreaLink* SysAreaAssociate::try_get_area_link(ActiveScene &rScene)
{
    return rScene.get_registry().try_get<ACompAreaLink>(rScene.hier_get_root());
}

void SysAreaAssociate::connect(ActiveScene& rScene, universe::Universe &rUni,
                               universe::Satellite areaSat)
{
    ActiveEnt root = rScene.hier_get_root();

    // Make sure no ACompAreaLink already exists
    assert(!rScene.get_registry().all_of<ACompAreaLink>(root));

    // Create Area Link
    rScene.get_registry().emplace<ACompAreaLink>(root, rUni, areaSat);

}

void SysAreaAssociate::disconnect(ActiveScene& rScene)
{
    // TODO: Have a vector of function pointers for each satellite type, that
    //       are called to de-activate them right away.
    //       Also delete the ACompAreaLink from the scene
}

void SysAreaAssociate::area_move(ActiveScene& rScene, Vector3g translate)
{
    ACompAreaLink *pArea = try_get_area_link(rScene);

    if (pArea == nullptr)
    {
        return;
    }

    auto &areaPosTraj = pArea->get_universe().get_reg()
            .get<universe::UCompTransformTraj>(pArea->m_areaSat);

//    areaPosTraj.m_position += translate;

    Vector3 meters = Vector3(translate) / universe::gc_units_per_meter;

    floating_origin_translate(rScene, -meters);
}

void SysAreaAssociate::sat_transform_set_relative(
        universe::Universe& rUni, universe::Satellite relativeSat,
        universe::Satellite tgtSat, Matrix4 transform)
{
    auto &satPosTraj = rUni.get_reg().get<universe::UCompTransformTraj>(tgtSat);

    auto const &areaPosTraj = rUni.get_reg()
            .get<universe::UCompTransformTraj>(relativeSat);

    // 1024 units = 1 meter
    Vector3g const posAreaRelative(transform.translation()
                                   * universe::gc_units_per_meter);

//    satPosTraj.m_position = areaPosTraj.m_position + posAreaRelative;
//    satPosTraj.m_rotation = Quaternion::
//            fromMatrix(transform.rotationScaling());
//    satPosTraj.m_dirty = true;
}

void SysAreaAssociate::floating_origin_translate(
        ActiveScene& rScene,Vector3 translation)
{
    auto &rReg = rScene.get_registry();

    auto view = rReg.view<ACompFloatingOrigin, ACompTransform>();

    for (ActiveEnt const ent : view)
    {
        auto &entTransform = view.get<ACompTransform>(ent);
        //auto &entFloatingOrigin = view.get<ACompFloatingOrigin>(ent);

        if (rReg.all_of<ACompTransformControlled>(ent))
        {
            auto *tfMutable = rReg.try_get<ACompTransformMutable>(ent);
            if (tfMutable == nullptr)
            {
                continue;
            }

            tfMutable->m_dirty = true;
        }

        entTransform.m_transform.translation() += translation;
    }
}


