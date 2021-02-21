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
#include <taskflow/taskflow.hpp>

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

using UpdateOrder_t = tf::Taskflow;
using UpdateOrderHandle_t = tf::Task;
using UpdateExecutor_t = tf::Executor;

using RenderOrder_t = tf::Taskflow;
using RenderOrderHandle_t = tf::Task;
using RenderExecutor_t = tf::Executor;

struct ACompFloatingOrigin
{
    //bool m_dummy;
};

// not really sure what else to put in here
class IDynamicSystem
{
public:
    virtual ~IDynamicSystem() = default;
};

using MapDynamicSys_t = std::map<std::string, std::unique_ptr<IDynamicSystem>,
                                 std::less<> >;

template <typename FUNC_T>
struct UpdateOrderConstraint
{
    FUNC_T m_function;
    std::string m_name;
    std::string m_succeeds;
    std::string m_precedes;
};

using SysUpdateContraint_t = UpdateOrderConstraint<void(*)(ActiveScene&)>;
using SysRenderContraint_t = UpdateOrderConstraint<void(*)(ActiveScene&, ACompCamera const&)>;

template <size_t N>
using SystemUpdates_t = std::array<SysUpdateContraint_t, N>;

template <size_t N>
using SystemRender_t = std::array<SysRenderContraint_t, N>;

using MapUpdateSystemTasks_t = std::map<std::string, SysUpdateContraint_t, std::less<> >;
using MapUpdateRender_t = std::map<std::string, SysRenderContraint_t, std::less<> >;

using MapTasks_t = std::map<std::string, tf::Task, std::less<> >;

}
