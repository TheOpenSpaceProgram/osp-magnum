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

#include "SubdivTriangleMesh.h"

#include <Magnum/Math/Vector3.h>

using namespace planeta;

ChunkedTriangleMesh planeta::make_subdivtrimesh_general(
        unsigned int chunkMax, unsigned int subdivLevels,
        unsigned int vrtxSize)
{
    // calculate stuff here

    using std::sqrt;
    using std::ceil;
    using std::pow;

    float const C = chunkMax;
    float const L = subdivLevels;

    // Calculate a fair number of shared vertices, based on a triangular
    // tiling pattern.
    // worked out here: https://www.desmos.com/calculator/ffd8lraosl
    unsigned int const sharedMax
            = ceil( ( 3.0f*C + sqrt(6.0f)*sqrt(C) ) * pow(2.0f, L-1) - C + 1 );

    unsigned int const fanMax
            = ChunkedTriangleMesh::smc_minFansVsLevel[subdivLevels] * 2;

    // i don't know if this is horrible of acceptable
    return ChunkedTriangleMesh(chunkMax, subdivLevels, sharedMax, vrtxSize,
                               fanMax);
}
