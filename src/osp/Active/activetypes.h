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

#include <map>
#include <memory>

#include "../FunctonOrder.h"

#include <entt/entity/registry.hpp>


namespace osp::active
{

class SysNewton;

class ActiveScene;

struct ACompCamera;

struct ACompTransform;

constexpr unsigned gc_heir_physics_level = 1;

enum class ActiveEnt: entt::id_type {};

inline std::ostream& operator<<(std::ostream& os, ActiveEnt e)
{ return os << static_cast<int>(e); }

using ActiveReg_t = entt::basic_registry<ActiveEnt>;

using UpdateOrder_t = FunctionOrder<void(ActiveScene&)>;
using UpdateOrderHandle_t = FunctionOrderHandle<void(ActiveScene&)>;

using RenderOrder_t = FunctionOrder<void(ActiveScene&, ACompCamera&)>;
using RenderOrderHandle_t = FunctionOrderHandle<void(ActiveScene&, ACompCamera&)>;

struct ACompFloatingOrigin
{
    //bool m_dummy;
};

}
