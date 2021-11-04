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
#include <osp/hierarchical_bitset.h>

#include <gtest/gtest.h>

#include <array>
#include <random>
#include <vector>

using osp::HierarchicalBitset;

/**
 * @brief Generate random integers in ascending order up to a certain maximum
 */
std::vector<int> random_ascending(int seed, size_t maximum)
{
    std::mt19937 gen(seed);
    std::uniform_int_distribution<int> dist(0, 1);
    std::vector<int> testSet;

    for (size_t i = 0; i < maximum; i ++)
    {
        if (dist(gen) == 1)
        {
            testSet.push_back(i);
        }
    }

    return testSet;
}

TEST(HierarchicalBitset, BasicUnaligned)
{
    HierarchicalBitset bitset(129);

    bitset.set(0);
    bitset.set(42);
    bitset.set(128);

    EXPECT_TRUE( bitset.test(0) );
    EXPECT_TRUE( bitset.test(42) );
    EXPECT_TRUE( bitset.test(128) );
    EXPECT_EQ( 3, bitset.count() );

    bitset.reset(0);
    bitset.reset(128);

    EXPECT_FALSE( bitset.test(0) );
    EXPECT_TRUE( bitset.test(42) );
    EXPECT_FALSE( bitset.test(128) );
    EXPECT_EQ( bitset.count(), 1 );

    // Try taking 11 bits, but there's only 1 left (42)
    std::array<int, 11> toTake;
    toTake.fill(-1); // make sure garbage values don't ruin the test
    int const remainder = bitset.take(toTake.begin(), 11);

    EXPECT_EQ( remainder, 10 );
    EXPECT_EQ( toTake[0], 42 );
    EXPECT_EQ( bitset.count(), 0 );
}

TEST(HierarchicalBitset, BasicAligned)
{
    HierarchicalBitset bitset(128);

    bitset.set(0);
    bitset.set(42);
    bitset.set(127);

    EXPECT_TRUE( bitset.test(0) );
    EXPECT_TRUE( bitset.test(42) );
    EXPECT_TRUE( bitset.test(127) );
    EXPECT_EQ( 3, bitset.count() );

    bitset.reset(0);
    bitset.reset(127);

    EXPECT_FALSE( bitset.test(0) );
    EXPECT_TRUE( bitset.test(42) );
    EXPECT_FALSE( bitset.test(127) );
    EXPECT_EQ( bitset.count(), 1 );

    // Try taking 11 bits, but there's only 1 left (42)
    std::array<int, 11> toTake;
    toTake.fill(-1); // make sure garbage values don't ruin the test
    int const remainder = bitset.take(toTake.begin(), 11);

    EXPECT_EQ( remainder, 10 );
    EXPECT_EQ( toTake[0], 42 );
    EXPECT_EQ( bitset.count(), 0 );
}

TEST(HierarchicalBitset, TakeRandomSet)
{
    constexpr size_t const sc_max = 13370;
    constexpr size_t const sc_seed = 420;

    std::vector<int> const testSet = random_ascending(sc_seed, sc_max);

    HierarchicalBitset bitset(sc_max);

    for (int i : testSet)
    {
        bitset.set(i);
    }

    EXPECT_EQ( bitset.count(), testSet.size() );

    std::vector<int> results(testSet.size());

    int const remainder = bitset.take(results.begin(), testSet.size() + 12);

    EXPECT_EQ( remainder, 12 );
    EXPECT_EQ( results, testSet );
    EXPECT_EQ( bitset.count(), 0 );
}

TEST(HierarchicalBitset, Resizing)
{
    HierarchicalBitset bitset(20);

    bitset.set(5);

    // Resize 20 -> 30 with fill enabled, creates 10 new bits starting at 20
    bitset.resize(30, true);

    EXPECT_TRUE( bitset.test(5) );
    EXPECT_EQ( bitset.count(), 11 );

    // Resize down to 6, this removes the 10 bits
    bitset.resize(6);

    EXPECT_TRUE( bitset.test(5) );
    EXPECT_EQ( bitset.count(), 1 );
}
