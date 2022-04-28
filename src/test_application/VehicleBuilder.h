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

#include "osp/Resource/resourcetypes.h"
#include "osp/types.h"

#include <longeron/id_management/registry.hpp>

#include <entt/container/dense_hash_map.hpp>

#include <cstdint>
#include <array>
#include <vector>

namespace testapp
{

enum class AttachId : uint32_t { };
enum class PartId : uint32_t { };

struct StructureLink
{
    PartId m_greater;
    PartId m_less;
};

struct VehicleData
{
    lgrn::IdRegistry<PartId>            m_partIds;
    std::vector<osp::Matrix4>           m_partTransforms;
    std::vector<osp::PrefabPair>        m_partPrefabs;

    lgrn::IdRegistry<AttachId>          m_attachments;
    std::vector<StructureLink>          m_attachLinks;
};

/**
 * Used to easily create Vehicle blueprints
 */
class VehicleBuilder
{
public:

    VehicleBuilder(osp::Resources *pResources)
     : m_pResources{pResources}
    {
        index_prefabs();
    };

    ~VehicleBuilder();

    template <std::size_t N>
    [[nodiscard]] std::array<PartId, N> create_parts();

    struct SetPrefab
    {
        PartId m_part;
        std::string_view m_prefabName;
    };

    void set_prefabs(std::initializer_list<SetPrefab> setPrefab);

    struct SetTransform
    {
        PartId m_part;
        osp::Matrix4 const& m_transform;
    };

    void set_transform(std::initializer_list<SetTransform> setTransform);

    osp::Matrix4 align_attach(PartId partA, std::string_view attachA,
                              PartId partB, std::string_view attachB);

    VehicleData const& data() const noexcept { return m_data; }

private:

    void index_prefabs();

    osp::Resources *m_pResources;

    entt::dense_hash_map< std::string_view, osp::PrefabPair > m_prefabs;

    VehicleData m_data;

    // put more attachment data here
};


template <std::size_t N>
[[nodiscard]] std::array<PartId, N> VehicleBuilder::create_parts()
{
    std::array<PartId, N> out;
    m_data.m_partIds.create(std::begin(out), N);

    m_data.m_partPrefabs.resize(m_data.m_partIds.capacity());
    m_data.m_partTransforms.resize(m_data.m_partIds.capacity());

    return out;
}

} // namespace testapp
