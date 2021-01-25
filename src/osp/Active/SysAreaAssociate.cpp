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

using osp::universe::TypeSatIndex;
using osp::universe::Universe;
using osp::universe::Satellite;

using osp::universe::UCompType;
using osp::universe::UCompActivatable;
using osp::universe::UCompActivationRadius;
using osp::universe::UCompActiveArea;

const std::string SysAreaAssociate::smc_name = "AreaAssociate";

SysAreaAssociate::SysAreaAssociate(ActiveScene &rScene)
 : m_updateScan(rScene.get_update_order(), "areascan", "physics", "",
                &SysAreaAssociate::update_scan)
{

}

void SysAreaAssociate::update_scan(ActiveScene& rScene)
{
    // scan for nearby satellites, maybe move this somewhere else some day

    ACompAreaLink *pArea = try_get_area_link(rScene);

    if (pArea == nullptr)
    {
        return;
    }

    // Clear enter/leave vectors
    for (ActivationTracker &tracker : pArea->m_activated)
    {
        tracker.m_enter.clear();
        tracker.m_leave.clear();
    }

    Universe &rUni = pArea->m_rUniverse;
    Satellite areaSat = pArea->m_areaSat;

    auto &areaUComp = rUni.get_reg().get<UCompActiveArea>(areaSat);

    auto viewActRadius = rUni.get_reg().view<
            UCompActivationRadius, UCompActivatable, UCompType>();

    for (Satellite sat : viewActRadius)
    {
        auto satRadius = viewActRadius.get<UCompActivationRadius>(sat);
        auto satType = viewActRadius.get<UCompType>(sat);

        if (satType.m_type == TypeSatIndex::Invalid)
        {
            continue; // Skip satellites that don't have a type (somehow)
        }

        ActivationTracker &activations = pArea->m_activated[
                                                    size_t(satType.m_type)];

        // Check if already activated
        auto found = activations.m_inside.find(sat);
        bool alreadyInside = (found != activations.m_inside.end());

        float distSqr = rUni.sat_calc_pos_meters(areaSat, sat).dot();
        float radius = areaUComp.m_areaRadius + satRadius.m_radius;

        if ((radius * radius) > distSqr)
        {
            if (alreadyInside)
            {
                continue; // Satellite already activated
            }
            // Satellite is nearby, activate it!
            auto entered = activations.m_inside.emplace(sat, entt::null).first;
            activations.m_enter.emplace_back(entered);
        }
        else if (alreadyInside)
        {
            // Satellite exited area
            activations.m_leave.emplace_back(found->first, found->second);
            activations.m_inside.erase(found);
        }
    }
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
    assert(!rScene.get_registry().has<ACompAreaLink>(root));

    // Create Area Link
    rScene.get_registry().emplace<ACompAreaLink>(root, rUni, areaSat);

}

void SysAreaAssociate::disconnect(ActiveScene& rScene)
{
//    if (!m_activatedSats.empty())
//    {
//        // De-activate all satellites

//        auto viewAct = m_scene.get_registry().view<ACompActivatedSat>();

//        for (ActiveEnt ent : viewAct)
//        {
//            sat_deactivate(ent, viewAct.get(ent));
//        }

//    }

//    m_areaSat = entt::null;
}

void SysAreaAssociate::area_move(ActiveScene& rScene, Vector3s translate)
{
    ACompAreaLink *pArea = try_get_area_link(rScene);

    if (pArea == nullptr)
    {
        return;
    }

    auto &areaPosTraj = pArea->get_universe().get_reg()
            .get<universe::UCompTransformTraj>(pArea->m_areaSat);

    areaPosTraj.m_position += translate;

    Vector3 meters = Vector3(translate) / gc_units_per_meter;

    floating_origin_translate(rScene, -meters);
}

void SysAreaAssociate::sat_transform_update(
        universe::Universe& rUni, universe::Satellite areaSat,
        universe::Satellite tgtSat, Matrix4 transform)
{
    auto &satPosTraj = rUni.get_reg().get<universe::UCompTransformTraj>(tgtSat);

    auto const &areaPosTraj = rUni.get_reg()
            .get<universe::UCompTransformTraj>(areaSat);

    // 1024 units = 1 meter
    Vector3s posAreaRelative(transform.translation() * gc_units_per_meter);

    satPosTraj.m_position = areaPosTraj.m_position + posAreaRelative;
    satPosTraj.m_rotation = Quaternion::
            fromMatrix(transform.rotationScaling());
    satPosTraj.m_dirty = true;
}

void SysAreaAssociate::floating_origin_translate(ActiveScene& rScene, Vector3 translation)
{
    auto view = rScene.get_registry()
            .view<ACompFloatingOrigin, ACompTransform>();

    for (ActiveEnt ent : view)
    {
        auto &entTransform = view.get<ACompTransform>(ent);
        //auto &entFloatingOrigin = view.get<ACompFloatingOrigin>(ent);

        if (entTransform.m_controlled)
        {
            if (!entTransform.m_mutable)
            {
                continue;
            }

            entTransform.m_transformDirty = true;
        }

        entTransform.m_transform.translation() += translation;
    }
}


