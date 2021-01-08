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
#include <Magnum/Shaders/Phong.h>
#include "osp/Resource/Resource.h"

namespace osp::active
{

struct ACompExhaustPlume
{
    ActiveEnt m_parentMachineRocket{entt::null};
    //Magnum::Shaders::Phong m_shader;
    //DependRes<PlumeEffect> m_effect;
};

class SysExhaustPlume : public IDynamicSystem
{
public:
    static inline std::string smc_name = "ExhaustPlume";

    SysExhaustPlume(ActiveScene& scene);
    ~SysExhaustPlume() = default;

    /**
     * Initialize plume graphics
     * 
     * SysMachineRocket only attaches ACompExhaustPlume to eligible entities;
     * this function takes such entities, retrieves the appropriate graphics
     * resources, and configures the graphical components
     */
    void initialize_plume(ActiveEnt e);

    void update_plumes(ActiveScene& rScene);

private:
    ActiveScene& m_scene;
    float m_time;

    UpdateOrderHandle_t m_updatePlume;
};

} // namespace osp::active
