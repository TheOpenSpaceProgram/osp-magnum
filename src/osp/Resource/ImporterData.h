/**
 * Open Space Program
 * Copyright © 2019-2022 Open Space Program Project
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

#include "../types.h"
#include "resourcetypes.h"

#include <Magnum/Trade/MaterialData.h>

#include <Corrade/Containers/Optional.h>

#include <longeron/containers/intarray_multimap.hpp>

#include <string_view>
#include <vector>

namespace osp
{

struct TextureImgSource : public ResIdOwner_t { };

struct ImporterData
{
    using OptMaterialData_t = Corrade::Containers::Optional<Magnum::Trade::MaterialData>;

    // Owned resources
    std::vector<ResIdOwner_t>           m_images;
    std::vector<ResIdOwner_t>           m_textures;
    std::vector<ResIdOwner_t>           m_meshes;

    std::vector<OptMaterialData_t>      m_materials;

    // [sceneId][child]
    lgrn::IntArrayMultiMap<int, int>    m_scnTopLevel;

    // Object data
    // note: terminology for 'things' vary
    // * Magnum: Object       * glTF: Node       * OSP & EnTT: Entity

    std::vector<int>                    m_objParents;
    lgrn::IntArrayMultiMap<int, int>    m_objChildren;

    std::vector<std::string_view>       m_objNames;
    std::vector<Matrix4>                m_objTransforms;

    std::vector<int>                    m_objMeshes;
    std::vector<int>                    m_objMaterials;
};

} // namespace osp
