#include <iostream>

#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Constants.h>
#include <Magnum/MeshTools/Compile.h>
#include <Magnum/MeshTools/Transform.h>
#include <Magnum/Primitives/Cube.h>

#include "SatActiveArea.h"
#include "SatVehicle.h"
#include "SatPlanet.h"

#include "../Active/FtrPlanet.h"
#include "../Active/FtrNewtonBody.h"
#include "../Active/FtrVehicle.h"

#include "../Resource/SturdyImporter.h"
#include "../Resource/PlanetData.h"

#include "../Universe.h"


// for the 0xrrggbb_rgbf literals
using namespace Magnum::Math::Literals;

using Magnum::Matrix4;

namespace osp
{

// debug navigation
struct InputControl
{
    bool up, dn, fr, bk, lf, rt;
} g_debugInput;

// Loading functions


int area_load_vehicle(SatActiveArea& area, SatelliteObject& loadMe)
{
    std::cout << "loadin a vehicle!\n";

    // Get the needed variables
    SatVehicle& vehicle = static_cast<SatVehicle&>(loadMe);
    VehicleBlueprint& vehicleData = vehicle.get_blueprint();

    // List of unique part prototypes used in the vehicle
    // Access with [partBlueprint.m_partIndex]
    std::vector<ResDependency<PartPrototype> >& partsUsed =
            vehicleData.get_prototypes();

    // List of parts, and how they're arranged
    std::vector<PartBlueprint>& parts = vehicleData.get_blueprints();

    // Loop through blueprints
    for (PartBlueprint& partBp : parts)
    {
        ResDependency<PartPrototype>& partDepends =
                partsUsed[partBp.m_partIndex];

        PartPrototype *proto = partDepends.get_data();

        // Check if the part prototype this depends on still exists
        if (!proto)
        {
            return -1;
        }

        Object3D* partObject = area.part_instantiate(*proto);

        // set the transformation
        partObject->setTransformation(
                Matrix4::from(partBp.m_rotation.toMatrix(),
                              partBp.m_translation)
                * Matrix4::scaling(partBp.m_scale));
    }
    return 0;
}

int area_load_planet(SatActiveArea& area, SatelliteObject& loadMe)
{
    std::cout << "loadin a planet!\n";

    // Get the needed variables
    SatPlanet& planet = static_cast<SatPlanet&>(loadMe);

    Object3D *obj = new Object3D(&(area.get_scene()));

    PlanetData dummy;

    new FtrPlanet(*obj, dummy);

    return 0;
}


SatActiveArea::SatActiveArea() : SatelliteObject(), m_nwtWorld(NewtonCreate())
{
    // use area_load_vehicle to load SatVechicles
    load_func_add<SatVehicle>(area_load_vehicle);

    load_func_add<SatPlanet>(area_load_planet);
}

SatActiveArea::~SatActiveArea()
{
    // TODO: unload satellites

    std::cout << "Active Area destroyed\n";

    // Clean up newton dynamics stuff
    NewtonDestroyAllBodies(m_nwtWorld);
    NewtonDestroy(m_nwtWorld);
}


int SatActiveArea::activate()
{
    // when switched to. Active areas can only be loaded by the universe.
    // why, and how would an active area load another active area?
    m_loadedActive = true;

    std::cout << "Active Area loading...\n";


    // Temporary: instantiate a part

    Universe* u = m_sat->get_universe();
    Package& p = u->debug_get_packges()[0];
    PartPrototype& part = p.get_resource<PartPrototype>(0)->m_data;

    m_shader = std::make_unique<Magnum::Shaders::Phong>(Magnum::Shaders::Phong{});

    //m_partTest = part_instantiate(part);

    // Temporary: draw a spinning rectangular prisim

    //m_bocks = &(Magnum::GL::Mesh(Magnum::MeshTools::compile(
    //                                    Magnum::Primitives::cubeSolid())));

    Object3D *cameraObj = new Object3D{&m_scene};
    (*cameraObj).translate(Magnum::Vector3::zAxis(100.0f));
    //            .rotate();
    m_camera = new Magnum::SceneGraph::Camera3D(*cameraObj);
    (*m_camera)
        .setAspectRatioPolicy(Magnum::SceneGraph::AspectRatioPolicy::Extend)
        .setProjectionMatrix(Magnum::Matrix4::perspectiveProjection(45.0_degf, 1.0f, 0.001f, 100.0f))
        .setViewport(Magnum::GL::defaultFramebuffer.viewport().size());

    cameraObj->setTransformation( Matrix4::translation(Vector3(0, 0, 5)));

    return 0;
}


void SatActiveArea::draw_gl()
{

    // lazy way to set name
    m_sat->set_name("ActiveArea");

    // scan for nearby satellites, partially remporary

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

        // if satellite is not loadable or is already loaded
        if (!sat.is_loadable() || sat.get_loader())
        {
            continue;
        }


        // ignore self (unreachable as active areas are unloadable)
        //if (&sat == m_sat)
        //{
        //    continue;
        //}

        // test distance to satellite
        // btw: precision is 10
        Vector3s relativePos = sat.position_relative_to(*m_sat);
        //std::cout << "nearby sat: " << sat.get_name() << " dot: " << relativePos.dot() << " satrad: " << sat.get_load_radius() << " arearad: " << m_sat->get_load_radius() << "\n";

        if (relativePos.dot()
                > Magnum::Math::pow(sat.get_load_radius()
                                        + m_sat->get_load_radius(), 2.0f))
        {
            continue;
        }

        //std::cout << "is near!\n";

        // Satellite is near! Attempt to load it

        load_satellite(sat);
    }

    using namespace Magnum;

    GL::Renderer::enable(GL::Renderer::Feature::DepthTest);
    GL::Renderer::enable(GL::Renderer::Feature::FaceCulling);

    // Temporary: draw the spinning rectangular prisim

    //static Deg lazySpin;
    //lazySpin += 1.0_degf;

    //Matrix4 ttt;
    //ttt = Matrix4::translation(Vector3(0, 0, -5))
    //    * Matrix4::rotationX(lazySpin)
    //    * Matrix4::scaling({1.0f, 1.0f, 1.0f});

    //(*m_shader)
    //    .setDiffuseColor(0x67ff00_rgbf)
    //    .setAmbientColor(0x111111_rgbf)
    //    .setSpecularColor(0x330000_rgbf)
    //    .setLightPosition({10.0f, 15.0f, 5.0f})
    //    .setTransformationMatrix(ttt)
    //    .setProjectionMatrix(m_camera->projectionMatrix())
    //    .setNormalMatrix(ttt.normalMatrix());

    //m_bocks->draw(*m_shader);

    //m_partTest->setTransformation(Matrix4::rotationX(lazySpin));

    //static_cast<Object3D&>(m_camera->object()).setTransformation(
    //                        Matrix4::translation(Vector3(0, 0, 5)));


    m_camera->draw(m_drawables);
}


Object3D* SatActiveArea::part_instantiate(PartPrototype& part)
{

    std::vector<ObjectPrototype> const& prototypes = part.get_objects();
    std::vector<Object3D*> newObjects(prototypes.size());

    std::cout << "size: " << newObjects.size() << "\n";

    for (int i = 0; i < prototypes.size(); i ++)
    {
        const ObjectPrototype& current = prototypes[i];
        Object3D *obj = new Object3D(&m_scene);
        newObjects[i] = obj;

        std::cout << "objind:" << " parent: " << current.m_parentIndex << "\n";
        std::cout << "trans: " << current.m_translation.x() << ", " << current.m_translation.y() << ", " << current.m_translation.z() << " \n";
        std::cout << "scale: " << current.m_scale.x() << ", " << current.m_scale.y() << ", " << current.m_scale.z() << " \n";

        if (current.m_parentIndex == i)
        {
            // if parented to self,
            //obj->setParent(&m_scene);
        }
        else
        {
            // since objects were loaded recursively, the parents always load
            // before their children
            obj->setParent(newObjects[current.m_parentIndex]);
        }

        obj->setTransformation(Matrix4::from(current.m_rotation.toMatrix(),
                                             current.m_translation)
                               * Matrix4::scaling(current.m_scale));

        if (current.m_type == ObjectType::MESH)
        {
            using Magnum::Trade::MeshData3D;
            using Magnum::GL::Mesh;

            // TODO: put package prefixes in resource path
            //       for now just get the first package
            Package& package = m_sat->get_universe()->debug_get_packges()[0];

            Mesh* mesh = nullptr;
            Resource<Mesh>* meshRes
                    = package.get_resource<Mesh>(current.m_drawable.m_mesh);


            if (!meshRes)
            {
                // Mesh isn't compiled yet, now check if mesh data exists
                std::string const& meshName
                        = part.get_strings()[current.m_drawable.m_mesh];

                Resource<MeshData3D>* meshDataRes
                        = package.get_resource<MeshData3D>(meshName);

                if (meshDataRes)
                {
                    // Compile the mesh data into a mesh

                    std::cout << "Compiling mesh \"" << meshName << "\"\n";

                    MeshData3D* meshData = &(meshDataRes->m_data);


                    Resource<Mesh> compiledMesh;
                    compiledMesh.m_data = Magnum::MeshTools::compile(*meshData);

                    mesh = &(package.debug_add_resource<Mesh>(
                                std::move(compiledMesh))->m_data);

                    m_bocks = mesh;
                }
                else
                {
                    std::cout << "Mesh \"" << meshName << "\" doesn't exist!\n";
                    return nullptr;
                }

            }
            else
            {
                // mesh already loaded
                // TODO: this actually crashes, as all the opengl stuff were
                //       cleared.
                mesh = &(meshRes->m_data);
            }

            // by now, the mesh should exist

            new DrawablePhongColored(*obj, *m_shader, *mesh, 0xff0000_rgbf, m_drawables);
        }
        else if (current.m_type == ObjectType::COLLIDER)
        {
            //new FtrNewtonBody(*obj, *this);
            // TODO collision shape!
        }
    }



    // first element is 100% going to be the root object

    // Temporary: add a rigid body root
    new FtrNewtonBody(*(newObjects[0]), *this);

    // return root object
    return newObjects[0];
}


int SatActiveArea::load_satellite(Satellite& sat)
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
    int loadStatus = funcMapIt->second(*this, *sat.get_object());

    if (loadStatus)
    {
        // Load failed
        return loadStatus;
    }

    // Load success
    sat.set_loader(m_sat->get_object());

    return 0;
}

void SatActiveArea::update_physics(float deltaTime)
{
    // debug navigation

    Object3D &cameraObj = static_cast<Object3D&>(m_camera->object());
    float spd = 0.05f;

    Vector3 mvtLocal((g_debugInput.rt - g_debugInput.lf) * spd, 0.0f,
                     (g_debugInput.bk - g_debugInput.fr) * spd);
    cameraObj.translateLocal(mvtLocal);

    Vector3 mvtGlobal(0.0f, (g_debugInput.up - g_debugInput.dn) * spd, 0.0f);
    cameraObj.translate(mvtGlobal);

    // update physics
    NewtonUpdate(m_nwtWorld, deltaTime);

    GroupFtrNewtonBody& nwtBodies = *this;

    for (unsigned i = 0; i < nwtBodies.size(); i++)
    {
        // Update positions of magnum objects
        // TODO: interpolation

        Matrix4 matrix;
        NewtonBodyGetMatrix(nwtBodies[i].get_body(), matrix.data());
        Vector3 trans = matrix.translation();

        Object3D& obj = static_cast<Object3D&>(nwtBodies[i].object());
        obj.setTransformation(matrix);


        //std::cout << "translation: [" << trans.x() << ", " << trans.y() << ", " << trans.z() << "]\n";
        //Vector3 force(0.0f, -9.8f, 0.0f);
        dFloat force[3] = {0, -1.0, 0};
        NewtonBodySetForce(nwtBodies[i].get_body(), force);
    }

}

using Magnum::Platform::Application;

void SatActiveArea::input_key_press(Application::KeyEvent& event)
{

    const bool dir = true;
    // debug navigation controls
    switch(event.key())
    {
    case Application::KeyEvent::Key::W:
        g_debugInput.fr = dir;
        break;
    case Application::KeyEvent::Key::A:
        g_debugInput.lf = dir;
        break;
    case Application::KeyEvent::Key::S:
        g_debugInput.bk = dir;
        break;
    case Application::KeyEvent::Key::D:
        g_debugInput.rt = dir;
        break;
    case Application::KeyEvent::Key::Q:
        g_debugInput.up = dir;
        break;
    case Application::KeyEvent::Key::E:
        g_debugInput.dn = dir;
        break;
    }
}

void SatActiveArea::input_key_release(Application::KeyEvent& event)
{
    const bool dir = false;
    // debug navigation controls
    switch(event.key())
    {
    case Application::KeyEvent::Key::W:
        g_debugInput.fr = dir;
        break;
    case Application::KeyEvent::Key::A:
        g_debugInput.lf = dir;
        break;
    case Application::KeyEvent::Key::S:
        g_debugInput.bk = dir;
        break;
    case Application::KeyEvent::Key::D:
        g_debugInput.rt = dir;
        break;
    case Application::KeyEvent::Key::Q:
        g_debugInput.up = dir;
        break;
    case Application::KeyEvent::Key::E:
        g_debugInput.dn = dir;
        break;
    }
}

void SatActiveArea::input_mouse_press(Application::MouseEvent& event)
{

}

void SatActiveArea::input_mouse_release(Application::MouseEvent& event)
{

}

void SatActiveArea::input_mouse_move(Application::MouseMoveEvent& event)
{

}

}
