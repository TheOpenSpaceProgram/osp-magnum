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

#include "SubdivSkeleton.h"

#include <osp/core/math_types.h>

#include <vector>

#include <cstddef>

namespace planeta
{

// Fundamental geometric properties of an icosahedron
inline constexpr std::uint8_t const gc_icoVrtxCount = 12;
inline constexpr std::uint8_t const gc_icoTriCount = 20;

// Ratio between an icosahedron's edge length and radius
// = sqrt(10.0 + 2.0 * sqrt(5.0)) / 4.0;
inline constexpr double const sc_icoEdgeRatio = 0.95105651629f;

// The 20 faces of the icosahedron {Top, Left, Right}
// Each number refers to one of 12 initial vertices
inline constexpr std::array<std::array<uint8_t, 3>, 20> sc_icoTriLUT
{{
    { 0,  2,  1},  { 0,  3,  2},  { 0,  4,  3},  { 0,  5,  4},  { 0,  1,  5},
    { 8,  1,  2},  { 2,  7,  8},  { 7,  2,  3},  { 3,  6,  7},  { 6,  3,  4},
    { 4, 10,  6},  {10,  4,  5},  { 5,  9, 10},  { 9,  5,  1},  { 1,  8,  9},
    {11,  7,  6},  {11,  8,  7},  {11,  9,  8},  {11, 10,  9},  {11,  6, 10}
}};

/**
 * @brief Create an icosahedron shaped Triangle Mesh Skeleton
 *
 * @param radius        [in] Radius of icosahedron in meters
 * @param pow2scale     [in] Scale for rPositions, 2^pow2scale units = 1 meter
 * @param vrtxIds       [out] Vertex IDs out for initial 12 vertices
 * @param triIds        [out] Triangle IDs out for initial 20 triangles
 * @param rPositions    [out] Position data to allocate
 * @param rNormals      [out] Normal data to allocate
 *
 * @return SubdivTriangleSkeleton for keeping track of Vertex and Triangle IDs
 */
SubdivTriangleSkeleton create_skeleton_icosahedron(
        double radius, int pow2scale,
        Corrade::Containers::StaticArrayView<gc_icoVrtxCount, SkVrtxId> vrtxIds,
        Corrade::Containers::StaticArrayView<gc_icoTriCount, SkTriId> triIds,
        std::vector<osp::Vector3l> &rPositions,
        std::vector<osp::Vector3> &rNormals);

/**
 * @brief Calculate positions and normals for 3 new vertices created when
 *        subdividing a Triangle along an icosahedron sphere
 *
 * @param radius        [in] Radius of icosahedron in meters
 * @param pow2scale     [in] Scale for rPositions, 2^pow2scale units = 1 meter
 * @param vrtxCorners   [in] Vertex IDs for the main Triangle's corners
 * @param vrtxMid       [in] Vertex IDs of the 3 new middle vertices
 * @param rPositions    [ref] Position data
 * @param rNormals      [ref] Normal data
 */
void ico_calc_middles(
        double radius, int pow2scale,
        std::array<SkVrtxId, 3> const vrtxCorners,
        std::array<SkVrtxId, 3> const vrtxMid,
        std::vector<osp::Vector3l> &rPositions,
        std::vector<osp::Vector3> &rNormals);

/**
 * @brief Calculate positions for vertices along an edge from Vertex A to B
 *
 * Corresponds to SubdivTriangleSkeleton::vrtx_create_chunk_edge_recurse
 *
 * @param radius        [in] Radius of icosahedron in meters
 * @param pow2scale     [in] Scale for rPositions, 2^pow2scale units = 1 meter
 * @param level         [in] Number of times to subdivide
 * @param a             [in] Vertex ID of A
 * @param b             [in] Vertex ID of B
 * @param vrtxs         [in] Vertex IDs of vertices between A and B
 * @param rPositions    [ref] Position data
 * @param rNormals      [ref] Normal data
 */
void ico_calc_chunk_edge_recurse(
        double radius, int pow2scale, unsigned int level,
        SkVrtxId a, SkVrtxId b,
        ArrayView_t<SkVrtxId const> vrtxs,
        std::vector<osp::Vector3l> &rPositions,
        std::vector<osp::Vector3> &rNormals);


}
