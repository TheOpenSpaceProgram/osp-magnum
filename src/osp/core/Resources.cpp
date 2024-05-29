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
#include "Resources.h"

using namespace osp;

ResId Resources::create(ResTypeId const typeId, PkgId const pkgId, SharedString name)
{
    // Create ResId associated to specified ResTypeId
    PerResType &rPerResType = get_type(typeId);
    ResId const newResId = rPerResType.m_resIds.create();

    // Resize ref counts
    rPerResType.m_resRefs.resize(rPerResType.m_resIds.capacity());

    // Associate it with the package
    assert(m_pkgData.size() > std::size_t(pkgId));
    PerPkg &rPkg = m_pkgData[std::size_t(pkgId)];
    assert(rPkg.m_resTypeOwn.size() > std::size_t(typeId));
    PerPkgResType &rPkgType = rPkg.m_resTypeOwn[std::size_t(typeId)];
    rPkgType.m_owned.resize(rPerResType.m_resIds.capacity());
    rPkgType.m_owned.insert(newResId);

    // Track name
    auto const& [newIt, success] = rPkgType.m_nameToResId.emplace(std::move(name), newResId);
    auto const& [key, value]     = *newIt;
    assert(success); // emplace should always succeed

    rPerResType.m_resNames.resize(rPerResType.m_resIds.capacity());
    rPerResType.m_resNames[std::size_t(value)] = key;

    return newResId;
}

ResId Resources::find(ResTypeId const typeId, PkgId const pkgId, std::string_view const name) const noexcept
{
    PerResType const &rPerResType = get_type(typeId);

    assert(m_pkgData.size() > std::size_t(pkgId));
    PerPkg const &rPkg = m_pkgData[std::size_t(pkgId)];
    assert(rPkg.m_resTypeOwn.size() > std::size_t(typeId));
    PerPkgResType const &rPkgType = rPkg.m_resTypeOwn[std::size_t(typeId)];

    // TODO: The call to SharedString::create_reference() shouldn't be necessary
    // but transparent comparitors for std::unordered_map don't work very well.
    if(auto const& findIt = rPkgType.m_nameToResId.find(SharedString::create_reference(name));
       findIt != rPkgType.m_nameToResId.end())
    {
        return findIt->second;
    }
    return lgrn::id_null<ResId>(); // not found
}

SharedString const& Resources::name(ResTypeId const typeId, ResId const resId) const noexcept
{
    PerResType const &rPerResType = get_type(typeId);

    return rPerResType.m_resNames[std::size_t(resId)];
}

lgrn::IdRegistryStl<ResId> const& Resources::ids(ResTypeId const typeId) const noexcept
{
    return get_type(typeId).m_resIds;
}

ResIdOwner_t Resources::owner_create(ResTypeId const typeId, ResId const resId) noexcept
{
    PerResType &rPerResType = get_type(typeId);
    rPerResType.m_resRefs[std::size_t(resId)] ++;
    ResIdOwner_t owner;
    owner.m_id = resId;
    return owner;
}

void Resources::owner_destroy(ResTypeId const typeId, ResIdOwner_t&& rOwner) noexcept
{
    if (!rOwner.has_value())
    {
        return;
    }
    PerResType &rPerResType = get_type(typeId);
    int &rCount = rPerResType.m_resRefs[std::size_t(rOwner.m_id)];
    -- rCount;
    rOwner.m_id = ResIdOwner_t{};
}

PkgId Resources::pkg_create()
{
    PkgId const newPkgId = m_pkgIds.create();
    m_pkgData.resize(m_pkgIds.capacity());
    m_pkgData[std::size_t(newPkgId)].m_resTypeOwn.resize(m_perResType.size());
    return newPkgId;
}
