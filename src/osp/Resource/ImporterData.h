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

#include <Magnum/Trade/SceneData.h>
#include <Magnum/Trade/MaterialData.h>

#include <vector>

namespace osp
{

struct TextureImgSource : public ResIdOwner_t { };

struct ImporterData
{
    std::vector<ResIdOwner_t> m_images;
    std::vector<ResIdOwner_t> m_textures;
    std::vector<ResIdOwner_t> m_meshes;
    std::vector<Magnum::Trade::MaterialData> m_materials;

    std::vector<std::string_view> m_nodeNames;
    std::vector<int> m_nodeMeshes;

    std::vector<Magnum::Trade::SceneData> m_scenes;
};

} // namespace osp
