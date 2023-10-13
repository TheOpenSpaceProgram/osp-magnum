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
#pragma once

#include "VehicleBuilder.h"

#include <osp/activescene/vehicles.h>
#include <osp/activescene/prefab_fn.h>

namespace adera
{

struct ACtxVehicleSpawnVB
{
    using SpVehicleId = osp::active::SpVehicleId;

    // Remap vectors convert IDs from VehicleData to ACtxParts.
    // A single vector for remaps is shared for all vehicles to spawn,
    // so offsets are used to divide up the vector.

    // PartId srcPart = /* ID from VehicleData */
    // PartId dstPart = remapParts[remapPartOffsets[newVehicleIndex] + srcPart];

    inline Corrade::Containers::StridedArrayView2D<std::size_t> remap_node_offsets_2d() noexcept
    {
        return {Corrade::Containers::arrayView(remapNodeOffsets.data(), remapNodeOffsets.size()),
                {dataVB.size(), osp::link::NodeTypeReg_t::size()}};
    }

    inline Corrade::Containers::StridedArrayView2D<std::size_t const> remap_node_offsets_2d() const noexcept
    {
        return {Corrade::Containers::arrayView(remapNodeOffsets.data(), remapNodeOffsets.size()),
                {dataVB.size(), osp::link::NodeTypeReg_t::size()}};
    }

    osp::KeyedVec<SpVehicleId, VehicleData const*> dataVB;

    std::vector<osp::active::PartId>        remapParts;
    osp::KeyedVec<SpVehicleId, std::size_t> remapPartOffsets;

    std::vector<osp::active::PartId>        remapWelds;
    osp::KeyedVec<SpVehicleId, std::size_t> remapWeldOffsets;

    std::vector<std::size_t>                machtypeCount;
    std::vector<osp::link::MachAnyId>       remapMachs;
    osp::KeyedVec<SpVehicleId, std::size_t> remapMachOffsets;

    // remapNodes are both shared between all new vehicles and all node types
    // An offset can exist for each pair of [New Vehicle, Node Type]
    std::vector<osp::link::NodeId>          remapNodes;
    osp::KeyedVec<SpVehicleId, std::size_t> remapNodeOffsets;
};

class SysVehicleSpawnVB
{
    using ACtxParts         = osp::active::ACtxParts;
    using ACtxPrefabs       = osp::active::ACtxPrefabs;
    using ACtxVehicleSpawn  = osp::active::ACtxVehicleSpawn;
    using Resources         = osp::Resources;
public:
    static void create_parts_and_welds(ACtxVehicleSpawn& rVehicleSpawn, ACtxVehicleSpawnVB& rVehicleSpawnVB, ACtxParts& rScnParts);

    static void request_prefabs(ACtxVehicleSpawn& rVehicleSpawn, ACtxVehicleSpawnVB const& rVehicleSpawnVB, ACtxParts& rScnParts, ACtxPrefabs& rPrefabs, Resources& rResources);
};


} // namespace adera
