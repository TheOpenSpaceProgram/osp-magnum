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

class HierarchicalBitset
{
    // maybe template these eventually?

    using block_int_t = uint64_t;
    using row_index_t = size_t;

    static constexpr int smc_blockSize = sizeof(block_int_t) * 8;
    static constexpr int smc_topLevelMaxBlocks = 1;
    static constexpr int smc_depth = 8;

    struct Pos { row_index_t m_block; int m_bit; };

public:

    struct Row { size_t m_offset; size_t m_size; };
    using rows_t = std::array<Row, smc_depth>;

    HierarchicalBitset() = default;
    HierarchicalBitset(size_t size, bool fill = false)
     : m_size{size}
     , m_topLevel{0}
    {
        m_blockCount = calc_blocks_recurse(size, 0, m_topLevel, m_rows);
        m_blocks = std::make_unique<block_int_t[]>(m_blockCount);

        if (fill)
        {
            set();
        }
    }

    bool test(size_t i)
    {
        bounds_check(i);
        Pos const pos = at(i);

        return bit_test(m_blocks[m_rows[0].m_offset + pos.m_block], pos.m_bit);
    }

    void set() noexcept
    {
        for (int i = 0; i < m_topLevel + 1; i ++)
        {
            size_t const bits = (i != 0) ? m_rows[i - 1].m_size : m_size;
            set_bits(&m_blocks[m_rows[i].m_offset], bits);
        }

        m_count = m_size;
    }

    constexpr size_t size() const noexcept { return m_size; }

    constexpr size_t count() const noexcept { return m_count; }

    void set(size_t i)
    {
        bounds_check(i);
        block_set_recurse(0, at(i));
    }

    void reset(size_t i)
    {
        bounds_check(i);
        block_reset_recurse(0, at(i));
    }

    template<typename IT_T, typename CONVERT_T = size_t>
    size_t take(IT_T out, size_t count)
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

    void resize(size_t size, bool fill = false)
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

    constexpr int top_row() const noexcept { return m_topLevel; }
    constexpr rows_t const& rows() const noexcept { return m_rows; }
    block_int_t const* data() const noexcept { return m_blocks.get(); }

private:

    static constexpr size_t calc_blocks_recurse(
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

    constexpr void bounds_check(size_t pos)
    {
        if (pos >= m_size)
        {
            throw std::out_of_range("Bit position out of range");
        }
    }

    static constexpr Pos at(size_t pos) noexcept
    {
        row_index_t const block = pos / smc_blockSize;
        int const bit = pos % smc_blockSize;
        return { block, bit };
    }

    void block_set_recurse(int level, Pos pos) noexcept
    {
        block_int_t &rBlock = m_blocks[m_rows[level].m_offset + pos.m_block];

        block_int_t const blockOld = rBlock;
        rBlock |= (block_int_t(1) << pos.m_bit);

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
                block_set_recurse(level + 1, at(pos.m_block));
            }
        }
    }

    void block_reset_recurse(int level, Pos pos) noexcept
    {
        block_int_t &rBlock = m_blocks[m_rows[level].m_offset + pos.m_block];

        if (rBlock == 0)
        {
            return; // block already zero, do nothing
        }

        block_int_t const blockOld = rBlock;
        rBlock &= ~(block_int_t(1) << pos.m_bit); // Clear bit

        if ( blockOld != rBlock )
        {
            if (0 == level)
            {
                m_count --;
            }

            if ( (rBlock == 0) && (level != m_topLevel) )
            {
                // Recurse, as block was just made zero
                block_reset_recurse(level + 1, at(pos.m_block));
            }
        }

    }

    /**
     * @return true if block is non-zero, false if block is zero
     */
    template<typename IT_T, typename CONVERT_T>
    constexpr bool take_recurse(int level, size_t blockNum, IT_T& out, size_t& rCount)
    {
        block_int_t &rBlock = m_blocks[m_rows[level].m_offset + blockNum];

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
                *out = CONVERT_T(rowBit); // Write row bit number to output
                std::advance(out, 1);
                rCount --;
                m_count --;
            }
            else
            {
                // Recurse into row and block below
                if ( take_recurse<IT_T, CONVERT_T>(level - 1, rowBit, out, rCount) )
                {
                    // Returned true, block below isn't zero, don't clear bit
                    continue;
                }
            }

            rBlock &= ~(block_int_t(1) << blockBit); // Clear bit
        }

        return false; // Block is zero, no more 1 bits left

    }

    /**
     * @brief Recalculate m_count by counting bits
     */
    void recount()
    {
        m_count = 0;
        for (size_t i = 0; i < m_rows[0].m_size; i ++)
        {
            m_count += std::bitset<smc_blockSize>(
                        m_blocks[m_rows[0].m_offset + i]).count();
        }
    }

    void recalc_blocks()
    {
        for (int i = 0; i < m_topLevel; i ++)
        {
            Row const current = m_rows[i + 1];
            Row const below = m_rows[i];

            for (size_t j = 0; j < current.m_size; j ++)
            {
                block_int_t blockNew = 0;

                size_t const belowBlocks = std::min<size_t>(
                            smc_blockSize, below.m_size - j * smc_blockSize);

                block_int_t currentBit = 1;

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

    size_t m_blockCount{0};
    std::unique_ptr<block_int_t[]> m_blocks;

    rows_t m_rows;
    size_t m_size{0};
    size_t m_count{0};
    int m_topLevel{0};
};

}
