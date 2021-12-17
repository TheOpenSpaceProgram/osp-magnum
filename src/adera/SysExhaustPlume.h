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
#include <adera/Plume.h>             // for PlumeEffectData

#include <osp/Active/activetypes.h>  // for ActiveEnt

#include <osp/Resource/Resource.h>   // for DependRes

#include <entt/entity/entity.hpp>    // for null, null_t

namespace osp { namespace active { class ActiveScene; } }

namespace adera::active
{

struct ACompExhaustPlume
{
    osp::active::ActiveEnt m_parentMCompRocket{entt::null};
    osp::DependRes<PlumeEffectData> m_effect;

    float m_time{0.0f};
    float m_powerLevel{0.0f};
};

class SysExhaustPlume
{
public:

    /**
     * @brief Add exaust plumes to vehicles in-construction
     *
     * @param rScene [ref] Scene containing rockets with exaust plumes
     */
    static void update_construct(osp::active::ActiveScene& rScene);

    static void update_plumes(osp::active::ActiveScene& rScene);
};

} // namespace adera::active
