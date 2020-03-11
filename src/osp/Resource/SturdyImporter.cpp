#include <iostream>

#include <Corrade/Containers/Optional.h>
#include <Corrade/PluginManager/Manager.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/MeshObjectData3D.h>
#include <Magnum/Trade/SceneData.h>
#include <Magnum/MeshTools/Compile.h>

#include "Package.h"
#include "SturdyImporter.h"

namespace osp
{

using Magnum::Containers::Pointer;
using Magnum::Trade::MeshData3D;
using Magnum::Trade::ObjectData3D;
using Magnum::Trade::SceneData;
using Magnum::Containers::Optional;

SturdyImporter::SturdyImporter()
{

    //Magnum::PluginManager::Manager<Magnum::Trade::AbstractImporter> manager;
    //Magnum::Containers::Pointer<Magnum::Trade::AbstractImporter> importer =
    //manager.loadAndInstantiate("GltfImporter");
}

void SturdyImporter::load_config(Package& package)
{
    // return when there's no data to load.
    if (!m_gltfImporter.isOpened()
        || m_gltfImporter.defaultScene() == -1)
    {
        return;
    }

    // try to actually load the scene

    std::cout << "# of nodes: " << m_gltfImporter.object3DCount() << "\n";

    Magnum::Containers::Optional<SceneData> sceneData
                = m_gltfImporter.scene(m_gltfImporter.defaultScene());

    if (!sceneData)
    {
        std::cout << "Cannot load scene, exiting\n";
        return;
    }


    // Find Parts by looping through all top-level nodes
    for(unsigned childId: sceneData->children3D())
    {
        const std::string& nodeName = m_gltfImporter.object3DName(childId);

        std::cout << "Node name: " << nodeName << "\n";

        if (nodeName.compare(0, 5, "part_") == 0)
        {

            std::cout << "PART!\n";

            //PartPrototype part;
            Resource<PartPrototype> part;
            //part.m_name = nodeName;
            part.m_path = nodeName;

            // Add objects to the part, and recurse
            proto_add_obj_recurse(part.m_data, 0, childId);

            package.debug_add_resource<PartPrototype>(std::move(part));

        }

    }

    // Loop through and all meshes
    // Load them immediately for the sake of development
    // eventually:
    // * load only required meshes
    for (unsigned i = 0; i < m_gltfImporter.mesh3DCount(); i ++)
    {
        std::string const& meshName = m_gltfImporter.mesh3DName(i);
        std::cout << "Mesh: " << meshName << "\n";

        Optional<MeshData3D> meshData = m_gltfImporter.mesh3D(i);
        if (!meshData || !meshData->hasNormals()
                || meshData->primitive() != Magnum::MeshPrimitive::Triangles)
        {
            continue;
        }

        Resource<MeshData3D> meshDataRes(std::move(*meshData));
        meshDataRes.m_path = meshName;


        // random dependency class test
        std::cout << "emptyA:" << meshDataRes.isEmpty() << "\n";
        {
        ResDependency<Magnum::Trade::MeshData3D> test;
        test.bind(meshDataRes);

        std::cout << "emptyB:" << meshDataRes.isEmpty() << "\n";


        }

        std::cout << "emptyC:" << meshDataRes.isEmpty() << "\n";

        package.debug_add_resource(std::move(meshDataRes));





        // apparently this needs a GL context
        // maybe store compiled meshes in the active area, since they're
        // specific to openGL
        //Resource<Magnum::GL::Mesh> meshResource(
        //            std::move(Magnum::MeshTools::compile(*meshData)));

        //package.debug_add_resource(std::move(meshResource));
    }

}
//either an appendable package, or
void SturdyImporter::proto_add_obj_recurse(PartPrototype& part,
                                           unsigned parentProtoIndex,
                                           unsigned childGltfIndex)
{
    // Add the object to the prototype
    Pointer<ObjectData3D> childData = m_gltfImporter.object3D(childGltfIndex);
    std::vector<ObjectPrototype>& protoObjects = part.get_objects();
    const std::string& name = m_gltfImporter.object3DName(childGltfIndex);

    // I think I've been doing too much C
    ObjectPrototype obj;
    obj.m_parentIndex = parentProtoIndex;
    obj.m_childCount = childData->children().size();
    obj.m_translation = childData->translation();
    obj.m_rotation = childData->rotation();
    obj.m_scale = childData->scaling();
    obj.m_type = ObjectType::NONE;
    obj.m_name = name;
    //obj.m_transform = childData->transformation();

    std::cout << "Adding obj to Part: " << name << "\n";

    bool hasMesh = (
            childData->instanceType() == Magnum::Trade::ObjectInstanceType3D::Mesh
            && childData->instance() != -1);

    if (name.compare(0, 4, "col_") == 0)
    {
        // it's a collider
        obj.m_type = ObjectType::COLLIDER;

        // do some stuff here
        obj.m_collider.m_type = ColliderType::CUBE;

        std::cout << "obj: " << name << " is a collider\n";
    }
    else if (hasMesh)
    {
        // Drawable mesh
        const std::string& meshName = m_gltfImporter
                                            .mesh3DName(childData->instance());
        std::cout << "obj: " << name << " uses mesh: " << meshName << "\n";
        obj.m_type = ObjectType::MESH;
        //obj.m_drawable.m_mesh = m_meshOffset + childData->instance();

        // The way it's currently set up is that the mesh's names are the same
        // as their resource paths. So the resource path is added to the part's
        // list of strings, and the object's mesh is set to the index to that
        // string.
        obj.m_drawable.m_mesh = obj.m_strings.size();
        obj.m_strings.push_back(meshName);
    }

    //obj.m_mesh = m_meshOffset + childData->
    int objIndex = protoObjects.size();
    protoObjects.push_back(std::move(obj));


    for (unsigned childId: childData->children())
    {
        //proto_add_obj_recurse(part, protoObjects.size() - 1, childId);
        proto_add_obj_recurse(part, objIndex, childId);
    }
}


void SturdyImporter::open_filepath(const std::string& filepath)
{
    m_gltfImporter.openFile(filepath);

}

void SturdyImporter::close()
{

}


/* void SturdyImporter::debug_add_obj_recurse(Object3D &parent, unsigned id)
{
    std::cout << "node: " << m_gltfImporter.object3DName(id) << "\n";
    Corrade::Containers::Pointer<Magnum::Trade::ObjectData3D> objectData = m_gltfImporter.object3D(id);
    Object3D* object = new Object3D{&parent};
    object->setTransformation(objectData->transformation());

    for (unsigned childId: objectData->children())
    {
        debug_add_obj_recurse(parent, childId);
    }
}

void SturdyImporter::dump_nodes(Scene3D &nodeDump)
{
    std::cout << "objects: " << m_gltfImporter.object3DCount() << "\n";

    if (m_gltfImporter.defaultScene() != -1)
    {
        std::cout << "scene loaded!\n";

        //Magnum::Containers::StaticArraySceneData

        Magnum::Containers::Optional<SceneData> sceneData
                    = m_gltfImporter.scene(m_gltfImporter.defaultScene());
        if (!sceneData)
        {
            std::cout << "Cannot load scene, exiting\n";
            return;
        }

        for(unsigned childId: sceneData->children3D())
        {
            debug_add_obj_recurse(nodeDump, childId);
        }


    }
}*/

}

