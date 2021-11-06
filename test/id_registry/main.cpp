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
#include <osp/id_registry.h>

#include <gtest/gtest.h>

#include <random>

enum class Id : uint64_t { };

// Basic intended use test of managing IDs
TEST(IdRegistry, ManageIds)
{
    osp::IdRegistry<Id> registry;

    Id idA = registry.create();
    Id idB = registry.create();
    Id idC = registry.create();

    EXPECT_TRUE( registry.exists(idA) );
    EXPECT_TRUE( registry.exists(idB) );
    EXPECT_TRUE( registry.exists(idC) );

    EXPECT_EQ( registry.size(), 3 );

    ASSERT_NE( idA, idB );
    ASSERT_NE( idB, idC );
    ASSERT_NE( idC, idA );

    registry.remove( idB );

    EXPECT_TRUE( registry.exists(idA) );
    EXPECT_FALSE( registry.exists(idB) );
    EXPECT_TRUE( registry.exists(idC) );

    EXPECT_EQ( registry.size(), 2 );

    std::array<Id, 32> ids;
    registry.create(std::begin(ids), ids.size());

    for (Id id : ids)
    {
        EXPECT_TRUE( registry.exists(id) );
    }

    EXPECT_EQ( registry.size(), 34 );
}

// A more chaotic test of repeated adding a random amount of new IDs then
// deleting half of them randomly
TEST(IdRegistry, RandomCreationAndDeletion)
{
    constexpr size_t const sc_seed = 69;
    constexpr size_t const sc_createMax = 100;
    constexpr size_t const sc_createMin = 60;
    constexpr size_t const sc_repetitions = 32;

    osp::IdRegistry<Id> registry;

    std::set<Id> idSet;

    std::mt19937 gen(sc_seed);
    std::uniform_int_distribution<int> distCreate(sc_createMin, sc_createMax);
    std::uniform_int_distribution<int> distFlip(0, 1);


    for (int i = 0; i < sc_repetitions; i ++)
    {
        // Create a bunch of new IDs

        size_t const toCreate = distCreate(gen);
        std::array<Id, sc_createMax> newIds;

        registry.create(std::begin(newIds), toCreate);
        idSet.insert(std::begin(newIds), std::begin(newIds) + toCreate);

        // Remove about half of the IDs

        std::vector<Id> toErase;

        for (Id id : idSet)
        {
            if (distFlip(gen) == 1)
            {
                registry.remove(id);
                toErase.push_back(id);

                EXPECT_FALSE( registry.exists(id) );
            }
        }

        for (Id id : toErase)
        {
            idSet.erase(id);
        }

        // Check if all IDs are still valid

        for (Id id : idSet)
        {
            EXPECT_TRUE( registry.exists(id) );
        }

        EXPECT_EQ( idSet.size(), registry.size() );
    }
}
