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

    static void load_sturdy_file(std::string const& filepath, Package& package);

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
private:
    /**
     * Load only associated config files, and add resource paths to the package
     * But for now, this function just loads everything.
     *
     * @param gltfImporter [in] glTF importer referencing opened sturdy file
     * @param package [out] Package to put resource paths into
     */
    static void load_sturdy(TinyGltfImporter& gltfImporter, Package& package);

    /**
     * Load a part from a sturdy
     *
     * Reads the config from the node with the specified ID into a PrototypePart
     * and stores it in the specified package
     *
     * @param gltfImpoter [in] importer used to read node data
     * @param package [out] package which receives loaded data
     * @param id [in] ID of node containing part information
     */
    static void load_part(TinyGltfImporter& gltfImporter,
        Package& package, unsigned id);

    static void proto_add_obj_recurse(TinyGltfImporter& gltfImporter,
                               Package& package,
                               PrototypePart& part,
                               unsigned parentProtoIndex,
                               unsigned childGltfIndex);

};

}
