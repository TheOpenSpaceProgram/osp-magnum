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

public:
    AssetImporter() : m_gltfImporter(m_pluginManager) {}

    /**
     * Load only associated config files, and add resource paths to the package
     * But for now, this function just loads everything.
     *
     * @param filepath [in] string filepath of requested sturdy file
     * @param package [out] Package to put resource paths into
     */
    void load_sturdy(const std::string& filepath, Package& package);

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
     * @param filepath [in] string identifier of MeshData resource
     * @param package [out] Package to put Mesh resource into
     */
    static DependRes<Magnum::GL::Mesh> compile_mesh(
        const std::string& meshDataName, Package& package);

    /**
     * Compile ImageData2D into an OpenGL Texture2D object
     *
     * Takes the ImageData2D object from the package and compiles it into a
     * Texture2D which can then be used by shaders
     * @param filepath [in] string identifier of ImageData2D resource
     * @param package [out] Package to put Texture2D resource into
     */
    static DependRes<Magnum::GL::Texture2D> compile_tex(
        const std::string& imageDataName, Package& package);
private:

    void proto_add_obj_recurse(Package& package,
                               PrototypePart& part,
                               unsigned parentProtoIndex,
                               unsigned childGltfIndex);

    Corrade::PluginManager::Manager<Magnum::Trade::AbstractImporter> m_pluginManager;
    Magnum::Trade::TinyGltfImporter m_gltfImporter;
};

}
