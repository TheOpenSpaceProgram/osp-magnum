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

#include "active_ent.h"

#include "../core/array_view.h"
#include "../core/keyed_vector.h"
#include "../core/math_types.h"
#include "../link/machines.h"
#include "../vehicles/prefabs.h"

#include <longeron/id_management/registry_stl.hpp>
#include <longeron/containers/intarray_multimap.hpp>

namespace osp::active
{

using PartId = uint32_t;
using WeldId = uint32_t;

struct Parts
{
    using MapPartToMachines_t = lgrn::IntArrayMultiMap<PartId, link::MachinePair>;

    lgrn::IdRegistryStl<PartId>                     m_partIds;
    KeyedVec<PartId, PrefabPair>                    m_partPrefabs;
    KeyedVec<PartId, WeldId>                        m_partToWeld;
    KeyedVec<PartId, Matrix4>                       m_partTransformWeld;
    MapPartToMachines_t                             m_partToMachines;
    std::vector<PartId>                             m_partDirty;

    lgrn::IdRegistryStl<WeldId>                     m_weldIds;
    lgrn::IntArrayMultiMap<WeldId, PartId>          m_weldToParts;
    std::vector<WeldId>                             m_weldDirty;

    link::Machines                                  m_machines;
    std::vector<PartId>                             m_machineToPart;
    std::vector<link::Nodes>                        m_nodePerType;
};

struct ACtxParts : Parts
{
    std::vector<ActiveEnt>                          m_partToActive;
    std::vector<PartId>                             m_activeToPart;
    std::vector<ActiveEnt>                          m_weldToEnt;
};

using SpVehicleId = StrongId<uint32_t, struct DummyForSpVehicleId>;
using SpPartId    = StrongId<uint32_t, struct DummyForSpPartId>;
using SpWeldId    = StrongId<uint32_t, struct DummyForSpWeldId>;
using SpMachAnyId = StrongId<uint32_t, struct DummyForSpMachAnyId>;

struct ACtxVehicleSpawn
{
    struct TmpToInit
    {
        Vector3    m_position;
        Vector3    m_velocity;
        Quaternion m_rotation;
    };

    std::size_t new_vehicle_count() const noexcept
    {
        return spawnRequest.size();
    }

    KeyedVec<SpVehicleId, TmpToInit>        spawnRequest;

    KeyedVec<SpPartId, PartId>              spawnedParts;
    KeyedVec<SpVehicleId, SpPartId>         spawnedPartOffsets;
    KeyedVec<PartId, SpPartId>              partToSpawned;
    KeyedVec<SpPartId, uint32_t>            spawnedPrefabs;

    KeyedVec<SpWeldId, WeldId>              spawnedWelds;
    KeyedVec<SpVehicleId, SpWeldId>         spawnedWeldOffsets;
    KeyedVec<SpWeldId, ActiveEnt>           rootEnts;

    KeyedVec<SpMachAnyId, link::MachAnyId>  m_newMachToMach;
};

} // namespace osp::active
