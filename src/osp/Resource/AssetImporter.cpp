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

#include "Package.h"
#include "AssetImporter.h"
#include "osp/string_concat.h"
#include "adera/Plume.h"
#include "machines.h"

#include <Corrade/Containers/Optional.h>
#include <Corrade/PluginManager/Manager.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/MeshObjectData3D.h>
#include <Magnum/Trade/SceneData.h>
#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/TextureData.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/ImageView.h>
#include <Magnum/Trade/MaterialData.h>
#include <Magnum/Trade/PbrMetallicRoughnessMaterialData.h>
#include <Magnum/MeshTools/Compile.h>
#include <Magnum/GL/Mesh.h>


#include <MagnumExternal/TinyGltf/tiny_gltf.h>



using Corrade::Containers::Optional;
using Magnum::Trade::ImageData2D;
using Magnum::Trade::MeshData;
using Magnum::Trade::SceneData;
using Magnum::GL::Texture2D;
using Magnum::GL::Mesh;
using Magnum::UnsignedInt;

namespace osp
{

void osp::AssetImporter::load_sturdy_file(std::string_view filepath, Package& rMachinePkg, Package& pkg)
{
    PluginManager pluginManager;
    TinyGltfImporter gltfImporter{pluginManager};

    // Open .sturdy.gltf file
    gltfImporter.openFile(std::string{filepath});
    if (!gltfImporter.isOpened() || gltfImporter.defaultScene() == -1)
    {
        SPDLOG_LOGGER_ERROR(get_logger(), "Could not open file {}", filepath);
    }

    // repurpose filepath into unique identifier for file resources
    std::string dataResourceName =
        string_concat(filepath.substr(0, filepath.find('.')), ":");

    load_sturdy(gltfImporter, dataResourceName, rMachinePkg, pkg);

    gltfImporter.close();
}

void AssetImporter::proto_load_machines(
        PartEntity_t entity,
        Package& rMachinePkg,
        tinygltf::Value const& extras,
        std::vector<PCompMachine>& rMachines)
{
    if (!extras.Has("machines"))
    {
        SPDLOG_LOGGER_ERROR(get_logger(), "Error: no machines found!");
        return;
    }
    tinygltf::Value const& machines = extras.Get("machines");

    SPDLOG_LOGGER_INFO(get_logger(), "JSON machines!");

    auto const& machArray = machines.Get<tinygltf::Value::Array>();

    std::vector<uint32_t> machineIndices;
    machineIndices.reserve(machArray.size());
    // Loop through machine configs
    // machArray looks like:
    // [
    //    { "type": "Rocket", stuff... },
    //    { "type": "Control", stuff...}
    // ]
    for (tinygltf::Value const& value : machArray)
    {
        std::string type = value.Get("type").Get<std::string>();

        SPDLOG_LOGGER_INFO(get_logger(), "test: {}", type);

        if (type.empty())
        {
            continue;
        }

        //rMachine.m_entity = entity;

        // Resolve machine type
        DependRes<RegisteredMachine> machineType = rMachinePkg.get<RegisteredMachine>(type);
        if (machineType.empty())
        {
            SPDLOG_LOGGER_ERROR(get_logger(), "Machine Type not found: {}", type);
            continue; // machine type not found
        }
        PCompMachine &rMachine = rMachines.emplace_back();
        rMachine.m_type = machineType->m_id;
        rMachine.m_entity = entity;

        for (auto const& key : value.Keys())
        {
          SPDLOG_LOGGER_INFO(get_logger(), "Reading config \'{}\'", key);

            tinygltf::Value v = value.Get(key);
            switch (v.Type())
            {
            case tinygltf::Type::REAL_TYPE:
            {
                double val = v.Get<double>();
                rMachine.m_config.emplace(key, val);
                SPDLOG_LOGGER_INFO(get_logger(), "{} is (real)", val);
                break;
            }

            case tinygltf::Type::INT_TYPE:
            {
                int val = v.Get<int>();
                rMachine.m_config.emplace(key, val);
                SPDLOG_LOGGER_INFO(get_logger(), "{} is (int)", val);
                break;
            }
            case tinygltf::Type::STRING_TYPE:
            {
                std::string val = v.Get<std::string>();
                rMachine.m_config.emplace(key, std::move(val));
                SPDLOG_LOGGER_INFO(get_logger(), "{} is (string)", val);
                break;
            }
            default:
                SPDLOG_LOGGER_INFO(get_logger(), "(unknown)");
                break;
            }
        }
        /* TODO: eventually it would be nice to pre-allocate the PrototypePart's
         * machine array, but for now it doesn't seem worth it to pre-traverse
         * the whole part tree just for this purpose. If we do end up with a
         * more involved loading scheme, we can change this then.
         */
    }
}

void osp::AssetImporter::load_part(
        TinyGltfImporter& gltfImporter,
        Package& rMachinePkg,
        Package& pkg,
        UnsignedInt id,
        std::string_view resPrefix)
{
    // Recursively add child nodes to part
    PrototypePart part;


    proto_add_obj_recurse(gltfImporter, rMachinePkg, pkg, resPrefix, part, 0, id);

    // Parse extra properties
    tinygltf::Value const& extras = static_cast<tinygltf::Node const*>(
        gltfImporter.object3D(id)->importerState())->extras;

    // TODO: individual glTF nodes can now have masses, but there's no
    //       implementation for it yet. This is a workaround to keep the old
    //       system
    PCompMass &totalMassTemporary = part.m_partMass.emplace_back();
    totalMassTemporary.m_entity = 0;
    totalMassTemporary.m_mass = extras.Get("massdry").Get<double>();

    pkg.add<PrototypePart>(gltfImporter.object3DName(id), std::move(part));
}

void osp::AssetImporter::load_plume(TinyGltfImporter& gltfImporter,
    Package& pkg, Magnum::UnsignedInt id, std::string_view resPrefix)
{
    using Corrade::Containers::Pointer;
    using Magnum::Trade::ObjectData3D;
    using namespace tinygltf;
    SPDLOG_LOGGER_INFO(get_logger(), "Plume! Node \'{}\'",
                       gltfImporter.object3DName(id));

    // Get mesh data
    Pointer<ObjectData3D> rootNode = gltfImporter.object3D(id);
    Magnum::Int meshID = gltfImporter.object3D(rootNode->children()[0])->instance();
    std::string meshName = string_concat(resPrefix, gltfImporter.meshName(meshID));

    // Get shader params from extras
    Node const& node = *static_cast<Node const*>(
        gltfImporter.object3D(id)->importerState());

    Value const& extras = node.extras;
    float flowVel = extras.Get("flowvelocity").Get<double>();
    Magnum::Color4 color;
    Value::Array colorArray = extras.Get("color").Get<Value::Array>();
    for (int i = 0; i < 4; i++)
    {
        Value const& component = colorArray[i];
        color[i] = component.GetNumberAsDouble();
    }
    float zMax = extras.Get("zMax").Get<double>();
    float zMin = extras.Get("zMin").Get<double>();

    adera::active::PlumeEffectData plumeData;
    plumeData.m_meshName = std::move(meshName);
    plumeData.m_flowVelocity = flowVel;
    plumeData.m_color = color;
    plumeData.m_zMax = zMax;
    plumeData.m_zMin = zMin;

    pkg.add<adera::active::PlumeEffectData>(gltfImporter.object3DName(id), std::move(plumeData));
}

/* Explanation of resPrefix:
   When a mesh is created in blender, the object itself has a name (the one that
   shows up in the scene hierarchy), but the underlying mesh data within that
   object actually has a unique name, usually the name of the primitive that was
   used initially, unless it was changed. These names will be something like
   "Cylinder.004" and are numbered to prevent name collisions within a blend
   file. The issue is that multiple blend files can have a "Cylinder.004" and
   unless the author renames the mesh object itself (which most people never
   touch), when you export the scenes to glTF files and load them both here, you
   end up with a resource key collision due to the repeated name. Passing
   resPrefix around allows us to specify a unique prefix to prepend to the mesh
   name (or any other resource that has the same problem) that is used
   internally to avoid name conflicts.
*/
void osp::AssetImporter::load_sturdy(
        TinyGltfImporter& gltfImporter,
        std::string_view resPrefix,
        Package& rMachinePkg,
        Package& pkg)
{
    SPDLOG_LOGGER_INFO(get_logger(), "Found {} nodes",
                     gltfImporter.object3DCount());

    Optional<SceneData> sceneData = gltfImporter.scene(gltfImporter.defaultScene());
    if (!sceneData)
    {
        SPDLOG_LOGGER_ERROR(get_logger(), "Error: couldn't load scene data");
        return;
    }

    // Loop over and discriminate all top-level nodes
    // Currently, part_* are the only nodes that necessitate special handling
    for (UnsignedInt childID : sceneData->children3D())
    {
        const std::string& nodeName = gltfImporter.object3DName(childID);
        SPDLOG_LOGGER_INFO(get_logger(), "Found node: {}", nodeName);

        if (nodeName.compare(0, 5, "part_") == 0)
        {
            load_part(gltfImporter, rMachinePkg, pkg, childID, resPrefix);
        }
        else if (nodeName.compare(0, 6, "plume_") == 0)
        {
            load_plume(gltfImporter, pkg, childID, resPrefix);
        }
    }

    // Load all associated mesh data
    // Temporary: eventually if would be preferable to retrieve the mesh names only
    for (UnsignedInt i = 0; i < gltfImporter.meshCount(); i++)
    {
        using Magnum::MeshPrimitive;

        std::string const& meshName =
            string_concat(resPrefix, gltfImporter.meshName(i));
        SPDLOG_LOGGER_INFO(get_logger(), "Mesh: {}", meshName);


        Optional<MeshData> meshData = gltfImporter.mesh(i);
        if (!meshData || (meshData->primitive() != MeshPrimitive::Triangles))
        {
            SPDLOG_LOGGER_ERROR(get_logger(),
                             "Error: Mesh {} not composed of triangles",
                             meshName);
            continue;
        }
        pkg.add<MeshData>(meshName, std::move(*meshData));
    }

    // Load all associated image data
    // Temporary: eventually it would be preferable to retrieve the URIs only
    for (UnsignedInt i = 0; i < gltfImporter.textureCount(); i++)
    {
        auto imgID = gltfImporter.texture(i)->image();
        std::string const& imgName = gltfImporter.image2DName(imgID);
        SPDLOG_LOGGER_INFO(get_logger(), "Loading image: {}", imgName);

        Optional<ImageData2D> imgData = gltfImporter.image2D(imgID);
        if (!imgData)
        {
            continue;
        }
        pkg.add<ImageData2D>(imgName, std::move(*imgData));
    }
}

DependRes<ImageData2D> osp::AssetImporter::load_image(
    const std::string& filepath, Package& pkg)
{
    using Magnum::Trade::AbstractImporter;
    using Corrade::PluginManager::Manager;
    using Corrade::Containers::Pointer;

    Manager<AbstractImporter> manager;
    Pointer<AbstractImporter> importer
        = manager.loadAndInstantiate("AnyImageImporter");
    if (!importer || !importer->openFile(filepath))
    {
        SPDLOG_LOGGER_ERROR(get_logger(), "Could not open file {}", filepath);
        return DependRes<ImageData2D>();
    }

    Optional<ImageData2D> image = importer->image2D(0);
    if (!image)
    {
        SPDLOG_LOGGER_ERROR(get_logger(), "Could not read image in file {}",
                         filepath);
        return DependRes<ImageData2D>();
    }

    return pkg.add<ImageData2D>(filepath, std::move(*image));
}

DependRes<Mesh> osp::AssetImporter::compile_mesh(
    const DependRes<MeshData> meshData, Package& pkg)
{
    using Magnum::GL::Mesh;

    if (meshData.empty())
    {
        SPDLOG_LOGGER_ERROR(get_logger(),
                         "Error: requested MeshData resource \'{}\' not found",
                         meshData.name());
        return {};
    }

    return pkg.add<Mesh>(meshData.name(), Magnum::MeshTools::compile(*meshData));
}

DependRes<Magnum::GL::Mesh> AssetImporter::compile_mesh(
    std::string_view meshDataName, Package& srcPackage, Package& dstPackage)
{
    DependRes<MeshData> meshData = srcPackage.get<MeshData>(meshDataName);
    if (meshData.empty())
    {
        return {};
    }
    return compile_mesh(meshData, dstPackage);
}

DependRes<Texture2D> osp::AssetImporter::compile_tex(
    const DependRes<ImageData2D> imageData, Package& package)
{
    using Magnum::GL::SamplerWrapping;
    using Magnum::GL::SamplerFilter;
    using Magnum::GL::textureFormat;

    if (imageData.empty())
    {
        SPDLOG_LOGGER_ERROR(
          get_logger(),
          "Error: requested ImageData2D resource \'{}\' not found\n",
          imageData.name());
        return {};
    }

    Magnum::ImageView2D view = *imageData;

    Magnum::GL::Texture2D tex;
    tex.setWrapping(SamplerWrapping::ClampToEdge)
        .setMagnificationFilter(SamplerFilter::Linear)
        .setMinificationFilter(SamplerFilter::Linear)
        .setStorage(1, textureFormat((*imageData).format()), (*imageData).size())
        .setSubImage(0, {}, view);

    return package.add<Texture2D>(imageData.name(), std::move(tex));
}

DependRes<Magnum::GL::Texture2D> AssetImporter::compile_tex(
    std::string_view imageDataName, Package& srcPackage, Package& dstPackage)
{
    DependRes<ImageData2D> imgData = srcPackage.get<ImageData2D>(imageDataName);
    if (imgData.empty())
    {
        return {};
    }
    return compile_tex(imgData, dstPackage);
}

void AssetImporter::proto_add_obj_recurse(
        TinyGltfImporter& gltfImporter,
        Package& rMachinePkg,
        Package& rPackage,
        std::string_view resPrefix,
        PrototypePart& rPart,
        PartEntity_t parentProtoIndex,
        UnsignedInt childGltfIndex)
{
    using Corrade::Containers::Pointer;
    using Corrade::Containers::Optional;
    using Magnum::Trade::ObjectData3D;
    using Magnum::Trade::MeshObjectData3D;
    using Magnum::Trade::ObjectInstanceType3D;
    using Magnum::Trade::MaterialData;
    using Magnum::Trade::MaterialType;
    using Magnum::Trade::PbrMetallicRoughnessMaterialData;

    PartEntity_t entity = rPart.m_entityCount++;

    // Add the object to the prototype
    Pointer<ObjectData3D> childData = gltfImporter.object3D(childGltfIndex);

    PCompHierarchy &rHier = rPart.m_partHier.emplace_back();
    rHier.m_parent = parentProtoIndex;
    rHier.m_childCount = childData->children().size();

    PCompTransform &rTransform = rPart.m_partTransform.emplace_back();
    rTransform.m_translation = childData->translation();
    rTransform.m_rotation = childData->rotation();
    rTransform.m_scale = childData->scaling();

    const std::string& name = gltfImporter.object3DName(childGltfIndex);
    PCompName &rName = rPart.m_partName.emplace_back();
    rName.m_entity = entity;
    rName.m_name = name;

    if (0 == name.compare(0, 4, "col_"))
    {
        // Part is collider

        // do some stuff here
        tinygltf::Node const& node = *static_cast<tinygltf::Node const*>(
            childData->importerState());
        tinygltf::Value const& extras = node.extras;

        // TODO: Add support different collider shapes here!
        std::string const& shapeName = extras.Get("shape").Get<std::string>();
        // change this to some map too
        const phys::EShape shape = (shapeName == "cylinder")
            ? phys::EShape::Cylinder : phys::EShape::Box;

        PCompPrimativeCollider &rCollider = rPart.m_partCollider.emplace_back();
        rCollider.m_entity = entity;
        rCollider.m_shape = shape;

    }

    int meshID = childData->instance();
    bool hasMesh = (childData->instanceType() == ObjectInstanceType3D::Mesh
                    && meshID != -1);

    if (hasMesh)
    {
        // It's a drawable mesh
        const std::string& meshName =
            string_concat(resPrefix, gltfImporter.meshName(meshID));

        SPDLOG_LOGGER_INFO(get_logger(), "obj: {} use mesh: {}", name,
                           meshName);

        PCompDrawable &rDrawable = rPart.m_partDrawable.emplace_back();
        rDrawable.m_entity = entity;
        rDrawable.m_mesh = rPackage.get_or_reserve<MeshData>(meshName);

        MeshObjectData3D& mesh = static_cast<MeshObjectData3D&>(*childData);
        Optional<MaterialData> mat = gltfImporter.material(mesh.material());

        if (mat->types() & MaterialType::PbrMetallicRoughness)
        {
            const auto& pbr = mat->as<PbrMetallicRoughnessMaterialData>();

            auto imgID = gltfImporter.texture(pbr.baseColorTexture())->image();
            std::string const& imgName = gltfImporter.image2DName(imgID);

            SPDLOG_LOGGER_INFO(get_logger(), "Base Tex: {}", imgName);
            rDrawable.m_textures.emplace_back(rPackage.get_or_reserve<ImageData2D>(imgName));

            if (pbr.hasNoneRoughnessMetallicTexture())
            {
                imgID = gltfImporter.texture(pbr.metalnessTexture())->image();

                SPDLOG_LOGGER_INFO(get_logger(), "Metal/rough texture: {}", gltfImporter.image2DName(imgID));
            }
            else
            {
              SPDLOG_LOGGER_WARN(get_logger(),
                                 "No Metal/rough texture found for: {}", name);
            }
            
        } else
        {
            SPDLOG_LOGGER_ERROR(get_logger(), "Unsupported material type");
        }
    }

    // Check for and read machines
    tinygltf::Node const& node =
        *static_cast<tinygltf::Node const*>(childData->importerState());
    if (node.extras.Has("machines"))
    {
        proto_load_machines(entity, rMachinePkg, node.extras, rPart.m_protoMachines);
    }

    for (UnsignedInt childId: childData->children())
    {
        proto_add_obj_recurse(
                gltfImporter, rMachinePkg, rPackage, resPrefix, rPart, entity, childId);
    }
}

}

