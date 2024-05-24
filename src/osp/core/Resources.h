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

#include "copymove_macros.h"
#include "shared_string.h"
#include "resourcetypes.h"

#include <longeron/id_management/id_set_stl.hpp>
#include <longeron/id_management/refcount.hpp>
#include <longeron/id_management/registry_stl.hpp>

#include <entt/core/family.hpp>
#include <entt/core/any.hpp>

#include <algorithm>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace osp
{

class Resources
{
    using res_data_family_t = entt::family<struct ResourceType>;
    using res_data_type_t = res_data_family_t::value_type;

    struct PerResType
    {
        lgrn::IdRegistryStl<ResId>      m_resIds;
        lgrn::RefCount<int>             m_resRefs;
        std::vector<res_data_type_t>    m_resDataTypes;
        std::vector<entt::any>          m_resData;

        // Pointed to by PerPkgResType::m_nameToResId
        std::vector<SharedString>       m_resNames;
    };

    struct PerPkgResType
    {
        lgrn::IdSetStl<ResId> m_owned;
        std::unordered_map< SharedString, ResId, std::hash<SharedString>, std::equal_to<> > m_nameToResId;
    };

    struct PerPkg
    {
        std::vector<PerPkgResType> m_resTypeOwn;
    };

public:

    Resources() = default;
    OSP_MOVE_ONLY_CTOR_ASSIGN(Resources);

    /**
     * @brief Resize to fit a certain number of resource types
     *
     * @param n [in] Number of types to support
     */
    inline void resize_types(std::size_t n)
    {
        m_perResType.resize(n);
    }

    /**
     * @brief Create a new resource Id
     *
     * @param typeId    [in] Resource Type Id
     * @param pkgId     [in] Package Id
     * @param name      [in] String name identifier
     *
     * @return Newly created Resource Id
     */
    [[nodiscard]] ResId create(ResTypeId typeId, PkgId pkgId, SharedString name);

    [[nodiscard]] ResId find(ResTypeId typeId, PkgId pkgId, std::string_view name) const noexcept;

    /**
     * @brief Get name of Resource Id
     *
     * @param typeId    [in] Resource Type Id
     * @param resId     [in] Resource Id
     *
     * @return Name of resources assigned in create(), unique to a single Package
     */
    [[nodiscard]] SharedString const& name(ResTypeId typeId, ResId resId) const noexcept;

    [[nodiscard]] lgrn::IdRegistryStl<ResId> const& ids(ResTypeId typeId) const noexcept;

    [[nodiscard]] ResIdOwner_t owner_create(ResTypeId typeId, ResId resId) noexcept;

    void owner_destroy(ResTypeId typeId, ResIdOwner_t&& rOwner) noexcept;

    /**
     * @brief Register a datatype to a resource Id
     *
     * This allows a datatype to be associated with Resource Ids (ResId) of a
     * certain type (ResTypeId).
     *
     * @param typeId [in] Resource type to associate data to
     */
    template<typename T>
    void data_register(ResTypeId typeId);

    /**
     * @brief Assign data to a Resource Id
     *
     * Make sure T is registered beforehand with data_register
     *
     * @param typeId    [in] Resource Type Id
     * @param resId     [in] Resource Id
     * @param args      [in] Arguments to pass to constructor
     *
     * @return Reference to datatype created
     */
    template<typename T, typename ... ARGS_T>
    T& data_add(ResTypeId typeId, ResId resId, ARGS_T&& ... args);

    /**
     * @brief Get reference to data type associated with a resource
     *
     * Assert or undefined behaviour if data isn't found
     *
     * @param typeId    [in] Resource Type Id
     * @param resId     [in] Resource Id
     *
     * @return Reference to found data
     */
    template<typename T>
    [[nodiscard]] T& data_get(ResTypeId typeId, ResId resId);

    /**
     * @copydoc Resources::data_get(typeId, resId)
     */
    template<typename T>
    [[nodiscard]] T const& data_get(ResTypeId typeId, ResId resId) const;

    /**
     * @brief Get pointer to data type to associated with a resource
     *
     * @param typeId    [in] Resource Type Id
     * @param resId     [in] Resource Id
     *
     * @return Pointer to found data, nullptr if not found
     */
    template<typename T>
    [[nodiscard]] T* data_try_get(ResTypeId typeId, ResId resId);

    /**
     * @copydoc Resources::data_get(typeId, resId)
     */
    template<typename T>
    [[nodiscard]] T const* data_try_get(ResTypeId typeId, ResId resId) const;

    /**
     * @brief Create a new package
     *
     * Name, metadata, and other additional information on a package is best
     * stored externally by associating the PkgId, not as a responsibility of
     * this class.
     *
     * @return Package Id for newly created package
     */
     [[nodiscard]] PkgId pkg_create();

private:

    PerResType const& get_type(ResTypeId typeId) const
    {
        assert(std::size_t(typeId) < m_perResType.size());
        return m_perResType[std::size_t(typeId)];
    }

    PerResType& get_type(ResTypeId typeId)
    {
        assert(std::size_t(typeId) < m_perResType.size());
        return m_perResType[std::size_t(typeId)];
    }

    template <typename T>
    res_container_t<T>& get_container(PerResType &rPerResType, ResTypeId typeId);

    template <typename T>
    res_container_t<T> const& get_container(PerResType const &rPerResType, ResTypeId typeId) const;

    std::vector<PerResType>     m_perResType;
    lgrn::IdRegistryStl<PkgId>  m_pkgIds;
    std::vector<PerPkg>         m_pkgData;
};

template<typename T>
void Resources::data_register(ResTypeId typeId)
{
    res_data_type_t const type = res_data_family_t::value<T>;
    PerResType &rPerResType = get_type(typeId);

    std::vector<res_data_type_t> &rTypes = rPerResType.m_resDataTypes;

    // Check to make sure type isn't already registered
    assert(std::find(rTypes.begin(), rTypes.end(), type) == rTypes.end());

    rTypes.push_back(type);
    rPerResType.m_resData.emplace_back(res_container_t<T>{});
}

template<typename T, typename ... ARGS_T>
T& Resources::data_add(ResTypeId typeId, ResId resId, ARGS_T&& ... args)
{
    PerResType &rPerResType = get_type(typeId);

    // Ensure resource ID exists
    assert(rPerResType.m_resIds.capacity() > std::size_t(resId));
    assert(rPerResType.m_resIds.exists(resId));

    res_container_t<T> &rContainer = get_container<T>(rPerResType, typeId);

    T& out = rContainer.emplace(resId, std::forward<ARGS_T>(args)...);

    return out;
}

template<typename T>
T& Resources::data_get(ResTypeId typeId, ResId resId)
{
    T* pData = data_try_get<T>(typeId, resId);
    assert(pData != nullptr);
    return *pData;
}

template<typename T>
T const& Resources::data_get(ResTypeId typeId, ResId resId) const
{
    T const* pData = data_try_get<T>(typeId, resId);
    assert(pData != nullptr);
    return *pData;
}

template<typename T>
T const* Resources::data_try_get(ResTypeId typeId, ResId resId) const
{
    static_assert(std::is_const_v<T>, "Non-const data requested from const Resources");
    using NonConst_t = std::remove_const_t<T>;

    PerResType const &rPerResType = get_type(typeId);

    // Ensure resource ID exists
    assert(rPerResType.m_resIds.capacity() > std::size_t(typeId));
    assert(rPerResType.m_resIds.exists(resId));

    res_container_t<NonConst_t> const &rContainer
            = get_container<NonConst_t>(rPerResType, typeId);
    return rContainer.get(resId);
}

template<typename T>
T* Resources::data_try_get(ResTypeId typeId, ResId resId)
{
    PerResType &rPerResType = get_type(typeId);

    // Ensure resource ID exists
    assert(rPerResType.m_resIds.capacity() > std::size_t(resId));
    assert(rPerResType.m_resIds.exists(resId));

    if constexpr (std::is_const_v<T>)
    {
        // Non-const requesting const
        using NonConst_t = std::remove_const_t<T>;
        res_container_t<NonConst_t> const &rContainer
                = const_cast<const Resources*>(this)
                    ->get_container<NonConst_t>(rPerResType, typeId);

        return rContainer.get(resId);
    }
    else
    {
        res_container_t<T> &rContainer = get_container<T>(rPerResType, typeId);
        return rContainer.get(resId);
    }
}

template <typename T>
res_container_t<T>& Resources::get_container(PerResType &rPerResType, ResTypeId typeId)
{
    res_data_type_t const type = res_data_family_t::value<T>;

    std::vector<res_data_type_t> const &rTypes = rPerResType.m_resDataTypes;
    auto typeFound = std::find(rTypes.cbegin(), rTypes.cend(), type);
    assert(typeFound != rTypes.cend()); // Ensure type is registered

    using Container_t = res_container_t<T>;

    size_t const typeIndex = std::distance(rTypes.begin(), typeFound);

    return entt::any_cast<Container_t&>(rPerResType.m_resData[typeIndex]);
}

template <typename T>
res_container_t<T> const& Resources::get_container(PerResType const &rPerResType, ResTypeId typeId) const
{
    return const_cast<Resources*>(this)->get_container<T>(const_cast<PerResType&>(rPerResType), typeId);
}

} // namespace osp
