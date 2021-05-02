#pragma once

#include <entt/core/family.hpp>

#include <limits>

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

// Templated enum class as integer types to prevent mixups

template<class WIRETYPE_T>
struct WireTypes
{
    enum class link : uint32_t {};
    enum class node : uint32_t {};
    enum class port : uint16_t {};
};

template<class WIRETYPE_T>
using nodeindex_t = typename WireTypes<WIRETYPE_T>::node;

template<class WIRETYPE_T>
using linkindex_t = typename WireTypes<WIRETYPE_T>::link;

template<class WIRETYPE_T>
using portindex_t = typename WireTypes<WIRETYPE_T>::port;

template<class TYPE_T>
constexpr TYPE_T nullvalue() { return TYPE_T(std::numeric_limits<typename std::underlying_type<TYPE_T>::type>::max()); }

//-----------------------------------------------------------------------------

// Added to Package to identify wiretypes by string for configs
struct RegisteredWiretype
{
    RegisteredWiretype(wire_id_t id) : m_id(id) {}

    wire_id_t m_id;

    // TODO: possibly put some info on fields for the config parser
};

}
