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

#include "../Resource/Package.h"
#include "../Resource/blueprints.h"
#include "osp/Active/SysMachine.h"

namespace osp::active
{

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

struct ACompVehicleInConstruction
{
    ACompVehicleInConstruction(DependRes<BlueprintVehicle>& blueprint)
     : m_blueprint(blueprint)
    { }
    DependRes<BlueprintVehicle> m_blueprint;
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

class SysVehicle
{
public:

    /**
     * Deal with vehicle separations and part deletions
     *
     * @param rScene [ref] Scene containing vehicles to update
     */
    static void update_vehicle_modification(ActiveScene& rScene);

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
    static ActiveEnt part_instantiate(
            ActiveScene& rScene, PrototypePart const& part,
            BlueprintPart const& blueprint, ActiveEnt rootParent);
};


}
