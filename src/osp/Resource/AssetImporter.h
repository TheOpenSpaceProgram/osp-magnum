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
#pragma once

#include <string>

#include <MagnumPlugins/TinyGltfImporter/TinyGltfImporter.h>
#include <MagnumPlugins/StbImageImporter/StbImageImporter.h>
#include <Corrade/PluginManager/Manager.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/MeshData.h>


#include "Package.h"
#include "PrototypePart.h"

#include "../types.h"
//#include "../scene.h"

namespace osp
{

class AssetImporter
{
typedef Magnum::Trade::TinyGltfImporter TinyGltfImporter;
typedef Corrade::PluginManager::Manager<Magnum::Trade::AbstractImporter>
PluginManager;

public:
    AssetImporter() {}

    static void load_sturdy_file(std::string_view filepath, Package& package);

    /**
     * Load an image from disk at the specified filepath
     * 
     * Loads an ImageData2D into the specified package, but does not create
     * a texture in GPU memory until compile_tex() is called
     * @param filepath [in] string filepath of requested image file
     * @param package [out] Package to put image data into
     */
    static DependRes<Magnum::Trade::ImageData2D> load_image(
        const std::string& filepath, Package& package);

    /**
     * Compile MeshData into an OpenGL Mesh object
     *
     * Takes the MeshData object from the package and compiles it into a Mesh
     * which can then be drawn
     * @param meshData [in] MeshData resource
     * @param package [out] Package to put Mesh resource into
     */
    static DependRes<Magnum::GL::Mesh> compile_mesh(
        const DependRes<Magnum::Trade::MeshData> meshData, Package& package);
    static DependRes<Magnum::GL::Mesh> compile_mesh(
        std::string_view meshDataName, Package& srcPackage, Package& dstPackage);

    /**
     * Compile ImageData2D into an OpenGL Texture2D object
     *
     * Takes the ImageData2D object from the package and compiles it into a
     * Texture2D which can then be used by shaders
     * @param imageData [in] ImageData2D resource
     * @param package [out] Package to put Texture2D resource into
     */
    static DependRes<Magnum::GL::Texture2D> compile_tex(
        const DependRes<Magnum::Trade::ImageData2D> imageData, Package& package);
    static DependRes<Magnum::GL::Texture2D> compile_tex(
        std::string_view imageDataName, Package& srcPackage, Package& dstPackage);
private:
    /**
     * Load only associated config files, and add resource paths to the package
     * But for now, this function just loads everything.
     *
     * @param gltfImporter [in] glTF importer referencing opened sturdy file
     * @param resPrefix [in] Unique name associated with the data source,
     *       used to make resource names (e.g. mesh) unique to avoid collisions
     * @param package [out] Package to put resource paths into
     */
    static void load_sturdy(TinyGltfImporter& gltfImporter,
            std::string_view resPrefix, Package& package);

    /**
     * Load a part from a sturdy
     *
     * Reads the config from the node with the specified ID into a PrototypePart
     * and stores it in the specified package
     *
     * @param gltfImpoter [in] importer used to read node data
     * @param package [out] package which receives loaded data
     * @param id [in] ID of node containing part information
     * @param resPrefix [in] Unique prefix for mesh names (see load_sturdy())
     */
    static void load_part(TinyGltfImporter& gltfImporter,
        Package& package, Magnum::UnsignedInt id, std::string_view resPrefix);

    /**
     * Load a plume object from a sturdy
     *
     * Reads the config from the node with the specified ID into a PlumeEffectData
     * and stores it in the specified package
     *
     * @param gltfImpoter [in] importer used to read node data
     * @param package [out] package which receives loaded data
     * @param id [in] ID of node containing plume information
     */
    static void load_plume(TinyGltfImporter& gltfImporter,
        Package& package, Magnum::UnsignedInt id, std::string_view resPrefix);

    static void proto_add_obj_recurse(TinyGltfImporter& gltfImporter,
                               Package& package,
                               std::string_view resPrefix,
                               PrototypePart& part,
                               Magnum::UnsignedInt parentProtoIndex,
                               Magnum::UnsignedInt childGltfIndex);

};

}
