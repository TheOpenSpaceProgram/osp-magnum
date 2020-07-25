#include <iostream>

#include <Magnum/GL/DefaultFramebuffer.h>

#include "SatActiveArea.h"

#include "../Active/ActiveScene.h"
#include "../Active/SysNewton.h"
#include "../Active/SysMachine.h"

#include "../Universe.h"


// for the 0xrrggbb_rgbf and angle literals
using namespace Magnum::Math::Literals;

using Magnum::Matrix4;

namespace osp
{



SatActiveArea::SatActiveArea(UserInputHandler &userInput) :
        SatelliteObject(),
        m_loadedActive(false),
        m_userInput(userInput)
{

}

SatActiveArea::~SatActiveArea()
{
    // Unload everything
    for (SatelliteObject* satObj : m_activatedSats)
    {
        satObj->get_satellite()->set_loader(nullptr);
    }

    std::cout << "Active Area destroyed\n";

}


int SatActiveArea::activate(OSPApplication& app)
{
    if (m_loadedActive)
    {
        return -1;
    }

    // lazy way to set name
    m_sat->set_name("ActiveArea");

    // when switched to. Active areas can only be activated by the universe.
    // why, and how would an active area load another active area?
    m_loadedActive = true;

    std::cout << "Active Area loading...\n";

    // Create the scene
    m_scene = std::make_shared<ActiveScene>(m_userInput, app);

    // Create the camera entity
    m_camera = m_scene->hier_create_child(m_scene->hier_get_root(), "Camera");
    CompTransform& cameraTransform = m_scene->reg_emplace<CompTransform>(m_camera);


    CompCamera& cameraComp
            = m_scene->get_registry().emplace<CompCamera>(m_camera);

    cameraTransform.m_transform = Matrix4::translation(Vector3(0, 0, 25));
    cameraTransform.m_enableFloatingOrigin = true;

    cameraComp.m_viewport = Vector2(Magnum::GL::defaultFramebuffer.viewport()
                                                                  .size());
    cameraComp.m_far = 4096.0f;
    cameraComp.m_near = 0.125f;
    cameraComp.m_fov = 45.0_degf;

    cameraComp.calculate_projection();

    return 0;
}


void SatActiveArea::draw_gl()
{
    m_scene->update_hierarchy_transforms();
    m_scene->draw(m_camera);
}

void SatActiveArea::activate_func_add(Id const* id, LoadStrategy function)
{
    m_loadFunctions[id] = function;
}


int SatActiveArea::activate_satellite(Satellite& sat)
{
    // see if there's a loading function for the satellite object
    auto funcMapIt =
            m_loadFunctions.find(&(sat.get_object()->get_id()));

    if(funcMapIt == m_loadFunctions.end())
    {
        // no loading function exists for this satellite object type
        return -1;
    }

    int loadStatus = funcMapIt->second(* this, *sat.get_object());

    if (loadStatus)
    {
        // Load failed
        return loadStatus;
    }

    // Load success
    sat.set_loader(m_sat->get_object());
    m_activatedSats.push_back(sat.get_object());

    return 0;
}

void SatActiveArea::update_physics(float deltaTime)
{

    // scan for nearby satellites, maybe move this somewhere else some day

    Universe& u = *(m_sat->get_universe());
    std::vector<Satellite>& satellites = u.get_sats();

    for (Satellite& sat : satellites)
    {
        if (SatelliteObject* obj = sat.get_object())
        {
            //std::cout << "obj: " << obj->get_id().m_name
            //          << ", " << (void*)(&(obj->get_id())) << "\n";
        }
        else
        {
            // Satellite has no object, it's empty
            continue;
        }

        // if satellite is not loadable or is already activated
        if (!sat.is_activatable() || sat.get_loader())
        {
            //std::cout << "already loaded\n";
            continue;
        }


        // ignore self (unreachable as active areas are unloadable)
        //if (&sat == m_sat)
        //{
        //    continue;
        //}

        // test distance to satellite
        // btw: precision is 10
        Vector3 relativePos = sat.position_relative_to(*m_sat).to_meters();
        //std::cout << "nearby sat: " << sat.get_name() << " dot: " << relativePos.dot() << " satrad: " << sat.get_load_radius() << " arearad: " << m_sat->get_load_radius() << "\n";

        using Magnum::Math::pow;

        // ignore if too far
        if (relativePos.dot() > pow(sat.get_load_radius()
                                     + m_sat->get_load_radius(), 2.0f))
        {
            continue;
        }

        std::cout << "is near! dist: " << relativePos.length() << "/"
                  << (sat.get_load_radius() + m_sat->get_load_radius()) << "\n";

        // Satellite is near! Attempt to load it

        activate_satellite(sat);
    }


    CompTransform& cameraTransform
            = m_scene->reg_get<CompTransform>(m_camera);
    //cameraTransform.m_transform[3].xyz().x() += 0.1f; // move the camera right

    // Temporary: Floating origin follow cameara
    Magnum::Vector3i tra(cameraTransform.m_transform[3].xyz());

    m_scene->floating_origin_translate(-Vector3(tra));
    m_sat->set_position(m_sat->get_position()
                        + Vector3sp({tra.x() * 1024,
                                     tra.y() * 1024,
                                     tra.z() * 1024}, 10));
    //std::cout << "x: " << Vector3sp(m_sat->get_position()).x() << "\n";


    m_scene->update();

}

}
