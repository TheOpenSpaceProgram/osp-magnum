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
#include <iostream>

#include <Corrade/Containers/Optional.h>
#include <Corrade/PluginManager/Manager.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/MeshObjectData3D.h>
#include <Magnum/Trade/SceneData.h>
#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/TextureData.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/MaterialData.h>
#include <Magnum/Trade/PbrMetallicRoughnessMaterialData.h>
#include <Magnum/MeshTools/Compile.h>
#include <Magnum/Mesh.h>


#include <MagnumExternal/TinyGltf/tiny_gltf.h>

#include "Package.h"
#include "SturdyImporter.h"

namespace osp
{

using Magnum::Containers::Pointer;
using Magnum::Trade::MeshData;
using Magnum::Trade::TextureData;
using Magnum::Trade::ImageData2D;
using Magnum::Trade::ObjectData3D;
using Magnum::Trade::MaterialData;
using Magnum::Trade::SceneData;
using Magnum::Containers::Optional;

SturdyImporter::SturdyImporter() : m_gltfImporter(m_pluginManager)
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

            //PrototypePart part;
            PrototypePart part;
            //part.m_name = nodeName;
            //part.m_path = nodeName;

            // Add objects to the part, and recurse
            proto_add_obj_recurse(package, part, 0, childId);

            // Parse extras
            tinygltf::Node const& node = *static_cast<tinygltf::Node const*>(
                        m_gltfImporter.object3D(childId)->importerState());

            tinygltf::Value const& extras = node.extras;
            tinygltf::Value const& machines = extras.Get("machines");
            if (machines.IsArray())
            {
                std::cout << "JSON machines!\n";
                auto const& machArray = machines.Get<tinygltf::Value::Array>();

                // Loop through machine configs
                // machArray looks like:
                // [
                //    { "type": "Rocket", stuff... },
                //    { "type": "Control", stuff...}
                // ]
                for (tinygltf::Value const& value : machArray)
                {

                    std::string type = value.Get("type").Get<std::string>();
                    std::cout << "test: " << type << "\n";

                    if (type.empty())
                    {
                        continue;
                    }

                    // TODO: more stuff
                    PrototypeMachine machine;
                    machine.m_type = std::move(type);
                    part.get_machines().emplace_back(std::move(machine));
                }
            }
            else
            {
                std::cout << "JSON machines array not found\n";
            }

            package.add<PrototypePart>(nodeName, std::move(part));

        }

    }

    // Loop through and all meshes
    // Load them immediately for the sake of development
    // eventually:
    // * load only required meshes
    for (unsigned i = 0; i < m_gltfImporter.meshCount(); i ++)
    {
        std::string const& meshName = m_gltfImporter.meshName(i);
        std::cout << "Mesh: " << meshName << "\n";

        Optional<MeshData> meshData = m_gltfImporter.mesh(i);
        if (!meshData
                || (meshData->primitive() != Magnum::MeshPrimitive::Triangles))
        {
            continue;
        }

        //Resource<MeshData3D> meshDataRes(std::move(*meshData));
        //meshDataRes.m_path = meshName;

        package.add<MeshData>(meshName, std::move(*meshData));
        
        // apparently this needs a GL context
        // maybe store compiled meshes in the active area, since they're
        // specific to openGL
        //Resource<Magnum::GL::Mesh> meshResource(
        //            std::move(Magnum::MeshTools::compile(*meshData)));

        //package.debug_add_resource(std::move(meshResource));
    }

    for (unsigned i = 0; i < m_gltfImporter.textureCount(); i++)
    {
        auto imgID = m_gltfImporter.texture(i)->image();
        std::string const& imgName = m_gltfImporter.image2DName(imgID);
        std::cout << "Loading image: " << imgName << "\n";
        Optional<ImageData2D> imgData = m_gltfImporter.image2D(imgID);

        if (!imgData)
        {
            continue;
        }
        package.add<ImageData2D>(imgName, std::move(*imgData));
    }
}
//either an appendable package, or
void SturdyImporter::proto_add_obj_recurse(Package& package,
                                           PrototypePart& part,
                                           unsigned parentProtoIndex,
                                           unsigned childGltfIndex)
{
    // Add the object to the prototype
    Pointer<ObjectData3D> childData = m_gltfImporter.object3D(childGltfIndex);
    std::vector<PrototypeObject>& protoObjects = part.get_objects();
    const std::string& name = m_gltfImporter.object3DName(childGltfIndex);

    // I think I've been doing too much C
    PrototypeObject obj;
    obj.m_parentIndex = parentProtoIndex;
    obj.m_childCount = childData->children().size();
    obj.m_translation = childData->translation();
    obj.m_rotation = childData->rotation();
    obj.m_scale = childData->scaling();
    obj.m_type = ObjectType::NONE;
    obj.m_name = name;
    //obj.m_transform = childData->transformation();

    std::cout << "Adding obj to Part: " << name << "\n";
    int meshID = childData->instance();

    bool hasMesh = (
            childData->instanceType() == Magnum::Trade::ObjectInstanceType3D::Mesh
            && meshID != -1);

    if (name.compare(0, 4, "col_") == 0)
    {   // It's a collider
        obj.m_type = ObjectType::COLLIDER;

        // do some stuff here
        obj.m_objectData = ColliderData{ECollisionShape::BOX};

        std::cout << "obj: " << name << " is a collider\n";
    }
    else if (hasMesh)
    {   // It's a drawable mesh
        const std::string& meshName = m_gltfImporter.meshName(meshID);
        std::cout << "obj: " << name << " uses mesh: " << meshName << "\n";
        obj.m_type = ObjectType::MESH;

        //obj.m_drawable.m_mesh = m_meshOffset + childData->instance();

        // The way it's currently set up is that the mesh's names are the same
        // as their resource paths. So the resource path is added to the part's
        // list of strings, and the object's mesh is set to the index to that
        // string.
        obj.m_objectData = DrawableData{ static_cast<unsigned>(part.get_strings().size()) };
        part.get_strings().push_back(meshName);

        Pointer<MaterialData> mat = m_gltfImporter.material(meshID);
        
        if (mat->types() & Magnum::Trade::MaterialType::PbrMetallicRoughness)
        {
            const auto& pbr =
                mat->as<Magnum::Trade::PbrMetallicRoughnessMaterialData>();

            auto imgID = m_gltfImporter.texture(pbr.baseColorTexture())->image();
            std::string const& imgName = m_gltfImporter.image2DName(imgID);
            std::cout << "Base Tex: " << imgName << "\n";
            std::get<DrawableData>(obj.m_objectData).m_textures.push_back(
                static_cast<unsigned>(part.get_strings().size()));
            part.get_strings().push_back(imgName);

            if (pbr.hasNoneRoughnessMetallicTexture())
            {
                imgID = m_gltfImporter.texture(pbr.metalnessTexture())->image();
                std::cout << "Metal/rough texture: " << m_gltfImporter.image2DName(imgID) << "\n";
            } else
            {
                std::cout << "No metal/rough texture found for " << name << "\n";
            }
            
        } else
        {
            std::cout << "Error: unsupported material type\n";
        }
    }

    //obj.m_mesh = m_meshOffset + childData->
    int objIndex = protoObjects.size();
    protoObjects.push_back(std::move(obj));


    for (unsigned childId: childData->children())
    {
        //proto_add_obj_recurse(part, protoObjects.size() - 1, childId);
        proto_add_obj_recurse(package, part, objIndex, childId);
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

