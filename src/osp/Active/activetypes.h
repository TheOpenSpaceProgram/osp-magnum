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

constexpr unsigned gc_heir_physics_level = 1;

using ActiveEnt = entt::entity;

using UpdateOrder = FunctionOrder<void(void)>;
using UpdateOrderHandle = FunctionOrderHandle<void(void)>;

using RenderOrder = FunctionOrder<void(ACompCamera const&)>;
using RenderOrderHandle = FunctionOrderHandle<void(ACompCamera const&)>;

struct ACompFloatingOrigin
{
    bool m_dummy;
};

// not really sure what else to put in here
class IDynamicSystem
{
public:
    virtual ~IDynamicSystem() = default;
};

using MapDynamicSys = std::map<std::string, std::unique_ptr<IDynamicSystem>>;

}
