#include <iostream>

#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Constants.h>
#include <Magnum/MeshTools/Compile.h>
#include <Magnum/MeshTools/Transform.h>
#include <Magnum/Primitives/Cube.h>

#include "SatActiveArea.h"

//#include "SatVehicle.h"
//#include "SatPlanet.h"


#include "../Active/ActiveScene.h"
#include "../Active/DebugObject.h"
#include "../Active/SysNewton.h"
#include "../Active/SysMachine.h"

#include "../Resource/SturdyImporter.h"
#include "../Resource/PlanetData.h"


//#include "../Active/Machines/Rocket.h"
//#include "../Active/Machines/UserControl.h"


#include "../Universe.h"


// for the 0xrrggbb_rgbf literalsm
using namespace Magnum::Math::Literals;

using Magnum::Matrix4;

namespace osp
{



SatActiveArea::SatActiveArea(UserInputHandler &userInput) :
        SatelliteObject(),
        m_loadedActive(false),
        m_userInput(userInput)
{


    // use area_load_vehicle to load SatVechicles
    //load_func_add<SatVehicle>(area_activate_vehicle);
    //load_func_add<SatPlanet>(area_activate_planet);


    //
    //("Rocket",
    //        std::make_unique<SysMachineRocket>(*this));
}

SatActiveArea::~SatActiveArea()
{
    // Unload everything
    for (SatelliteObject* satObj : m_activatedSats)
    {
        satObj->get_satellite()->set_loader(nullptr);
    }

    std::cout << "Active Area destroyed\n";

    // Clean up newton dynamics stuff
    //NewtonDestroyAllBodies(m_nwtWorld);
    //NewtonDestroy(m_nwtWorld);
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

    m_shader = std::make_unique<Magnum::Shaders::Phong>(Magnum::Shaders::Phong{});

    //m_partTest = part_instantiate(part);

    // Temporary: draw a spinning rectangular prisim

    m_bocks = (new Magnum::GL::Mesh(Magnum::MeshTools::compile(
                                        Magnum::Primitives::cubeSolid())));

    // Create the scene
    m_scene = std::make_shared<ActiveScene>(m_userInput, app);


    // create debug entities that have a transform and box model
    m_debug_aEnt = m_scene->hier_create_child(m_scene->hier_get_root(),
                                                   "Entity A");
    CompTransform& aTransform
            = m_scene->get_registry().emplace<CompTransform>(m_debug_aEnt);
    CompDrawableDebug& aBocks
            = m_scene->get_registry().emplace<CompDrawableDebug>(m_debug_aEnt,
                    m_bocks, m_shader.get(), 0x67FF00_rgbf);
    aTransform.m_enableFloatingOrigin = true;

    ActiveEnt bEnt = m_scene->hier_create_child(m_debug_aEnt, "Entity B");
    CompTransform& bTransform
            = m_scene->get_registry().emplace<CompTransform>(bEnt);
    CompDrawableDebug& bBocks
            = m_scene->get_registry().emplace<CompDrawableDebug>(bEnt,
                    m_bocks, m_shader.get(), 0xEE0201_rgbf);

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

    // Add the debug camera controller
    std::unique_ptr<DebugCameraController> camObj
            = std::make_unique<DebugCameraController>(*m_scene, m_camera);



    m_scene->reg_emplace<CompDebugObject>(m_camera, std::move(camObj));

    return 0;
}


void SatActiveArea::draw_gl()
{

    using namespace Magnum;

    // temporary stuff
    static Deg lazySpin;
    lazySpin += 3.0_degf;

    CompTransform& aTransform = m_scene->reg_get<CompTransform>(m_debug_aEnt);
    CompHierarchy& aHierarchy = m_scene->reg_get<CompHierarchy>(m_debug_aEnt);

    // root's child child is bEnt from activate();
    CompTransform& bTransform
            = m_scene->reg_get<CompTransform>(aHierarchy.m_childFirst);
    bTransform.m_transform
            = Matrix4::translation(Vector3(0.0f,
                                           2.0f * Math::sin(lazySpin),
                                           0.0f))
            * Matrix4::scaling({0.5f, 0.5f, 2.0f});;


    m_scene->update_hierarchy_transforms();

    GL::Renderer::enable(GL::Renderer::Feature::DepthTest);
    GL::Renderer::enable(GL::Renderer::Feature::FaceCulling);

    // Temporary: draw the spinning rectangular prisim



    Matrix4 ttt;
    ttt = Matrix4::translation(Vector3(0, 0, -5))
        * Matrix4::rotationX(lazySpin)
        * Matrix4::scaling({1.0f, 1.0f, 1.0f});

    (*m_shader)
        .setDiffuseColor(0x67ff00_rgbf)
        .setAmbientColor(0x111111_rgbf)
        .setSpecularColor(0x330000_rgbf)
        .setLightPosition({10.0f, 15.0f, 5.0f})
        .setTransformationMatrix(ttt)
        .setProjectionMatrix(Magnum::Matrix4::perspectiveProjection(45.0_degf, 1.0f, 0.001f, 100.0f))
        .setNormalMatrix(ttt.normalMatrix());

    //m_bocks->draw(*m_shader);

    m_scene->draw(m_camera);

    //m_partTest->setTransformation(Matrix4::rotationX(lazySpin));

    //static_cast<Object3D&>(m_camera->object()).setTransformation(
    //                        Matrix4::translation(Vector3(0, 0, 5)));


    //m_camera->draw(m_drawables);
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

    std::cout << "loading!\n";
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

    // scan for nearby satellites, partially tremporary

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
