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

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

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
    AssetImporter() { }

    static void load_sturdy_file(std::string_view filepath,
                                 Package& rMachinePkg, Package& package);

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
     * Load machines from node extras
     * 
     * Each node in the glTF tree may possess machines, but only the root
     * PrototypePart stores them. The PrototypePart's machineArray is passed,
     * alongside the current node (object)'s machineIndexArray.
     * PrototypeMachines are added to the PrototypePart's master list, and the
     * index of each machine is added to the machineIndexArray so that the
     * current node/object can keep track of which machines belong to it.
     *
     * @param extras            [in] An extras node from a glTF file
     * @param machineArray      [out] A machine array from a PrototypePart
     * 
     * @return A machineIndexArray which is used by PrototypeObjects to store
     * the indices of the machineArray elements which belong to it
     */
    static void proto_load_machines(
            Package& rMachinePackage,
            tinygltf::Value const& extras,
            std::vector<PrototypeMachine>& rMachines);
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
            std::string_view resPrefix, Package& rMachinePackage, Package& package);

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
    static void load_part(
            TinyGltfImporter& gltfImporter,
            Package& rMachinePackage,
            Package& package,
            Magnum::UnsignedInt id,
            std::string_view resPrefix);

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

private:
    /**
     * Read glTF node as part of a sturdy Part. Write data into a PrototypePart
     *
     * This function will recurse through all descendents of the node to include
     * them all in the PrototypePart
     *
     * @param gltfImporter     [in] glTF data to read
     * @param machinePackage   [in] Package containing RegisteredMachines
     * @param rPackage         [ref] Package to reserve found resources into
     * @param resPrefix        [in] Prefix of rPackage
     * @param part             [out] Prototy pePart to write data into
     * @param parentProtoIndex [in] PartEntity of current glTF node's parent
     * @param childGltfIndex   [in] Index of current glTF node
     */
    static void proto_add_obj_recurse(
            TinyGltfImporter& gltfImporter,
            Package& machinePackage,
            Package& rPackage,
            std::string_view resPrefix,
            PrototypePart& part,
            PartEntity_t parentProtoIndex,
            Magnum::UnsignedInt childGltfIndex);

    /*
    * This cannot be a reference or else spdlog will complain. Don't know why.
    */
    static const std::shared_ptr<spdlog::logger> get_logger() 
    {
        return spdlog::get("assetimporter");
    }
};

}
