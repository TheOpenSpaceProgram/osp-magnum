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

#include <longeron/id_management/registry.hpp>

#include <entt/core/family.hpp>
#include <entt/core/any.hpp>

#include <algorithm>
#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace osp
{

enum class ResTypeId : uint32_t { };
enum class ResId : uint32_t { };
enum class PkgId : uint32_t { };

template <typename T>
struct resource_container
{
    using type = std::unordered_map<ResId, T>;
};

template <typename T>
using resource_container_t = typename resource_container<T>::type;

class Resources
{
    using res_data_family_t = entt::family<struct ResourceType>;
    using res_data_type_t = res_data_family_t::family_type;

    struct PerResType
    {
        lgrn::IdRegistry<ResId>         m_resIds;
        std::vector<res_data_type_t>    m_resDataTypes;
        std::vector<entt::any>          m_resData;
    };

    struct PerPkgResType
    {
        lgrn::HierarchicalBitset<uint64_t> m_owned;
    };

    struct PerPkg
    {
        std::vector<PerPkgResType> m_resTypeOwn;
        // points to a key in m_pkgNames, might be a bit hacky
        std::string_view m_name;
    };

public:

    void resize_types(std::size_t n)
    {
        m_perResType.resize(n);
    }

    ResId create(ResTypeId typeId, PkgId pkgId, std::string_view name)
    {
        // Create ResId associated to specified ResTypeId
        assert(m_perResType.size() > std::size_t(typeId));
        PerResType &rPerResType = m_perResType[std::size_t(typeId)];
        ResId const newResId = rPerResType.m_resIds.create();

        // Associate it with the package
        assert(m_pkgData.size() > std::size_t(pkgId));
        PerPkg &rPkg = m_pkgData[std::size_t(pkgId)];
        assert(rPkg.m_resTypeOwn.size() > std::size_t(typeId));
        PerPkgResType &rPkgType = rPkg.m_resTypeOwn[std::size_t(typeId)];
        rPkgType.m_owned.resize(m_perResType.size());
        rPkgType.m_owned.set(std::size_t(newResId));

        return newResId;
    }

    template<typename T>
    void data_register(ResTypeId typeId)
    {
        assert(std::size_t(typeId) < m_perResType.size());
        PerResType &rPerResType = m_perResType[std::size_t(typeId)];
        res_data_type_t const type = res_data_family_t::type<T>;

        std::vector<res_data_type_t> &rTypes = rPerResType.m_resDataTypes;

        // Check to make sure type isn't already registered
        assert(std::find(rTypes.begin(), rTypes.end(), type) == rTypes.end());

        rTypes.push_back(type);
        rPerResType.m_resData.emplace_back(resource_container_t<T>());
    }

    template<typename T, typename ... ARGS_T>
    T& data_add(ResTypeId typeId, ResId resId, ARGS_T&& ... args)
    {
        using Container_t = resource_container_t<T>;

        res_data_type_t const type = res_data_family_t::type<T>;
        assert(m_perResType.size() > std::size_t(typeId));
        PerResType &rPerResType = m_perResType[std::size_t(typeId)];

        assert(rPerResType.m_resIds.exists(resId)); // Ensure resource ID exists

        std::vector<res_data_type_t> &rTypes = rPerResType.m_resDataTypes;
        auto typeFound = std::find(rTypes.begin(), rTypes.end(), type);
        assert(typeFound != rTypes.end()); // Ensure type is registered

        size_t const typeIndex = std::distance(rTypes.begin(), typeFound);
        Container_t &rContainer = entt::any_cast<Container_t&>(
                                        rPerResType.m_resData[typeIndex]);

        // Emplace without needing copy constructor
        auto const & [newIt, success] = rContainer.try_emplace(
                            resId, std::forward<ARGS_T>(args)...);

        assert(success); // Emplace should always succeed

        return newIt->second;
    }

    PkgId pkg_add(std::string_view name)
    {
        assert(m_pkgData.size() == m_pkgNames.size());

        uint32_t const newPkgId = m_pkgData.size();

        auto newIt = m_pkgNames.emplace(name, newPkgId);
        m_pkgData.emplace_back(
                PerPkg{ std::vector<PerPkgResType>(m_perResType.size()),
                        newIt.first->first });

        return PkgId(newPkgId);
    }

private:
    std::vector<PerResType> m_perResType;

    std::unordered_map<std::string, uint32_t> m_pkgNames;
    std::vector<PerPkg> m_pkgData;
};


} // namespace osp
