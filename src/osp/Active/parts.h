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

#include "activetypes.h"

#include "../Resource/resourcetypes.h"
#include "../types.h"
#include "../id_map.h"

#include "../link/machines.h"

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
    std::vector<PrefabPair>                         m_partPrefabs;
    std::vector<WeldId>                             m_partToWeld;
    std::vector<Matrix4>                            m_partTransformWeld;
    MapPartToMachines_t                             m_partToMachines;
    std::vector<WeldId>                             m_partDirty;

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

using NewVehicleId  = uint32_t;
using NewPartId     = uint32_t;
using NewWeldId     = uint32_t;

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
        return m_newVhBasicIn.size();
    }

    std::vector<TmpToInit>          m_newVhBasicIn;
    std::vector<NewPartId>          m_newVhPartOffsets;
    std::vector<NewWeldId>          m_newVhWeldOffsets;

    std::vector<PartId>             m_newPartToPart;
    std::vector<uint32_t>           m_newPartPrefabs;

    std::vector<NewPartId>          m_partToNewPart;

    std::vector<WeldId>             m_newWeldToWeld;
    std::vector<ActiveEnt>          m_newWeldToEnt;

    std::vector<link::MachAnyId>    m_newMachToMach;
};

} // namespace osp::active
