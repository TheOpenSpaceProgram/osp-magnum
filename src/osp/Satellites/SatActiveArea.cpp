
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Constants.h>
#include <Magnum/MeshTools/Compile.h>
#include <Magnum/MeshTools/Transform.h>
#include <Magnum/Primitives/Cube.h>



#include <iostream>


#include "SatActiveArea.h"
#include "SatVehicle.h"
#include "../Resource/SturdyImporter.h"
#include "../Universe.h"


// for the 0xrrggbb_rgbf literals
using namespace Magnum::Math::Literals;

using Magnum::Matrix4;

namespace osp
{


// Loading functions


int area_load_vehicle(SatActiveArea& area, SatelliteObject& loadMe)
{
    std::cout << "loadin a vehicle!\n";
    return 0;
}

SatActiveArea::SatActiveArea() : SatelliteObject()
{
    // use area_load_vehicle to load SatVechicles
    load_func_add<SatVehicle>(area_load_vehicle);
}

SatActiveArea::~SatActiveArea()
{
    // do something here eventually
}



Object3D* SatActiveArea::part_instantiate(PartPrototype& part, Package& package)
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


            Mesh* mesh = nullptr;
            Resource<Mesh>* meshRes
                    = package.get_resource<Mesh>(current.m_drawable.m_mesh);


            if (!meshRes)
            {
                // Mesh isn't compiled yet, now check if mesh data exists
                std::string const& meshName
                        = current.m_strings[current.m_drawable.m_mesh];

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



    }
    // first element is 100% going to be the root object
    return newObjects[0];
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

        if (!sat.is_loadable())
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

        std::cout << "is near!\n";

        // Satellite is near! Attempt to load it

        // see if there's a loading function for the satellite object
        auto funcMapIt =
                m_loadFunctions.find(&(sat.get_object()->get_id()));

        if(funcMapIt != m_loadFunctions.end())
        {
            std::cout << "loading!\n";
            funcMapIt->second(*this, *sat.get_object());
        }
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

    static_cast<Object3D&>(m_camera->object()).setTransformation(
                            Matrix4::translation(Vector3(0, 0, 5)));


    m_camera->draw(m_drawables);
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

    m_partTest = part_instantiate(part, p);

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

    return 0;
}

}
