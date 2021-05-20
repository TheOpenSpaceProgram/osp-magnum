#pragma once

#include <entt/core/family.hpp>

namespace osp
{

using machine_family_t = entt::family<struct resource_type>;
using machine_id_t = machine_family_t::family_type;
//using machine_id_t = uint32_t;

template<typename MACH_T>
constexpr machine_id_t mach_id() noexcept
{
    return machine_family_t::type<MACH_T>;
}

// Added to Package to identify machine types by string for configs
struct RegisteredMachine
{
    RegisteredMachine(machine_id_t id) : m_id(id) {}

    machine_id_t m_id;

    // TODO: possibly put some info on fields for the config parser
};

}
