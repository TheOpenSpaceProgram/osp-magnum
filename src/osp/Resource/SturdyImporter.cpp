#include <iostream>

#include <Corrade/Containers/Optional.h>
#include <Corrade/PluginManager/Manager.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/MeshObjectData3D.h>
#include <Magnum/Trade/SceneData.h>
#include "SturdyImporter.h"

namespace osp
{

using Magnum::Trade::SceneData;

SturdyImporter::SturdyImporter()
{

    //Magnum::PluginManager::Manager<Magnum::Trade::AbstractImporter> manager;
    //Magnum::Containers::Pointer<Magnum::Trade::AbstractImporter> importer =
    //manager.loadAndInstantiate("GltfImporter");




}

void SturdyImporter::obtain_parts(std::vector<PartPrototype> parts)
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


    // Loop through top-level nodes
    for(unsigned childId: sceneData->children3D())
    {
        const std::string& nodeName = m_gltfImporter.object3DName(childId);

        std::cout << "Node name: " << nodeName << "\n";

        if (nodeName.compare(0, 5, "part_") == 0)
        {
            std::cout << "PART!\n";
        }

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

void SturdyImporter::load_parts(int index, PartPrototype& out)
{

}

}

