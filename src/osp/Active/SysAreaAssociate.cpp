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

    Universe &rUni = pArea->m_rUniverse;
    Satellite areaSat = pArea->m_areaSat;

    auto &areaUComp = rUni.get_reg().get<UCompActiveArea>(areaSat);

    auto viewActRadius = rUni.get_reg().view<UCompActivationRadius,
                                             UCompActivatable>();

    for (Satellite sat : viewActRadius)
    {
        auto satRadius = viewActRadius.get<UCompActivationRadius>(sat);

        float distSqr = rUni.sat_calc_pos_meters(areaSat, sat).dot();
        float radius = areaUComp.m_areaRadius + satRadius.m_radius;

        if ((radius * radius) > distSqr)
        {
            std::cout << "nearby!\n";
        }

    }

    //for (Satellite sat : view)
    //{
//        if (SatelliteObject* obj = sat.get_object())
//        {
//            //std::cout << "obj: " << obj->get_id().m_name
//            //          << ", " << (void*)(&(obj->get_id())) << "\n";
//        }
//        else
//        {
//            // Satellite has no object, it's empty
//            continue;
//        }

//        // if satellite is not loadable or is already activated
//        if (!sat.is_activatable() || sat.get_loader())
//        {
//            //std::cout << "already loaded\n";
//            continue;
//        }


        // ignore self (unreachable as active areas are unloadable)
        //if (&sat == m_sat)
        //{
        //    continue;
        //}

        // Calculate distance to satellite
        // For these long vectors, 1024 units = 1 meter
        //Vector3s relativePos = m_universe.sat_calc_position(m_areaSat, sat);
        //Vector3 relativePosMeters = Vector3(relativePos) / 1024.0f;

        //using Magnum::Math::pow;

        // ignore if too far
        //if (relativePos.dot() > pow(sat.get_load_radius()
        //    + m_sat->get_load_radius(), 2.0f))
        //{
        //    continue;
        //}

        //std::cout << "is near! dist: " << relativePos.length() << "/"
        //<< (sat.get_load_radius() + m_sat->get_load_radius()) << "\n";

        // Satellite is near! Attempt to load it

        //sat_activate(sat);
    //}

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
    auto &areaLink = rScene.get_registry().emplace<ACompAreaLink>(root, rUni,
                                                                  areaSat);
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

void SysAreaAssociate::area_move(Vector3s translate)
{
//    auto &areaPosTraj = m_universe.get_reg()
//            .get<universe::UCompTransformTraj>(m_areaSat);

//    areaPosTraj.m_position += translate;

//    Vector3 meters = Vector3(translate) / gc_units_per_meter;

//    floating_origin_translate(-meters);
}

void SysAreaAssociate::sat_transform_update(ActiveEnt ent)
{
//    auto const &entAct = m_scene.reg_get<ACompActivatedSat>(ent);
//    auto const &entTransform = m_scene.reg_get<ACompTransform>(ent);

//    Satellite sat = entAct.m_sat;
//    auto &satPosTraj = m_universe.get_reg()
//            .get<universe::UCompTransformTraj>(sat);

//    auto const &areaPosTraj = m_universe.get_reg()
//            .get<universe::UCompTransformTraj>(m_areaSat);

//    // 1024 units = 1 meter
//    Vector3s posAreaRelative(entTransform.m_transform.translation()
//                             * gc_units_per_meter);



//    satPosTraj.m_position = areaPosTraj.m_position + posAreaRelative;
//    satPosTraj.m_rotation = Quaternion::
//            fromMatrix(entTransform.m_transform.rotationScaling());
//    satPosTraj.m_dirty = true;
}

void SysAreaAssociate::floating_origin_translate(Vector3 translation)
{
//    auto view = m_scene.get_registry()
//            .view<ACompFloatingOrigin, ACompTransform>();

//    for (ActiveEnt ent : view)
//    {
//        auto &entTransform = view.get<ACompTransform>(ent);
//        //auto &entFloatingOrigin = view.get<ACompFloatingOrigin>(ent);

//        if (entTransform.m_controlled)
//        {
//            if (!entTransform.m_mutable)
//            {
//                continue;
//            }

//            entTransform.m_transformDirty = true;
//        }

//        entTransform.m_transform.translation() += translation;
//    }
}


