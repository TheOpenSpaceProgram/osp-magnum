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

#include "hierarchical_bitset.h"

#include <cassert>
#include <limits>
#include <utility>
#include <vector>

namespace osp
{



template<class TYPE_T>
constexpr TYPE_T id_null() noexcept
{
    using underlying_t = typename std::underlying_type_t<TYPE_T>;
    return TYPE_T(std::numeric_limits<underlying_t>::max());
}

//-----------------------------------------------------------------------------

/**
 * @brief Generates reusable sequential IDs
 */
template<typename ID_T, bool NO_AUTO_RESIZE = false>
class IdRegistry
{
    using id_int_t = std::underlying_type_t<ID_T>;

public:

    IdRegistry() = default;
    IdRegistry(size_t capacity) { reserve(capacity); };

    [[nodiscard]] ID_T create()
    {

    }

    template<typename IT_T>
    void create(IT_T out, size_t count)
    {

    }

    /**
     * @return Size required to fit all existing IDs, or allocated size if
     *         reserved ahead of time
     */
    size_t capacity() const noexcept { return m_deleted.size(); }

    size_t size() const
    {
        return 0;
    }

    void reserve(size_t n)
    {

    }

    void remove(ID_T id)
    {
        if ( ! exists(id))
        {
            throw std::runtime_error("ID over max capacity with automatic resizing disabled");
        }
    }

    bool exists(ID_T id) const noexcept
    {

    };

    template <typename FUNC_T>
    void for_each(FUNC_T func);

private:
    HierarchicalBitset m_deleted;

}; // class IdRegistry

template<typename ID_T, bool NO_AUTO_RESIZE>
template <typename FUNC_T>
void IdRegistry<ID_T, NO_AUTO_RESIZE>::for_each(FUNC_T func)
{
    for (size_t i = 0; i < size(); i ++)
    {
        //if (m_exists[i])
        //{
        //    func(ID_T(i));
        //}
    }
}

//-----------------------------------------------------------------------------

/**
 * @brief Long term reference counted storage for IDs
 */
template<typename ID_T, typename REG_T>
class IdStorage
{
    friend REG_T;

public:
    IdStorage() : m_id{ id_null<ID_T>() } { }
    ~IdStorage() { assert( ! has_value() ); }

    // Delete copy
    IdStorage(IdStorage const& copy) = delete;
    IdStorage& operator=(IdStorage const& copy) = delete;

    // Allow move only if there's no ID stored
    IdStorage(IdStorage&& move)
    {
        assert( ! has_value() );
        m_id = std::exchange(move.m_id, id_null<ID_T>());
    }
    IdStorage& operator=(IdStorage&& move)
    {
        assert( ! has_value() );
        m_id = std::exchange(move.m_id, id_null<ID_T>());
        return *this;
    }

    operator ID_T() const noexcept { return m_id; }
    constexpr ID_T value() const noexcept { return m_id; }
    constexpr bool has_value() const noexcept { return m_id != id_null<ID_T>(); }

private:
    IdStorage(ID_T id) : m_id{ id } { }

    ID_T m_id;

}; // class IdStorage

//-----------------------------------------------------------------------------

/**
 * @brief An IdRegistry that uses IdStorages to uniquely store Ids
 */
template<typename ID_T, bool NO_AUTO_RESIZE = false>
class UniqueIdRegistry : IdRegistry<ID_T, NO_AUTO_RESIZE>
{
    using base_t = IdRegistry<ID_T, NO_AUTO_RESIZE>;
public:

    using Storage_t = IdStorage<ID_T, UniqueIdRegistry<ID_T, NO_AUTO_RESIZE> >;

    using IdRegistry<ID_T, NO_AUTO_RESIZE>::IdRegistry;
    using base_t::capacity;
    using base_t::size;
    using base_t::reserve;
    using base_t::exists;
    using base_t::for_each;

    [[nodiscard]] Storage_t create()
    {
        return base_t::create();
    }

    void remove(Storage_t& rStorage)
    {
        base_t::remove(rStorage);
        rStorage.release();
    }
}; // class UniqueIdRegistry


//-----------------------------------------------------------------------------

template<typename COUNT_T = uint8_t>
class RefCount : std::vector<COUNT_T>
{
    using base_t = std::vector<COUNT_T>;
public:

    RefCount() = default;
    RefCount(size_t capacity) { resize(capacity); };

    // Delete copy
    RefCount(RefCount const& copy) = delete;
    RefCount& operator=(RefCount const& copy) = delete;

    // Allow move only if all counts are zero
    RefCount(RefCount&& move)
    {
        assert(isRemainingZero(0));
        base_t(std::move(move));
    }
    RefCount& operator=(RefCount&& move)
    {
        assert(isRemainingZero(0));
        base_t(std::move(move));
        return *this;
    }

    ~RefCount()
    {
        // Make sure ref counts are all zero on destruction
        assert(isRemainingZero(0));
    }

    bool isRemainingZero(size_t start) const noexcept
    {
        for (size_t i = start; i < size(); i ++)
        {
            if (0 != (*this)[i])
            {
                return false;
            }
        }
        return true;
    }

    using base_t::size;
    using base_t::operator[];

    void resize(size_t newSize)
    {
        if (newSize < size())
        {
            // sizing down, make sure zeros
            if ( ! isRemainingZero(newSize))
            {
                throw std::runtime_error("Downsizing non-zero ref counts");
            }
        }
        base_t::resize(newSize, 0);
    }

}; // class RefCount

template<typename ID_T, typename COUNT_T = uint8_t>
class IdRefCount : public RefCount<COUNT_T>
{
public:

    using Storage_t = IdStorage<ID_T, IdRefCount>;

    Storage_t ref_add(ID_T id)
    {
        auto const idInt = std::underlying_type_t<ID_T>(id);
        if (this->size() <= idInt)
        {
            this->resize(idInt + 1);
        }
        (*this)[idInt] ++;

        return Storage_t(id);
    }

    void ref_release(Storage_t& rStorage) noexcept
    {
        if (rStorage.has_value())
        {
            auto const idInt = std::underlying_type_t<ID_T>(rStorage.m_id);
            (*this)[idInt] --;
            rStorage.m_id = id_null<ID_T>();
        }
    }

}; // class IdRefCount

}
