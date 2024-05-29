/**
 * Open Space Program
 * Copyright © 2019-2023 Open Space Program Project
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
#include "vehicles_vb_fn.h"

#include <osp/core/Resources.h>

using namespace osp;
using namespace osp::active;

namespace adera
{

void SysVehicleSpawnVB::create_parts_and_welds(ACtxVehicleSpawn& rVehicleSpawn, ACtxVehicleSpawnVB& rVehicleSpawnVB, ACtxParts& rScnParts)
{
    std::size_t const newVehicleCount = rVehicleSpawn.new_vehicle_count();
    ACtxVehicleSpawnVB &rVSVB = rVehicleSpawnVB;

    rVSVB.remapPartOffsets              .resize(newVehicleCount);
    rVSVB.remapWeldOffsets              .resize(newVehicleCount);
    rVehicleSpawn.spawnedPartOffsets    .resize(newVehicleCount);
    rVehicleSpawn.spawnedWeldOffsets    .resize(newVehicleCount);

    // Count total parts and welds, and calculate offsets for remaps

    uint32_t partTotal       = 0;
    uint32_t remapPartTotal  = 0;

    uint32_t weldTotal       = 0;
    uint32_t remapWeldTotal  = 0;

    for (uint32_t spVhInt = 0; spVhInt < newVehicleCount; ++spVhInt)
    {
        auto const spVhId = SpVehicleId{spVhInt};
        VehicleData const* pVData = rVSVB.dataVB[spVhId];
        if (pVData == nullptr)
        {
            continue;
        }

        rVehicleSpawn.spawnedPartOffsets[spVhId] = SpPartId{partTotal};
        partTotal += pVData->m_partIds.size();

        rVSVB.remapPartOffsets[spVhId] = remapPartTotal;
        remapPartTotal += pVData->m_partIds.capacity();

        rVehicleSpawn.spawnedWeldOffsets[spVhId] = SpWeldId{weldTotal};
        weldTotal += pVData->m_weldIds.size();

        rVSVB.remapWeldOffsets[spVhId] = remapWeldTotal;
        remapWeldTotal += pVData->m_weldIds.capacity();
    }

    // Resize containers for new IDs

    rVehicleSpawn.spawnedParts      .resize(partTotal);
    rVehicleSpawn.spawnedWelds      .resize(weldTotal);
    rVehicleSpawn.spawnedPrefabs    .resize(partTotal);
    rVSVB.remapParts                .resize(remapPartTotal, lgrn::id_null<PartId>());
    rVSVB.remapWelds                .resize(remapPartTotal, lgrn::id_null<WeldId>());

    // Create new Scene PartIds and WeldIds

    rScnParts.partIds.create(rVehicleSpawn.spawnedParts.begin(), rVehicleSpawn.spawnedParts.end());
    rScnParts.weldIds.create(rVehicleSpawn.spawnedWelds.begin(), rVehicleSpawn.spawnedWelds.end());

    rScnParts.partDirty.insert(rScnParts.partDirty.begin(), rVehicleSpawn.spawnedParts.begin(), rVehicleSpawn.spawnedParts.end());
    rScnParts.weldDirty.insert(rScnParts.weldDirty.begin(), rVehicleSpawn.spawnedWelds.begin(), rVehicleSpawn.spawnedWelds.end());

    // Resize scene containers to account for new IDs

    std::size_t const maxParts = rScnParts.partIds.capacity();
    std::size_t const maxWelds = rScnParts.weldIds.capacity();
    rScnParts.partPrefabs           .resize(maxParts);
    rScnParts.partTransformWeld     .resize(maxParts);
    rScnParts.partToWeld            .resize(maxParts);
    rScnParts.weldToParts           .data_reserve(maxParts);
    rScnParts.weldToParts           .ids_reserve(maxWelds);
    rScnParts.weldToActive          .resize(maxWelds);
    rVehicleSpawn.partToSpawned     .resize(maxParts);

    // Populate "Scene PartId -> NewPartId" map
    for (uint32_t spPartInt = 0; spPartInt < rVehicleSpawn.spawnedParts.size(); ++spPartInt)
    {
        auto const spPart = SpPartId{spPartInt};
        rVehicleSpawn.partToSpawned[rVehicleSpawn.spawnedParts[spPart]] = spPart;
    }

    // Populate remap vectors and set weld connections

    auto itDstPartIds = rVehicleSpawn.spawnedParts.cbegin();
    auto itDstWeldIds = rVehicleSpawn.spawnedWelds.cbegin();

    for (uint32_t spVhInt = 0; spVhInt < newVehicleCount; ++spVhInt)
    {
        auto const spVhId = SpVehicleId{spVhInt};

        VehicleData const* pVData = rVSVB.dataVB[spVhId];
        if (pVData == nullptr)
        {
            continue;
        }

        std::size_t const remapPartOffset = rVSVB.remapPartOffsets[spVhId];
        std::size_t const remapWeldOffset = rVSVB.remapWeldOffsets[spVhId];

        for (PartId const srcPart : pVData->m_partIds)
        {
            PartId const dstPart = *itDstPartIds;
            ++itDstPartIds;

            // Populate map for "VehicleBuilder PartId -> ACtxParts PartId"
            rVSVB.remapParts[remapPartOffset + srcPart] = dstPart;
        }

        for (WeldId const srcWeld : pVData->m_weldIds)
        {
            WeldId const dstWeld = *itDstWeldIds;
            ++itDstWeldIds;

            // Populate map for "VehicleBuilder WeldId -> ACtxParts WeldId"
            // rVehicleSpawnVB.remapWelds
            rVSVB.remapWelds[remapWeldOffset + srcWeld] = dstWeld;

            // Use remaps to connect ACtxParts WeldIds and PartIds
            // rScnParts.m_partToWeld and rScnParts.m_weldToParts

            auto const srcWeldPartSpan  = pVData->m_weldToParts[srcWeld];
            WeldId *pDstWeldPartOut     = rScnParts.weldToParts.emplace(dstWeld, srcWeldPartSpan.size());

            for (PartId const srcPart : srcWeldPartSpan)
            {
                PartId const dstPart = rVSVB.remapParts[remapPartOffset + srcPart];

                (*pDstWeldPartOut) = dstPart;
                std::advance(pDstWeldPartOut, 1);

                rScnParts.partToWeld[dstPart] = dstWeld;
            }
        }
    }
}


void SysVehicleSpawnVB::request_prefabs(ACtxVehicleSpawn& rVehicleSpawn, ACtxVehicleSpawnVB const& rVehicleSpawnVB, ACtxParts& rScnParts, ACtxPrefabs& rPrefabs, Resources& rResources)
{
    std::size_t const newVehicleCount = rVehicleSpawn.new_vehicle_count();

    auto itDstPartIds = rVehicleSpawn.spawnedParts.cbegin();
    auto itPrefabOut  = rVehicleSpawn.spawnedPrefabs.begin();

    for (uint32_t spVhInt = 0; spVhInt < newVehicleCount; ++spVhInt)
    {
        auto const spVhId = SpVehicleId{spVhInt};

        VehicleData const* pVData = rVehicleSpawnVB.dataVB[spVhId];
        if (pVData == nullptr)
        {
            continue;
        }

        // Copy Part data from VehicleBuilder to scene
        for (uint32_t srcPart : pVData->m_partIds)
        {
            PartId const dstPart = *itDstPartIds;
            ++itDstPartIds;

            PrefabPair const& prefabPairSrc = pVData->m_partPrefabs[srcPart];
            PrefabPair prefabPairDst{
                rResources.owner_create(osp::restypes::gc_importer, prefabPairSrc.m_importer),
                prefabPairSrc.m_prefabId
            };
            rScnParts.partPrefabs[dstPart]        = std::move(prefabPairDst);
            rScnParts.partTransformWeld[dstPart]  = pVData->m_partTransformWeld[srcPart];

            // Add Prefab and Part init events
            (*itPrefabOut) = rPrefabs.spawnRequest.size();
            ++itPrefabOut;

            rPrefabs.spawnRequest.push_back(TmpPrefabRequest{
                .m_importerRes = prefabPairSrc.m_importer,
                .m_prefabId = prefabPairSrc.m_prefabId,
                .m_pTransform = &pVData->m_partTransformWeld[srcPart]
            });
        }
    }
}


} // namespace adera
