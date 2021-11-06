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

template<typename TYPE_T>
struct type_identity
{
    using type = TYPE_T;
};

template<typename TYPE_T>
using type_identity_t = typename type_identity<TYPE_T>::type;

template<typename TYPE_T, typename = void>
struct underlying_int_type;

template<typename TYPE_T>
struct underlying_int_type< TYPE_T, std::enable_if_t< std::is_enum_v<TYPE_T> > >
 : std::underlying_type<TYPE_T>
{ };

template<typename TYPE_T>
struct underlying_int_type< TYPE_T, std::enable_if_t< ! std::is_enum_v<TYPE_T> > >
 : type_identity<TYPE_T>
{ };

template<typename TYPE_T>
using underlying_int_type_t = typename underlying_int_type<TYPE_T>::type;

enum class Test : uint32_t {};
static_assert(std::is_same_v<underlying_int_type_t<int>, int>);
static_assert(std::is_same_v<underlying_int_type_t<Test>, uint32_t>);

//-----------------------------------------------------------------------------


template<class TYPE_T>
constexpr TYPE_T id_null() noexcept
{
    using id_int_t = underlying_int_type_t<TYPE_T>;
    return TYPE_T(std::numeric_limits<id_int_t>::max());
}

//-----------------------------------------------------------------------------

/**
 * @brief Generates reusable sequential IDs
 */
template<typename ID_T, bool NO_AUTO_RESIZE = false>
class IdRegistry
{
    using id_int_t = underlying_int_type_t<ID_T>;

public:

    IdRegistry() = default;
    IdRegistry(size_t capacity) { reserve(capacity); };

    /**
     * @brief Create a single ID
     *
     * @return A newly created ID
     */
    [[nodiscard]] ID_T create()
    {
        ID_T output{ id_null<ID_T>() };
        create(&output, 1);
        return output;
    }

    /**
     * @brief Create multiple IDs
     *
     * @param out   [out] Iterator out
     * @param count [in] Number of IDs to generate
     */
    template<typename IT_T>
    void create(IT_T out, size_t count);

    /**
     * @return Size required to fit all existing IDs, or allocated size if
     *         reserved ahead of time
     */
    constexpr size_t capacity() const noexcept { return m_deleted.size(); }

    /**
     * @return Number of registered IDs
     */
    constexpr size_t size() const { return capacity() - m_deleted.count(); }

    /**
     * @brief Allocate space for at least n IDs
     *
     * @param n [in] Number of IDs to allocate for
     */
    void reserve(size_t n)
    {
        m_deleted.resize(n, true);
    }

    /**
     * @brief Remove an ID. This will mark it for reuse
     *
     * @param id [in] ID to remove
     */
    void remove(ID_T id)
    {
        if ( ! exists(id))
        {
            throw std::runtime_error("ID does not exist");
        }

        m_deleted.set(id_int_t(id));
    }

    /**
     * @brief Check if an ID exists
     *
     * @param id [in] ID to check
     *
     * @return true if ID exists
     */
    bool exists(ID_T id) const noexcept
    {
        return ! m_deleted.test(id_int_t(id));
    };

    template <typename FUNC_T>
    void for_each(FUNC_T func);

private:
    HierarchicalBitset<uint64_t> m_deleted;

}; // class IdRegistry

template<typename ID_T, bool NO_AUTO_RESIZE>
template<typename IT_T>
void IdRegistry<ID_T, NO_AUTO_RESIZE>::create(IT_T out, size_t count)
{
    if (m_deleted.count() < count)
    {
        // auto-resize
        if constexpr (NO_AUTO_RESIZE)
        {
            throw std::runtime_error(
                    "Reached max capacity with automatic resizing disabled");
        }
        else
        {
            reserve(std::max(capacity() + count, capacity() * 2));
        }
    }

    m_deleted.take<IT_T, ID_T>(out, count);
}

template<typename ID_T, bool NO_AUTO_RESIZE>
template <typename FUNC_T>
void IdRegistry<ID_T, NO_AUTO_RESIZE>::for_each(FUNC_T func)
{
    // TODO: this is kind of inefficient and doesn't utilize the hierarchy
    for (size_t i = 0; i < size(); i ++)
    {
        if ( ! m_deleted.test(i) )
        {
            func(ID_T(i));
        }
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
    IdStorage(IdStorage&& move) = default;
    ~IdStorage() { assert( ! has_value() ); }

    // Delete copy
    IdStorage(IdStorage const& copy) = delete;
    IdStorage& operator=(IdStorage const& copy) = delete;

    // Allow move assignment only if there's no ID stored
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
    RefCount(RefCount&& move) = default;
    RefCount(size_t capacity) { resize(capacity); };

    // Delete copy
    RefCount(RefCount const& copy) = delete;
    RefCount& operator=(RefCount const& copy) = delete;

    // Allow move assign only if all counts are zero
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
    using id_int_t = underlying_int_type_t<ID_T>;

public:

    using Storage_t = IdStorage<ID_T, IdRefCount>;

    Storage_t ref_add(ID_T id)
    {
        auto const idInt = id_int_t(id);
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
            auto const idInt = id_int_t(rStorage.m_id);
            (*this)[idInt] --;
            rStorage.m_id = id_null<ID_T>();
        }
    }

}; // class IdRefCount

}
