/**
 * Open Space Program
 * Copyright © 2019-2022 Open Space Program Project
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

#include "load_tinygltf.h"

#include "ImporterData.h"
#include "resources.h"

#include "../logging.h"
#include "../string_concat.h"

#include <MagnumPlugins/TinyGltfImporter/TinyGltfImporter.h>
#include <MagnumExternal/TinyGltf/tiny_gltf.h>

#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/TextureData.h>
#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/ObjectData3D.h>
#include <Magnum/Trade/MeshObjectData3D.h>
#include <Magnum/Trade/SceneData.h>

#include <Corrade/PluginManager/Manager.h>

using namespace osp;

using Magnum::Trade::TinyGltfImporter;

using Magnum::Trade::ImageData2D;
using Magnum::Trade::TextureData;
using Magnum::Trade::MeshData;
using Magnum::Trade::MeshObjectData3D;
using Magnum::Trade::MaterialData;
using Magnum::Trade::ObjectData3D;
using Magnum::Trade::SceneData;


using Magnum::Trade::ObjectInstanceType3D;

using Magnum::UnsignedInt;

using PluginManager = Corrade::PluginManager::Manager<Magnum::Trade::AbstractImporter>;
using Corrade::Containers::Optional;
using Corrade::Containers::Pointer;

using TinyGltfNodeExtras_t = std::vector<tinygltf::Value>;

void osp::register_tinygltf_resources(Resources &rResources)
{
    rResources.data_register<TinyGltfNodeExtras_t>(restypes::gc_importer);
}

static void load_gltf(TinyGltfImporter &rImporter, ResId res, std::string_view name, Resources &rResources, PkgId pkg)
{
    ImporterData &rImportData = rResources.data_add<ImporterData>(restypes::gc_importer, res);

    // Combine resource names. Maybe make this customizable
    // ie: name = "dir/file.gltf" and resName = "mytexture"
    // "dir/file.gltf:mytexture"
    // "unnamed-[id]" is used as the resource name if it's empty
    auto format_name = [tempStr = std::string{}, name]
            (std::string_view resName, UnsignedInt id) mutable -> std::string const&
    {
        tempStr.clear();
        if ( ! resName.empty())
        {
            string_append(tempStr, name, ":", resName);
        }
        else
        {
            // i don't like std::to_string but screw it, fix it later
            string_append(tempStr, name, ":unnamed-", std::to_string(id));
        }

        return tempStr;
    };

    using namespace restypes;

    // Store images
    rImportData.m_images.resize(rImporter.image2DCount());
    for (UnsignedInt i = 0; i < rImporter.image2DCount(); i ++)
    {
        Optional<ImageData2D> img = rImporter.image2D(i);

        if ( ! bool(img) )
        {
            continue;
        }

        // Create and keep track of resource Id
        ResId const imgRes = rResources.create(gc_image, pkg, format_name(rImporter.image2DName(i), i));
        rImportData.m_images[i] = rResources.owner_create(gc_image, imgRes);

        // Add image data to resource
        rResources.data_add<ImageData2D>(gc_image, imgRes, std::move(*img));
    }

    // Store textures
    rImportData.m_textures.resize(rImporter.textureCount());
    for (UnsignedInt i = 0; i < rImporter.textureCount(); i ++)
    {
        Optional<TextureData> tex = rImporter.texture(i);

        if ( ! bool(tex) )
        {
            continue;
        }

        // Create and keep track of resource Id
        ResId const texRes = rResources.create(gc_texture, pkg, format_name(rImporter.textureName(i), i));
        rImportData.m_textures[i] = rResources.owner_create(gc_texture, texRes);

        // Add data to resource
        rResources.data_add<TextureData>(gc_texture, texRes, std::move(*tex));

        // Keep track of which image this texture uses
        if (ResIdOwner_t const& imgRes = rImportData.m_images.at(tex->image());
            imgRes.has_value())
        {
            ResIdOwner_t imgOwner = rResources.owner_create(gc_image, imgRes);
            rResources.data_add<TextureImgSource>(gc_texture, texRes, TextureImgSource{std::move(imgOwner)} );
        }
    }

    // Store meshes
    rImportData.m_meshes.resize(rImporter.meshCount());
    for (UnsignedInt i = 0; i < rImporter.meshCount(); i ++)
    {
        Optional<MeshData> mesh = rImporter.mesh(i);

        if ( ! bool(mesh) )
        {
            continue;
        }

        ResId const meshRes = rResources.create(gc_mesh, pkg, format_name(rImporter.meshName(i), i));
        rResources.data_add<MeshData>(gc_mesh, meshRes, std::move(*mesh));
        rImportData.m_meshes[i] = rResources.owner_create(gc_mesh, meshRes);
    }

    // Store materials
    rImportData.m_materials.reserve(rImporter.materialCount());
    for (UnsignedInt i = 0; i < rImporter.materialCount(); i ++)
    {
        rImportData.m_materials.emplace_back(rImporter.material(i));
    }

    // Store various node data

    auto &rNodeExtras = rResources.data_add<TinyGltfNodeExtras_t>(
                                restypes::gc_importer, res);

    UnsignedInt parentsSet = 0;

    UnsignedInt const objCount = rImporter.object3DCount();
    rNodeExtras                 .resize(objCount);
    rImportData.m_objNames      .resize(objCount);
    rImportData.m_objMeshes     .resize(objCount);
    rImportData.m_objMaterials  .resize(objCount);
    rImportData.m_objTransforms .resize(objCount);
    rImportData.m_objParents    .resize(objCount);
    rImportData.m_objChildren   .ids_reserve(objCount);
    rImportData.m_objChildren   .data_reserve(objCount);

    for (UnsignedInt i = 0; i < objCount; i ++)
    {
        Pointer<ObjectData3D const> obj         = rImporter.object3D(i);
        int const instance                      = obj->instance();
        ObjectInstanceType3D const instanceType = obj->instanceType();

        if ( obj == nullptr )
        {
            continue;
        }

        rImportData.m_objNames[i] = rImporter.object3DName(i);
        rImportData.m_objTransforms[i] = NodeTransform
        {
            obj->translation(), obj->rotation(), obj->scaling()
        };

        // Store hierarchy data

        auto const& children = obj->children();
        for (UnsignedInt childId : children)
        {
            ++parentsSet;
            rImportData.m_objParents[childId] = i;
        }
        rImportData.m_objChildren.emplace(i, children.begin(), children.end());

        // Store drawable data if present

        bool const hasMesh = (instanceType == ObjectInstanceType3D::Mesh)
                             && (instance != -1);

        MeshObjectData3D const *pObjMesh
                = hasMesh ? static_cast<MeshObjectData3D const*>(obj.get())
                          : nullptr;

        rImportData.m_objMeshes[i]     = hasMesh ? instance : -1;
        rImportData.m_objMaterials[i]  = hasMesh ? pObjMesh->material() : -1;

        // Store custom properties

        tinygltf::Node const *pNode
                = static_cast<tinygltf::Node const*>(obj->importerState());
        rNodeExtras[i] = pNode->extras;

    }

    // Store scenes
    rImportData.m_scenes.ids_reserve(rImporter.sceneCount());

    // Use number of unparented nodes as the number of scene-level nodes
    // getting a bit 'clever' here but too bad
    UnsignedInt const unparentedNodes = objCount - parentsSet;
    rImportData.m_scenes.data_reserve(unparentedNodes);

    for (UnsignedInt i = 0; i < rImporter.sceneCount(); i ++)
    {
        Optional<SceneData> const scene = rImporter.scene(i);

        if (bool(scene))
        {
            auto const& children = scene->children3D();
            rImportData.m_scenes.emplace(i, children.begin(), children.end());
        }
        else
        {
            rImportData.m_scenes.emplace(i, {});
        }
    }

    rImportData.m_objNames.resize(objCount);
}

ResId osp::load_tinygltf_file(std::string_view filepath, Resources &rResources, PkgId pkg)
{
    PluginManager pluginManager;

    // Create Importer resource
    ResId const res = rResources.create(restypes::gc_importer, pkg, filepath);
    TinyGltfImporter importer{pluginManager};

    // note: Magnum master branch has a Corrade StringView version of this function
    importer.openFile(std::string{filepath});

    if (!importer.isOpened() || importer.defaultScene() == -1)
    {
        // TODO: delete resource (not yet implemented)
        OSP_LOG_ERROR("Could not open file {}", filepath);
        return lgrn::id_null<ResId>();
    }

    load_gltf(importer, res, filepath, rResources, pkg);

    importer.close();

    return res;
}

