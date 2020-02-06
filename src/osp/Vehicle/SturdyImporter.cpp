#include <iostream>


#include <Corrade/PluginManager/Manager.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <MagnumPlugins/TinyGltfImporter/TinyGltfImporter.h>

#include "SturdyImporter.h"

namespace osp
{

SturdyImporter::SturdyImporter()
{

    //Magnum::PluginManager::Manager<Magnum::Trade::AbstractImporter> manager;
    //Magnum::Containers::Pointer<Magnum::Trade::AbstractImporter> importer =
    //manager.loadAndInstantiate("GltfImporter");
    Magnum::Trade::TinyGltfImporter tgltfi;


}


void SturdyImporter::open_filepath(const std::string& filepath)
{

}

void SturdyImporter::close()
{

}

void SturdyImporter::load_scene()
{

}

void SturdyImporter::load_parts(int index, PartPrototype& out)
{

}

}

