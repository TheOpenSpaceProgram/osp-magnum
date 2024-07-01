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

/**
 * @file
 * @brief Functions and tables for creating spherical triangle skeletons.
 *
 * It's possible to write similar functions to support non-spherical terrain.
 */
#pragma once

#include "skeleton.h"
#include "geometry.h"

#include <osp/core/math_types.h>

#include <cstdint>

namespace planeta
{

// Fundamental geometric properties of an icosahedron
inline constexpr std::uint8_t const gc_icoVrtxCount = 12;
inline constexpr std::uint8_t const gc_icoTriCount = 20;

// Tables generated with scripts/icosahedron_tables.py

/**
 * @brief Positions of the 12 vertices on an icosahedron
 */
inline constexpr std::array<osp::Vector3d, 12> gc_icoVrtxPos
{{
    { 0.000000000000000e+0,  0.000000000000000e+0,  1.000000000000000e+0},
    { 8.944271909999159e-1,  0.000000000000000e+0,  4.472135954999579e-1},
    { 2.763932022500210e-1, -8.506508083520400e-1,  4.472135954999579e-1},
    {-7.236067977499790e-1, -5.257311121191336e-1,  4.472135954999579e-1},
    {-7.236067977499790e-1,  5.257311121191336e-1,  4.472135954999579e-1},
    { 2.763932022500210e-1,  8.506508083520400e-1,  4.472135954999579e-1},
    {-8.944271909999159e-1,  0.000000000000000e+0, -4.472135954999579e-1},
    {-2.763932022500210e-1, -8.506508083520400e-1, -4.472135954999579e-1},
    { 7.236067977499790e-1, -5.257311121191336e-1, -4.472135954999579e-1},
    { 7.236067977499790e-1,  5.257311121191336e-1, -4.472135954999579e-1},
    {-2.763932022500210e-1,  8.506508083520400e-1, -4.472135954999579e-1},
    { 0.000000000000000e+0,  0.000000000000000e+0, -1.000000000000000e+0}
}};

/**
 * @brief Indices for the 20 triangular faces of the icosahedron {Top, Left, Right}
 */
inline constexpr std::array<std::array<uint8_t, 3>, 20> gc_icoIndx
{{
    { 0,  2,  1},  { 0,  3,  2},  { 0,  4,  3},  { 0,  5,  4},  { 0,  1,  5},
    { 8,  1,  2},  { 2,  7,  8},  { 7,  2,  3},  { 3,  6,  7},  { 6,  3,  4},
    { 4, 10,  6},  {10,  4,  5},  { 5,  9, 10},  { 9,  5,  1},  { 1,  8,  9},
    {11,  7,  6},  {11,  8,  7},  {11,  9,  8},  {11, 10,  9},  {11,  6, 10}
}};

/**
 * @brief Neighbor indices along edges {0->1, 1->2, 2->0} for the 20 icosahedron faces
 */
inline constexpr std::array<std::array<uint8_t, 3>, 20> gc_icoNeighbors
{{
    { 1,  5,  4},  { 2,  7,  0},  { 3,  9,  1},  { 4, 11,  2},  { 0, 13,  3},
    {14,  0,  6},  { 7, 16,  5},  { 6,  1,  8},  { 9, 15,  7},  { 8,  2, 10},
    {11, 19,  9},  {10,  3, 12},  {13, 18, 11},  {12,  4, 14},  { 5, 17, 13},
    {16,  8, 19},  {17,  6, 15},  {18, 14, 16},  {19, 12, 17},  {15, 10, 18}
}};

/**
 * @brief Icosahedron Minimum Edge Length vs Subdiv Levels, radius = 1.0
 */
inline constexpr std::array<float, 24> const gc_icoMinEdgeVsLevel
{
    1.05146222e+0f,  5.46533058e-1f,  2.75904484e-1f,  1.38283174e-1f,
    6.91829904e-2f,  3.45966718e-2f,  1.72989830e-2f,  8.64957239e-3f,
    4.32479631e-3f,  2.16239942e-3f,  1.08119987e-3f,  5.40599953e-4f,
    2.70299979e-4f,  1.35149990e-4f,  6.75749950e-5f,  3.37874975e-5f,
    1.68937487e-5f,  8.44687437e-6f,  4.22343719e-6f,  2.11171859e-6f,
    1.05585930e-6f,  5.27929648e-7f,  2.63964824e-7f,  1.31982412e-7f
};

/**
 * @brief Icosahedron Maximum Edge Length vs Subdiv Levels, radius = 1.0
 */
inline constexpr std::array<float, 24> const gc_icoMaxEdgeVsLevel
{{
    1.05146222e+0f,  6.18033989e-1f,  3.24919696e-1f,  1.64647160e-1f,
    8.26039665e-2f,  4.13372560e-2f,  2.06730441e-2f,  1.03370743e-2f,
    5.16860619e-3f,  2.58431173e-3f,  1.29215694e-3f,  6.46078606e-4f,
    3.23039320e-4f,  1.61519662e-4f,  8.07598312e-5f,  4.03799157e-5f,
    2.01899578e-5f,  1.00949789e-5f,  5.04748946e-6f,  2.52374473e-6f,
    1.26187236e-6f,  6.30936182e-7f,  3.15468091e-7f,  1.57734046e-7f
}};


/**
 * @brief Tower height required to clear the horizon over an edge vs Subdiv levels, radius = 1.0
 *
 * If identical towers were built on the two vertices spanning an edge, this is how high each tower
 * needs to be in order to see each other over the horizon.
 */
inline constexpr std::array<float, 24> const gc_icoTowerOverHorizonVsLevel
{
    1.75570505e-1f,  3.95676520e-2f,  9.65341549e-3f,  2.39888395e-3f,
    5.98823224e-4f,  1.49649798e-4f,  3.74089507e-5f,  9.35201901e-6f,
    2.33799109e-6f,  5.84496918e-7f,  1.46124176e-7f,  3.65310407e-8f,
    9.13275996e-9f,  2.28318998e-9f,  5.70797494e-10f, 1.42699373e-10f,
    3.56748433e-11f, 8.91871083e-12f, 2.22967771e-12f, 5.57419427e-13f,
    1.39354857e-13f, 3.48387142e-14f, 8.70967855e-15f, 2.17741964e-15f
};

/**
 * @brief Create an icosahedron shaped Triangle Mesh Skeleton
 *
 * @param radius        [in] Radius of icosahedron in meters
 * @param vrtxIds       [out] Vertex IDs out for initial 12 vertices
 * @param groupIds      [out] Triangle Group IDs out for initial 5 groups for 5*4 triangles
 * @param triIds        [out] Triangle IDs out for initial 20 triangles
 * @param rSkData       [out] Vertex data out
 *
 * @return SubdivTriangleSkeleton for keeping track of Vertex and Triangle IDs
 */
SubdivTriangleSkeleton create_skeleton_icosahedron(
        double                                                  radius,
        Corrade::Containers::StaticArrayView<12, SkVrtxId>      vrtxIds,
        Corrade::Containers::StaticArrayView<5,  SkTriGroupId>  groupIds,
        Corrade::Containers::StaticArrayView<20, SkTriId>       triIds,
        SkeletonVertexData                                      &rSkData);

/**
 * @brief Calculate positions and normals for 3 new vertices created when
 *        subdividing a Triangle along an icosahedron sphere
 *
 * @param radius        [in] Radius of icosahedron in meters
 * @param vrtxCorners   [in] Vertex IDs for the main Triangle's corners
 * @param vrtxMid       [in] Vertex IDs of the 3 new middle vertices
 * @param rSkData       [out] Vertex data out
 */
void ico_calc_middles(
        double                                                  radius,
        std::array<SkVrtxId, 3>                                 vrtxCorners,
        std::array<osp::MaybeNewId<SkVrtxId>, 3>                vrtxMid,
        SkeletonVertexData                                      &rSkData);

/**
 * @brief Calculate positions for vertices along an edge from Vertex A to B
 *
 * Corresponds to SubdivTriangleSkeleton::vrtx_create_chunk_edge_recurse
 *
 * @param radius        [in] Radius of icosahedron in meters
 * @param level         [in] Number of times to subdivide
 * @param cornerA       [in] Vertex on one end
 * @param cornerB       [in] Vertex on the other end lol
 * @param vrtxEdge      [in] Vertices between corner A and B, position and normal will be written to
 * @param rSkData       [out] Vertex data out
 */
void ico_calc_chunk_edge(
        double                                                  radius,
        std::uint8_t                                            level,
        SkVrtxId                                                a,
        SkVrtxId                                                b,
        osp::ArrayView<osp::MaybeNewId<SkVrtxId> const>         vrtxEdge,
        SkeletonVertexData                                      &rSkData);


/**
 * @brief Calculate center of a triangle given a spherical terrain mesh, writes to sktriCenter
 *
 * This accounts for the min/max height of terrain elevation
 */
void ico_calc_sphere_tri_center(
        SkTriGroupId                    groupId,
        double                          maxRadius,
        double                          height,
        SubdivTriangleSkeleton    const &rSkel,
        SkeletonVertexData              &rSkData);

}
