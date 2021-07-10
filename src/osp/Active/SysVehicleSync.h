/**
 * Open Space Program
 * Copyright © 2019-2021 Open Space Program Project
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

#include "universe_sync.h"

namespace osp::active
{

struct SyncVehicles
{
    MapSatToEnt_t m_inArea;
};

class SysVehicleSync
{
public:

    static ActiveEnt activate(ActiveScene &rScene, universe::Universe &rUni,
                              universe::Satellite areaSat,
                              universe::Satellite tgtSat);

    static void deactivate(ActiveScene &rScene, universe::Universe &rUni,
                           universe::Satellite areaSat,
                           universe::Satellite tgtSat, ActiveEnt tgtEnt);

    /**
     * Activate/Deactivate vehicles that enter/exit the ActiveArea
     *
     * Nearby vehicles are detected by SysAreaAssociate, and are added to a
     * queue. This function reads the queue and activates vehicles accordingly.
     * Activated vehicles will be in an incomplete "In-Construction" state so
     * that individual features can be handled by separate systems.
     *
     * This function also update Satellites transforms of the currently
     * activated vehicles in the scene
     *
     * @param rScene [ref] Scene containing vehicles to update
     */
    static void update_universe_sync(ActiveScene& rScene);
};

}
