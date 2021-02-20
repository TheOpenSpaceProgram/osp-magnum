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
#include "osp/Active/physics.h"
#include "adera/Plume.h"
#include "adera/Shaders/PlumeShader.h"
#include "osp/Resource/Resource.h"

namespace osp::active
{

struct ACompExhaustPlume
{
    ActiveEnt m_parentMachineRocket{entt::null};
    DependRes<PlumeEffectData> m_effect;
};

class SysExhaustPlume : public IDynamicSystem
{
public:
    static inline std::string smc_name = "ExhaustPlume";

    SysExhaustPlume() = default;
    ~SysExhaustPlume() = default;

    /**
     * Initialize plume graphics
     * 
     * SysMachineRocket only attaches ACompExhaustPlume to eligible entities;
     * this function takes such entities, retrieves the appropriate graphics
     * resources, and configures the graphical components
     */
    static void initialize_plume(ActiveScene& rScene, ActiveEnt e);

    static void update_plumes(ActiveScene& rScene);

private:
    float m_time;

    static inline SystemUpdates_t<1> smc_update
    {
        {&SysExhaustPlume::update_plumes, "exhaust_plume", "mach_rocket", ""}
    };
};

} // namespace osp::active
