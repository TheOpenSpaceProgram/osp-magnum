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
#include "resources.h"

using namespace osp;

ResId Resources::create(ResTypeId typeId, PkgId pkgId, std::string_view name)
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
    rPkgType.m_owned.set(std::size_t(newResId));

    // Track name
    rPerResType.m_resNames.resize(rPerResType.m_resIds.capacity());
    SharedString& rResName = rPerResType.m_resNames[std::size_t(newResId)];
    rResName = SharedString::create(name); // allocates name
    [[maybe_unused]] auto newIt = rPkgType.m_nameToResId.emplace(rResName, newResId);
    assert(newIt.second); // emplace should always succeed

    return newResId;
}

ResId Resources::find(ResTypeId typeId, PkgId pkgId, std::string_view name) const noexcept
{
    PerResType const &rPerResType = get_type(typeId);

    assert(m_pkgData.size() > std::size_t(pkgId));
    PerPkg const &rPkg = m_pkgData[std::size_t(pkgId)];
    assert(rPkg.m_resTypeOwn.size() > std::size_t(typeId));
    PerPkgResType const &rPkgType = rPkg.m_resTypeOwn[std::size_t(typeId)];

    auto findIt = rPkgType.m_nameToResId.find(name);

    if (findIt == rPkgType.m_nameToResId.end())
    {
        return lgrn::id_null<ResId>(); // not found
    }
    return findIt->second;
}

lgrn::IdRegistry<ResId> const& Resources::ids(ResTypeId typeId) const noexcept
{
    return get_type(typeId).m_resIds;
}

ResIdOwner_t Resources::owner_create(ResTypeId typeId, ResId resId) noexcept
{
    PerResType &rPerResType = get_type(typeId);
    rPerResType.m_resRefs[std::size_t(resId)] ++;
    ResIdOwner_t owner;
    owner.m_id = resId;
    return owner;
}

void Resources::owner_destroy(ResTypeId typeId, ResIdOwner_t&& rOwner) noexcept
{
    if (!rOwner.has_value())
    {
        return;
    }
    PerResType &rPerResType = get_type(typeId);
    int &rCount = rPerResType.m_resRefs[std::size_t(rOwner.m_id)];
    rCount --;
    rOwner.m_id = ResIdOwner_t{};
}

PkgId Resources::pkg_create()
{
    PkgId const newPkgId = m_pkgIds.create();
    m_pkgData.resize(m_pkgIds.capacity());
    m_pkgData[std::size_t(newPkgId)].m_resTypeOwn.resize(m_perResType.size());
    return newPkgId;
}


