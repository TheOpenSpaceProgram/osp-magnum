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

#include <Magnum/Shaders/Phong.h>

// forward declare
namespace osp
{
    class PrototypePart;
}

// forward declare
namespace osp::universe
{
    class SatActiveArea;
}

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

class SysVehicle : public IDynamicSystem, public IActivator
{
public:

    SysVehicle(ActiveScene &scene);
    SysVehicle(SysNewton const& copy) = delete;
    SysVehicle(SysNewton&& move) = delete;
    ~SysVehicle() = default;

    //static int area_activate_vehicle(ActiveScene& scene,
    //                                 SysAreaAssociate &area,
    //                                 universe::Satellite areaSat,
    //                                 universe::Satellite loadMe);
    StatusActivated activate_sat(ActiveScene &scene, SysAreaAssociate &area,
            universe::Satellite areaSat, universe::Satellite tgtSat);
    int deactivate_sat(ActiveScene &scene, SysAreaAssociate &area,
            universe::Satellite areaSat, universe::Satellite tgtSat,
            ActiveEnt tgtEnt);

    /**
     * Create a Physical Part from a PrototypePart and put it in the world
     * @param part the part to instantiate
     * @param rootParent Entity to put part into
     * @return Pointer to object created
     */
    ActiveEnt part_instantiate(PrototypePart& part, ActiveEnt rootParent);

    // Handle deleted parts and separations
    void update_vehicle_modification();

private:
    ActiveScene& m_scene;
    //AppPackages& m_packages;

    // temporary
    std::unique_ptr<Magnum::Shaders::Phong> m_shader;

    UpdateOrderHandle m_updateVehicleModification;
};


}
