#include "SysAreaAssociate.h"

#include "ActiveScene.h"


using namespace osp::universe;
using namespace osp::active;
using namespace osp;

SysAreaAssociate::SysAreaAssociate(ActiveScene &rScene, Universe &uni) :
       m_scene(rScene),
       m_universe(uni),
       m_updateScan(rScene.get_update_order(), "areascan", "", "",
                    std::bind(&SysAreaAssociate::update_scan, this))
{

}

void SysAreaAssociate::update_scan()
{
    // scan for nearby satellites, maybe move this somewhere else some day

    //Universe& uni = m_universe;
    //std::vector<Satellite>& satellites = uni.
    auto view = m_universe.get_reg().view<UCompActivatable>();

    for (Satellite sat : view)
    {
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

        sat_activate(sat);
    }


//    ACompTransform& cameraTransform = m_scene->reg_get<ACompTransform>(m_camera);
//    //cameraTransform.m_transform[3].xyz().x() += 0.1f; // move the camera right

//    // Temporary: Floating origin follow cameara
//    Magnum::Vector3i tra(cameraTransform.m_transform[3].xyz());

//    m_scene->floating_origin_translate(-Vector3(tra));
//    m_sat->set_position(m_sat->get_position()
//    + Vector3sp({tra.x() * 1024,
//    tra.y() * 1024,
//    tra.z() * 1024}, 10));
    //std::cout << "x: " << Vector3sp(m_sat->get_position()).x() << "\n";

}

void SysAreaAssociate::connect(universe::Satellite sat)
{
    using namespace osp::universe;

    //auto &reg = m_scene.get_registry();

    //UCompActiveArea &areaAA = reg.get<>

    m_areaSat = sat;
}

void SysAreaAssociate::activator_add(universe::ITypeSatellite* type,
                                         IActivator &activator)
{
    m_activators[type] = &activator;
}

int SysAreaAssociate::sat_activate(universe::Satellite sat)
{

    if (m_activatedSats.find(sat) != m_activatedSats.end())
    {
        // satellite already loaded
        return 1;
    }

    auto &ureg = m_universe.get_reg();

    auto &satType = ureg.get<UCompType>(sat);

    //auto &areaSatAA = reg.get<UCompType>(m_areaSat);

    // see if there's a loading function for the satellite object
    auto funcMapIt =  m_activators.find(satType.m_type);

    if(funcMapIt == m_activators.end())
    {
        // no loading function exists for this satellite object type
        return -1;
    }

    //(ActiveScene& scene, osp::universe::SatActiveArea& area,
    //                universe::Satellite areaSat, universe::Satellite loadMe

    // Get activator and call the activate function
    IActivator *activator = funcMapIt->second;
    int loadStatus = activator->activate_sat(m_scene, *this, m_areaSat, sat);

    if (loadStatus)
    {
        // Load failed
        return loadStatus;
    }

    // Load success
    //sat.set_loader(m_sat->get_object());
    m_activatedSats.emplace(sat);

    return 0;
}

int SysAreaAssociate::sat_deactivate(ActiveEnt entity)
{

}

