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
#include "VehicleBuilder.h"

#include <osp/Resource/ImporterData.h>
#include <osp/Resource/resources.h>
#include <osp/logging.h>

namespace testapp
{

using osp::restypes::gc_importer;

VehicleBuilder::~VehicleBuilder()
{
    // clear resource owners
    for (auto & [_, value] : m_prefabs)
    {
        m_pResources->owner_destroy(gc_importer, std::move(value.m_importer));
    }

    for (osp::PrefabPair &rPrefabPair : m_data.m_partPrefabs)
    {
        m_pResources->owner_destroy(gc_importer, std::move(rPrefabPair.m_importer));
    }
}

void VehicleBuilder::set_prefabs(std::initializer_list<SetPrefab> setPrefabs)
{
    for (SetPrefab const& set : setPrefabs)
    {
        auto const foundIt = m_prefabs.find(set.m_prefabName);
        if (foundIt != std::end(m_prefabs))
        {
            auto &rPrefabPair = m_data.m_partPrefabs[std::size_t(set.m_part)];
            rPrefabPair.m_prefabId = foundIt->second.m_prefabId;
            rPrefabPair.m_importer = m_pResources->owner_create(gc_importer, foundIt->second.m_importer);
        }
        else
        {
            OSP_LOG_WARN("Prefab {} not found!", set.m_prefabName);
        }
    }
}

void VehicleBuilder::set_transform(std::initializer_list<SetTransform> setTransform)
{
    for (SetTransform const& set : setTransform)
    {
        m_data.m_partTransforms[std::size_t(set.m_part)] = set.m_transform;
    }
}

void VehicleBuilder::index_prefabs()
{
    for (unsigned int i = 0; i < m_pResources->ids(gc_importer).capacity(); ++i)
    {
        auto resId = osp::ResId(i);
        if ( ! m_pResources->ids(gc_importer).exists(resId))
        {
            continue;
        }

        auto const *pPrefabData = m_pResources->data_try_get<osp::Prefabs>(gc_importer, resId);

        if (pPrefabData == nullptr)
        {
            continue; // No prefab data
        }

        for (int j = 0; j < pPrefabData->m_prefabNames.size(); ++j)
        {
            osp::ResIdOwner_t owner = m_pResources->owner_create(gc_importer, resId);

            m_prefabs.emplace(pPrefabData->m_prefabNames[j],
                              osp::PrefabPair{std::move(owner), j});
        }
    }
}


} // namespace testapp
