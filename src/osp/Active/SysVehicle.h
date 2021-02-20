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

#include "activetypes.h"

#include "SysAreaAssociate.h"

#include "../Universe.h"
#include "../Resource/Package.h"
#include "../Resource/blueprints.h"
#include "osp/Active/SysMachine.h"

namespace osp::active
{

struct WireMachineConnection
{
    ActiveEnt m_fromPartEnt;
    unsigned m_fromMachine;
    WireOutPort m_fromPort;

    ActiveEnt m_toPartEnt;
    unsigned m_toMachine;
    WireInPort m_toPort;
};

struct ACompVehicle
{
    std::vector<ActiveEnt> m_parts;
    //entt::sparse_set<ActiveEnt> m_parts;

    // index to 'main part' in m_parts. if the vehicle separates into multiple
    // vehicles, then the resulting vehicle containing the main part is the
    // original vehicle.
    unsigned m_mainPart{0};

    // set this if vehicle is modified:
    // * 0: nothing happened
    // * 1: something exploded (m_destroy set on some parts), but vehicle
    //      isn't split into peices
    // * 2+: number of separation islands
    unsigned m_separationCount{0};
};

struct ACompPart
{
    ActiveEnt m_vehicle{entt::null};

    // set this to true if this part is to be destroyed on the vehicle
    // modification update. also set m_separationCount on ACompVehicle
    bool m_destroy{false};

    // if vehicle separates into more vehicles, then set this to
    // something else on parts that are separated together.
    // actual separation happens in update_vehicle_modification
    unsigned m_separationIsland{0};
};

class SysVehicle : public IDynamicSystem
{
public:

    static inline std::string smc_name = "Vehicle";

    SysVehicle() = default;
    SysVehicle(SysNewton const& copy) = delete;
    SysVehicle(SysNewton&& move) = delete;
    ~SysVehicle() = default;

    static ActiveEnt activate(ActiveScene &rScene, universe::Universe &rUni,
                              universe::Satellite areaSat,
                              universe::Satellite tgtSat);

    static void deactivate(ActiveScene &rScene, universe::Universe &rUni,
                           universe::Satellite areaSat,
                           universe::Satellite tgtSat, ActiveEnt tgtEnt);

    /**
     * Deal with activating and deactivating nearby vehicle Satellites in the
     * Universe, and also update transforms of currently activated vehicles.
     *
     * @param rScene [in/out] Scene containing vehicles to update
     */
    static void update_activate(ActiveScene& rScene);

    /**
     * Deal with vehicle separations and part deletions
     *
     * @param rScene [in/out] Scene containing vehicles to update
     */
    static void update_vehicle_modification(ActiveScene& rScene);

private:

    /* Stores the association between a PrototypeObj entity and the indices of
     * its owned machines in the PrototypePart array. Stores a const reference
     * to a vector because the vector is filled with PrototypePart machine data
     * by part_instantiate() and consumed by part_instantiate_machines(), all
     * within the activate() function scope within which the index data is stable.
     */
    struct MachineDef
    {
        ActiveEnt m_machineOwner;
        std::vector<unsigned> const& m_machineIndices;

        MachineDef(ActiveEnt owner, std::vector<unsigned> const& indexArray)
            : m_machineOwner(owner)
            , m_machineIndices(indexArray)
        {}
    };

    /**
     * Compute the volume of a part
     *
     * Traverses the immediate children of the specified entity and sums the
     * volumes of any detected collision volumes. Cannot account for overlapping
     * collider volumes.
     *
     * @param rScene [in] ActiveScene containing relevant scene data
     * @param part   [in] The part
     *
     * @return The part's collider volume
     */
    static float compute_hier_volume(ActiveScene& rScene, ActiveEnt part);

    /**
     * Create a Physical Part from a PrototypePart and put it in the world
     *
     * @param part [in] The part prototype to instantiate
     * @param blueprint [in] Unique part configuration data
     * @param rootParent [in] Entity to put part into
     *
     * @return A pair containing the entity created and the list of part machines
     */
    static std::pair<ActiveEnt, std::vector<MachineDef>> part_instantiate(
        ActiveScene& rScene,
        PrototypePart& part,
        BlueprintPart& blueprint,
        ActiveEnt rootParent);

    /**
     * Add machines to the specified entity
     *
     * Receives the master arrays of prototype and blueprint machines from a
     * prototype/blueprint part, as well as an array of indices that describe
     * which of the machines from the master list are to be instantiated for
     * the specified entity.
     * @param partEnt [in] The part entity which owns the ACompMachines component
     * @param entity [in] The target entity to receive the machines
     * @param protoMachines [in] The master array of prototype machines
     * @param blueprintMachines [in] The master array of machine configs
     * @param machineIndices [in] The indices of the machines to add to entity
     */
    static void add_machines_to_object(ActiveScene& rScene,
        ActiveEnt partEnt, ActiveEnt entity,
        std::vector<PrototypeMachine> const& protoMachines,
        std::vector<BlueprintMachine> const& blueprintMachines,
        std::vector<unsigned> const& machineIndices);

    /**
     * Instantiate all part machines
     *
     * Machine instantiation requires the part's hierarchy to already exist
     * in case a sub-object needs information about its peers. Since this means
     * machines can't be instantiated alongside their associated object, the
     * association between entities and prototype machines is captured, then used
     * to call this function after all part children exist to instance all the
     * machines at once.
     *
     * Effectively just calls add_machines_to_object() for each object in the
     * part.
     *
     * @param partEnt [in] The root entity of the part
     * @param machineMapping [in] The mapping from objects to their machines
     * @param part [in] The prototype part being created
     * @param partBP [in] The blueprint configs of the part being created
     */
    static void part_instantiate_machines(ActiveScene& rScene, ActiveEnt partEnt,
        std::vector<MachineDef> const& machineMapping,
        PrototypePart const& part, BlueprintPart const& partBP);

    static inline std::array<SysUpdateContraint_t, 2> smc_update
    {
        SysUpdateContraint_t{&SysVehicle::update_activate,
            "vehicle_activate", "", "vehicle_modification"},
        SysUpdateContraint_t{&SysVehicle::update_vehicle_modification,
            "vehicle_modification", "", "physics"}
    };
};


}
