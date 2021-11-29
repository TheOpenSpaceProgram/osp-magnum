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

#include "SubdivSkeleton.h"

using namespace planeta;

SkTriGroupId SubdivTriangleSkeleton::tri_subdiv(SkTriId triId,
                                                std::array<SkVrtxId, 3> vrtxMid)
{
    SkeletonTriangle &rTri = tri_at(triId);

    if (rTri.m_children.has_value())
    {
        throw std::runtime_error("SkeletonTriangle is already subdivided");
    }

    SkTriGroup const& parentGroup = m_triData[size_t(tri_group_id(triId))];

    // non-macro shorthands
    auto corner = [&rTri] (int i) -> SkVrtxId { return rTri.m_vertices[i]; };
    auto middle = [&vrtxMid] (int i) -> SkVrtxId { return vrtxMid[i]; };

    // c?: Corner vertex corner(?) aka: rTri.m_vertices[?]
    // m?: Middle vertex middle(?) aka: vrtxMid[?]
    // t?: Skeleton Triangle
    //
    //          c0
    //          /\                 Vertex order reminder:
    //         /  \                0:Top   1:Left   2:Right
    //        / t0 \                        0
    //    m0 /______\ m2                   / \
    //      /\      /\                    /   \
    //     /  \ t3 /  \                  /     \
    //    / t1 \  / t2 \                1 _____ 2
    //   /______\/______\
    // c1       m1       c2
    //
    SkTriGroupId const groupId = tri_group_create(
        parentGroup.m_depth + 1,
        triId,
        {{
            { corner(0), middle(0), middle(2) }, // 0: Top
            { middle(0), corner(1), middle(1) }, // 1: Left
            { middle(2), middle(1), corner(2) }, // 2: Right
            { middle(1), middle(2), middle(0) }  // 3: Center
        }}
    );

    rTri.m_children = groupId;

    return groupId;
}
