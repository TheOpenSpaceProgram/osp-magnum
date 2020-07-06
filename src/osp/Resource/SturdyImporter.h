#pragma once

#include <string>

#include <MagnumPlugins/TinyGltfImporter/TinyGltfImporter.h>

#include "Package.h"
#include "PrototypePart.h"

#include "../types.h"
//#include "../scene.h"

namespace osp
{

class SturdyImporter
{

typedef Magnum::Trade::TinyGltfImporter TinyGltfImporter;


public:
    SturdyImporter();

    void open_filepath(const std::string& filepath);

    void close();

    TinyGltfImporter& get_gltf_importer()
    {
        return m_gltfImporter;
    }

    /**
     * Load all the sturdy data right away
     * @param rPackage [out] Package to dump loaded data into
     */
    void load_all(Package& rPackage);

    /**
     * Load only associated config files, and add resource paths to the package
     * But for now, this function just loads everything.
     *
     * @param package [out] Package to put resource paths into
     */
    void load_config(Package& package);

private:

    // When meshes are loaded, they are appended to the list of meshes in
    // the package. This is the index to the first added mesh
    unsigned m_meshOffset;

    void proto_add_obj_recurse(Package& package,
                               PrototypePart& part,
                               unsigned parentProtoIndex,
                               unsigned childGltfIndex);

    Magnum::Trade::TinyGltfImporter m_gltfImporter;

};

}
