#pragma once

#include <string>

#include <MagnumPlugins/TinyGltfImporter/TinyGltfImporter.h>

#include "Package.h"
#include "PartPrototype.h"

#include "../types.h"
#include "../scene.h"

namespace osp
{

class SturdyImporter
{


typedef Magnum::Trade::TinyGltfImporter TinyGltfImporter;




public:
    SturdyImporter();

    /**
     * Determine which nodes are parts, and add them to the passed vector.
     * Only looks through already-loaded data
     * @param parts Vector to add part data to
     */
    //void obtain_parts(std::vector<PartPrototype> parts);

    void open_filepath(const std::string& filepath);

    void close();

    TinyGltfImporter& get_gltf_importer()
    {
        return m_gltfImporter;
    }

    /**
     * Load all data right away
     * @param package
     */
    void load_all(Package& package);

    /**
     * Load only associated config files, and add resource paths to the package
     * But for now, this function just loads everything.
     *
     * @param package [out] Package to put resource paths into
     */
    void load_config(Package& package);


    //void debug_add_obj_recurse(Object3D &parent, unsigned id);
    //void dump_nodes(Scene3D& nodeDump);
    //PartPrototype load_parts(int index)
    //void load_parts(int index, Package& out);


private:

    // When meshes are loaded, they are appended to the list of meshes in
    // the package. This is the index to the first added mesh
    unsigned m_meshOffset;

    void proto_add_obj_recurse(Package& package,
                               PartPrototype& part,
                               unsigned parentProtoIndex,
                               unsigned childGltfIndex);

    Magnum::Trade::TinyGltfImporter m_gltfImporter;

};

}
