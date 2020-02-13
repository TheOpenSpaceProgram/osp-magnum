#pragma once

#include <string>

#include <MagnumPlugins/TinyGltfImporter/TinyGltfImporter.h>

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
     * Determine which nodes are parts, and add them to the passed vector
     * @param parts Vector to add part data to
     */
    void obtain_parts(std::vector<PartPrototype> parts);

    void open_filepath(const std::string& filepath);

    void close();

    TinyGltfImporter& get_gltf_importer()
    {
        return m_gltfImporter;
    }

    //void debug_add_obj_recurse(Object3D &parent, unsigned id);

    //void dump_nodes(Scene3D& nodeDump);

    //PartPrototype load_parts(int index)
    void load_parts(int index, PartPrototype& out);

private:

    Magnum::Trade::TinyGltfImporter m_gltfImporter;

};

}
