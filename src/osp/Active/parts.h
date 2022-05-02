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

#include "../link/machines.h"

#include <longeron/id_management/registry_stl.hpp>
#include <longeron/containers/intarray_multimap.hpp>

namespace osp::active
{

using PartEnt_t = uint32_t;
using RigidGroup_t = uint32_t;

/**
 * @brief Data to support Parts in a scene
 *
 * 'Part' refers to a more complex physical 'thing' in a scene, such as a
 * rocket engine, fuel tank, or a capsule.
 *
 * ACtxParts provides the following feature:
 * * Basic structural connections using 'RigidGroups'
 * * A physical model for visual and colliders using a Prefab
 *
 * Rigid groups
 * * Parts that are structurally fixed together make a RigidGroup.
 * * Parts within the same rigid group store transforms relative to the same
 *   (arbitrary) origin.
 * * Storing potentially messy 'part-to-part' transforms are not required.
 * * Parts can retain their original transform after separations or other.
 *   structural modifications, preventing precision errors from accumolating
 * * An external system can use RigidGroups to generate physics constraints
 *   or parent together Prefabs.
 *
 * Additional features are added in other structs.
 *
 * Note that unlike the universe, scenes don't have a concept of 'vehicles'.
 */
struct ACtxParts
{
    // Every part that exists is assigned an ID
    lgrn::IdRegistryStl<PartEnt_t>                      m_partIds;

    // Associate with a Prefab
    std::vector<PrefabPair>                             m_partPrefabs;
    std::vector<ActiveEnt>                              m_partToActive;
    std::vector<PartEnt_t>                              m_activeToPart;

    // Rigid Groups
    lgrn::IdRegistryStl<RigidGroup_t>                   m_rigidIds;
    lgrn::IntArrayMultiMap<RigidGroup_t, PartEnt_t>     m_rigidParts;
    std::vector<Matrix4>                                m_partTransformRigid;
    std::vector<RigidGroup_t>                           m_partRigids;

    // Machines
    link::Machines                                      m_machines;
    lgrn::IntArrayMultiMap<PartEnt_t, link::MachAnyId>  m_partMachines;
};

} // namespace osp::active
