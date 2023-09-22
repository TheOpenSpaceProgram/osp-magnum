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
#include <osp/core/shared_string.h>

#include <gtest/gtest.h>

#include <string>
#include <string_view>

#define COMPARE_EQUAL(LHS, RHS) \
    ASSERT_EQ(LHS, RHS);        \
    ASSERT_EQ(RHS, LHS);        \
    ASSERT_LE(LHS, RHS);        \
    ASSERT_LE(RHS, LHS);        \
    ASSERT_GE(LHS, RHS);        \
    ASSERT_GE(RHS, LHS);

#define COMPARE_NOT_EQUAL(LHS, RHS) \
    ASSERT_NE(LHS, RHS);            \
    ASSERT_NE(RHS, LHS);

// Some trivial assertions. No lifetime tests yet.
TEST(SharedString, BasicAssertions)
{
    // Expect two strings to be equal.
    COMPARE_EQUAL(osp::SharedString::create("hello"), "hello");
    COMPARE_EQUAL(osp::SharedString::create("hello"), std::string_view("hello"));
    COMPARE_EQUAL(osp::SharedString::create("hello"), std::string("hello"));
    COMPARE_EQUAL(osp::SharedString::create("hello"), osp::SharedString::create("hello"));

    // Expect two strings not to be equal.
    COMPARE_NOT_EQUAL(osp::SharedString::create("hello"), "world");
    COMPARE_NOT_EQUAL(osp::SharedString::create("hello"), std::string_view("world"));
    COMPARE_NOT_EQUAL(osp::SharedString::create("hello"), std::string("world"));
    COMPARE_NOT_EQUAL(osp::SharedString::create("hello"), osp::SharedString::create("world"));
}
