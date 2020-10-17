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

    Corrade::PluginManager::Manager<Magnum::Trade::AbstractImporter> m_pluginManager;
    Magnum::Trade::TinyGltfImporter m_gltfImporter;
    Magnum::Trade::StbImageImporter m_imgImporter;
};

}
