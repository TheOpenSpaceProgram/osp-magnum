/**
 * Open Space Program
 * Copyright © 2019-2024 Open Space Program Project
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

#include "geometry.h"

namespace planeta
{


void BasicChunkMeshGeometry::resize(ChunkSkeleton const& skCh, ChunkMeshBufferInfo const& info)
{
    using Corrade::Containers::Array;

    auto const maxChunks     = skCh.m_chunkIds.capacity();
    auto const maxSharedVrtx = skCh.m_sharedIds.capacity();

    osp::BufferFormatBuilder formatBuilder;
    vbufPositions = formatBuilder.insert_block<osp::Vector3>(info.vrtxTotal);
    vbufNormals   = formatBuilder.insert_block<osp::Vector3>(info.vrtxTotal);

    vrtxBuffer = Array<std::byte>    (Corrade::ValueInit, formatBuilder.total_size());
    indxBuffer = Array<osp::Vector3u>(Corrade::ValueInit, info.faceTotal);

    chunkFanNormalContrib  .resize(maxChunks * info.fanMaxSharedCount);
    chunkFillSharedNormals .resize(maxChunks * skCh.m_chunkSharedCount, osp::Vector3{osp::ZeroInit});
    sharedNormalSum        .resize(maxSharedVrtx, osp::Vector3{osp::ZeroInit});
    sharedPosNoHeightmap   .resize(maxSharedVrtx, osp::Vector3{osp::ZeroInit});
}

} // namespace planeta
