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

//-----------------------------------------------------------------------------

using wire_family_t = entt::family<struct wire_type>;
using wire_id_t = wire_family_t::family_type;

template<typename WIRETYPE_T>
constexpr wire_id_t wiretype_id() noexcept
{
    return wire_family_t::type<WIRETYPE_T>;
}


// Templated enum class for ports, to prevent mixups

template<class WIRETYPE_T>
struct WireTypes {
    enum class port : uint16_t {};
};

template<class WIRETYPE_T>
using wire_port_t = typename WireTypes<WIRETYPE_T>::port;

//-----------------------------------------------------------------------------

// Added to Package to identify wiretypes by string for configs
struct RegisteredWiretype
{
    RegisteredWiretype(wire_id_t id) : m_id(id) {}

    wire_id_t m_id;

    // TODO: possibly put some info on fields for the config parser
};

}
