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

#include <array>
#include <bitset>
#include <cstdint>
#include <memory>
#include <stdexcept>

namespace osp
{

#ifdef __GNUC__

    inline int ctz(uint64_t a) noexcept { return __builtin_ctzll(a); }

#elif defined(_MSC_VER)

    #include <intrin.h>

    inline int ctz(uint64_t a) noexcept
    {
        unsigned long int b = 0;
        _BitScanForward64(&b, a);
        return b;
    }
#elif

    static_assert(false, "Missing ctz 'count trailing zeros' implementation for this compiler");

#endif

/**
 * @brief Divide and round up
 */
template<typename NUM_T, typename DENOM_T>
constexpr decltype(auto) div_ceil(NUM_T num, DENOM_T denom) noexcept
{
    return (num / denom) + (num % denom != 0);
}

template<typename INT_T>
constexpr bool bit_test(INT_T block, int bit)
{
    return (block & (INT_T(1) << bit) ) != 0;
}

template<typename INT_T>
constexpr void copy_bits(INT_T const* pSrc, INT_T* pDest, size_t bits) noexcept
{
    while (bits >= sizeof(INT_T) * 8)
    {
        *pDest = *pSrc;
        pSrc ++;
        pDest ++;
        bits -= sizeof(INT_T) * 8;
    }

    if (bits != 0)
    {
        INT_T const mask = (~INT_T(0)) << bits;

        *pDest &= mask;
        *pDest |= ( (~mask) & (*pSrc) );
    }
}

template<typename INT_T>
constexpr void set_bits(INT_T* pDest, size_t bits) noexcept
{
    while (bits >= sizeof(INT_T) * 8)
    {
        *pDest = ~INT_T(0);
        pDest ++;
        bits -= sizeof(INT_T) * 8;
    }

    if (bits != 0)
    {
        *pDest |= ~( (~INT_T(0)) << bits );
    }
}

//-----------------------------------------------------------------------------

/**
 * @brief A bitset featuring hierarchical rows for fast iteration over set bits
 *
 * For example, when using 8-bit block sizes, the structure looks like this:
 *
 * Row 1: 00000101
 * Row 0: 00000011 00000000 00001100 00000000 00000000
 *
 * Size: 40 bits, determined by Row 0
 * Bits set: { 0, 1, 18, 19 }
 *
 * note: Bits are displayed in msb->lsb, but arrays are left->right
 *
 * * Row 1's bit 0 and bit 2 means Row 0's block 0 and block 2 is non-zero
 * * Row 1's last 3 bits are always zero
 * * Container supports sizes that aren't aligned with block sizes, which means
 *   the last bits of Row 0's last block may be ignored
 */
template <typename BLOCK_INT_T = uint64_t>
class HierarchicalBitset
{
    using row_index_t = size_t;

    static constexpr int smc_blockSize = sizeof(BLOCK_INT_T) * 8;

    // Max size of the top row.
    // A higher number means functions like take() must search the top level for
    // non-zero blocks, but 1 less row to manage. Unlikely this will do anything
    static constexpr int smc_topLevelMaxBlocks = 1;

    // Max bits representable = smc_blockSize^smc_maxRows
    // Due to its exponential nature, this number doesn't need to be that high
    static constexpr int smc_maxRows = 8;

    struct RowBit { row_index_t m_block; int m_bit; };

public:

    struct Row { size_t m_offset; size_t m_size; };
    using rows_t = std::array<Row, smc_maxRows>;

    HierarchicalBitset() = default;

    /**
     * @brief Construct with size
     *
     * @param size [in] Size in bits
     * @param fill [in] Initialize with 1 bits if true
     */
    HierarchicalBitset(size_t size, bool fill = false)
     : m_size{size}
     , m_topLevel{0}
    {
        m_blockCount = calc_blocks_recurse(size, 0, m_topLevel, m_rows);
        m_blocks = std::make_unique<BLOCK_INT_T[]>(m_blockCount);

        if (fill)
        {
            set();
        }
    }

    /**
     * @brief Test if a bit is set
     *
     * @param i [in] Bit number
     *
     * @return True if bit is set, False if not, 3.14159 if the universe broke
     */
    bool test(size_t i) const;

    /**
     * @brief Set all bits to 1
     */
    void set() noexcept
    {
        for (int i = 0; i < m_topLevel + 1; i ++)
        {
            size_t const bits = (i != 0) ? m_rows[i - 1].m_size : m_size;
            set_bits(&m_blocks[m_rows[i].m_offset], bits);
        }

        m_count = m_size;
    }

    /**
     * @return Total number of supported bits
     */
    constexpr size_t size() const noexcept { return m_size; }

    /**
     * @return Number of set bits
     */
    constexpr size_t count() const noexcept { return m_count; }

    /**
     * @brief Set a bit to 1
     *
     * @param i [in] Bit to set
     */
    void set(size_t i)
    {
        bounds_check(i);
        block_set_recurse(0, bit_at(i));
    }

    /**
     * @brief Reset a bit to 0
     *
     * @param i [in] Bit to reset
     */
    void reset(size_t i)
    {
        bounds_check(i);
        block_reset_recurse(0, bit_at(i));
    }

    /**
     * @brief Take multiple bits, clear them, and write their indices to an
     *        iterator.
     *
     * @param out           [out] Output Iterator
     * @param count         [in] Number of bits to take
     *
     * @tparam IT_T         Output iterator type
     * @tparam CONVERT_T    Functor or type to convert index to iterator value
     *
     * @return Remainder from count, non-zero if container becomes empty
     */
    template<typename IT_T, typename CONVERT_T = size_t>
    size_t take(IT_T out, size_t count);

    /**
     * @brief Reallocate to fit a certain number of bits
     *
     * @param size [in] Size in bits
     * @param fill [in] If true, new space will be set to 1
     */
    void resize(size_t size, bool fill = false);

    // TODO: a highest bit function and iterators

    /**
     * @return Index of top row
     */
    constexpr int top_row() const noexcept { return m_topLevel; }

    /**
     * @return Read-only access to row offsets and sizes
     */
    constexpr rows_t const& rows() const noexcept { return m_rows; }

    /**
     * @return Read-only access to block data
     */
    BLOCK_INT_T const* data() const noexcept { return m_blocks.get(); }

private:

    /**
     * @brief Calculate required number of blocks
     *
     * @param bitCount  [in] Bits needed to be represented by this row
     * @param dataUsed  [in] Current number of blocks used
     * @param rLevel    [out] Current or highest level reached
     * @param rLevels   [out] Row offsets and sizes
     *
     * @return Total number of blocks required
     */
    static constexpr size_t calc_blocks_recurse(
            size_t bitCount, size_t dataUsed, int& rLevel, rows_t& rLevels);

    /**
     * @brief Convert bit position to a row block number and block bit number
     */
    static constexpr RowBit bit_at(size_t pos) noexcept
    {
        row_index_t const block = pos / smc_blockSize;
        int const bit = pos % smc_blockSize;
        return { block, bit };
    }

    /**
     * @brief Throw an exception if pos is out of this container's range
     *
     * @param pos [in] Position to check
     */
    constexpr void bounds_check(size_t pos) const
    {
        if (pos >= m_size)
        {
            throw std::out_of_range("Bit position out of range");
        }
    }

    /**
     * @brief Set a bit and recursively set bits of rows above if needed
     *
     * @param level [in] Current level (row)
     * @param pos   [in] Bit to set
     */
    void block_set_recurse(int level, RowBit pos) noexcept;

    /**
     * @brief Resets a bit and recursively resets bits of rows above if needed
     *
     * @param level [in] Current level (row)
     * @param pos   [in] Bit to reset
     */
    void block_reset_recurse(int level, RowBit pos) noexcept;

    /**
     * @brief Recursive function to walk down the hierarchy, arriving at a set
     *        bit at Row 0
     *
     * @param level     [in] Current Level
     * @param blockNum  [in] Row's block number
     * @param rOut      [out] Output iterator to write indices to
     * @param rCount    [out] Remaining values to write to output iteratr
     *
     * @return true if block is non-zero and/or count stopped
     *         false if block is zero
     */
    template<typename IT_T, typename CONVERT_T>
    constexpr bool take_recurse(
            int level, size_t blockNum, IT_T& rOut, size_t& rCount);

    /**
     * @brief Recalculate m_count by counting bits
     */
    void recount();

    /**
     * @brief Recalculate bits of all valid rows above 0
     */
    void recalc_blocks();

    size_t m_blockCount{0};
    std::unique_ptr<BLOCK_INT_T[]> m_blocks;

    rows_t m_rows;
    size_t m_size{0};
    size_t m_count{0};
    int m_topLevel{0};
}; // class HierarchicalBitset


template <typename BLOCK_INT_T>
bool HierarchicalBitset<BLOCK_INT_T>::test(size_t i) const
{
    bounds_check(i);
    RowBit const pos = bit_at(i);

    return bit_test(m_blocks[m_rows[0].m_offset + pos.m_block], pos.m_bit);
}

template <typename BLOCK_INT_T>
template<typename IT_T, typename CONVERT_T>
size_t HierarchicalBitset<BLOCK_INT_T>::take(IT_T out, size_t count)
{
    int level = m_topLevel;

    // Loop through top-level blocks
    for (row_index_t i = 0; i < m_rows[m_topLevel].m_size; i ++)
    {
        take_recurse<IT_T, CONVERT_T>(level, i, out, count);

        if (0 == count)
        {
            break;
        }
    }

    return count;
}

template <typename BLOCK_INT_T>
void HierarchicalBitset<BLOCK_INT_T>::resize(size_t size, bool fill)
{
    HierarchicalBitset replacement(size);

    if (fill)
    {
        replacement.set();
    }

    // copy row 0 to new replacment
    copy_bits(&m_blocks[m_rows[0].m_offset],
              &replacement.m_blocks[replacement.m_rows[0].m_offset],
              std::min(m_size, replacement.m_size));

    replacement.recalc_blocks();
    replacement.recount();

    *this = std::move(replacement);
}

template <typename BLOCK_INT_T>
constexpr size_t HierarchicalBitset<BLOCK_INT_T>::calc_blocks_recurse(
        size_t bitCount, size_t dataUsed, int& rLevel, rows_t& rLevels)
{
    // blocks needed to fit [bitCount] bits
    size_t const blocksRequired = div_ceil(bitCount, smc_blockSize);

    rLevels[rLevel] = { dataUsed, blocksRequired };

    if (blocksRequired > smc_topLevelMaxBlocks)
    {
        rLevel ++;
        return blocksRequired + calc_blocks_recurse(
                blocksRequired, dataUsed + blocksRequired, rLevel, rLevels );
    }

    return blocksRequired;
}

template <typename BLOCK_INT_T>
void HierarchicalBitset<BLOCK_INT_T>::block_set_recurse(
        int level, RowBit pos) noexcept
{
    BLOCK_INT_T &rBlock = m_blocks[m_rows[level].m_offset + pos.m_block];

    BLOCK_INT_T const blockOld = rBlock;
    rBlock |= (BLOCK_INT_T(1) << pos.m_bit);

    if ( blockOld != rBlock )
    {
        // something changed
        if (0 == level)
        {
            m_count ++;
        }

        if ( blockOld == 0 && (level != m_topLevel) )
        {
            // Recurse, as block was previously zero
            block_set_recurse(level + 1, bit_at(pos.m_block));
        }
    }
}

template <typename BLOCK_INT_T>
void HierarchicalBitset<BLOCK_INT_T>::block_reset_recurse(
        int level, RowBit pos) noexcept
{
    BLOCK_INT_T &rBlock = m_blocks[m_rows[level].m_offset + pos.m_block];

    if (rBlock == 0)
    {
        return; // block already zero, do nothing
    }

    BLOCK_INT_T const blockOld = rBlock;
    rBlock &= ~(BLOCK_INT_T(1) << pos.m_bit); // Clear bit

    if ( blockOld != rBlock )
    {
        if (0 == level)
        {
            m_count --;
        }

        if ( (rBlock == 0) && (level != m_topLevel) )
        {
            // Recurse, as block was just made zero
            block_reset_recurse(level + 1, bit_at(pos.m_block));
        }
    }

}

template <typename BLOCK_INT_T>
template<typename IT_T, typename CONVERT_T>
constexpr bool HierarchicalBitset<BLOCK_INT_T>::take_recurse(
        int level, size_t blockNum, IT_T& rOut, size_t& rCount)
{
    BLOCK_INT_T &rBlock = m_blocks[m_rows[level].m_offset + blockNum];

    while (0 != rBlock)
    {
        // Return if enough bits have been taken
        if (0 == rCount)
        {
            return true;
        }

        int const blockBit = ctz(rBlock);
        size_t const rowBit = blockNum * smc_blockSize + blockBit;

        if (0 == level)
        {
            // Take the bit
            *rOut = CONVERT_T(rowBit); // Write row bit number to output
            std::advance(rOut, 1);
            rCount --;
            m_count --;
        }
        else
        {
            // Recurse into row and block below
            if ( take_recurse<IT_T, CONVERT_T>(level - 1, rowBit, rOut, rCount) )
            {
                // Returned true, block below isn't zero, don't clear bit
                continue;
            }
        }

        rBlock &= ~(BLOCK_INT_T(1) << blockBit); // Clear bit
    }

    return false; // Block is zero, no more 1 bits left

}

template <typename BLOCK_INT_T>
void HierarchicalBitset<BLOCK_INT_T>::recount()
{
    m_count = 0;
    for (size_t i = 0; i < m_rows[0].m_size; i ++)
    {
        m_count += std::bitset<smc_blockSize>(
                    m_blocks[m_rows[0].m_offset + i]).count();
    }
}

template <typename BLOCK_INT_T>
void HierarchicalBitset<BLOCK_INT_T>::recalc_blocks()
{
    for (int i = 0; i < m_topLevel; i ++)
    {
        Row const current = m_rows[i + 1];
        Row const below = m_rows[i];

        for (size_t j = 0; j < current.m_size; j ++)
        {
            BLOCK_INT_T blockNew = 0;

            size_t const belowBlocks = std::min<size_t>(
                        smc_blockSize, below.m_size - j * smc_blockSize);

            BLOCK_INT_T currentBit = 1;

            // Evaluate a block for each bit
            for (int k = 0; k < belowBlocks; k ++)
            {
                blockNew |= currentBit * (m_blocks[below.m_offset + k]);
                currentBit <<= 1;
            }

            m_blocks[current.m_offset + j] = blockNew;
        }
    }
}

}
