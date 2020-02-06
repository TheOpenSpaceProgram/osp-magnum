#pragma once

#include <string>

#include "PartPrototype.h"

namespace osp
{


class SturdyImporter
{


public:
    SturdyImporter();


    void open_filepath(const std::string& filepath);

    void close();

    void load_scene();

    //PartPrototype load_parts(int index)
    void load_parts(int index, PartPrototype& out);



};

}
