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

/**
 * @file
 * @brief
 */
#pragma once


#include <osp/core/strong_id.h>

#include <longeron/id_management/registry_stl.hpp>
#include <longeron/id_management/refcount.hpp>

#include <array>

namespace planeta
{

class SubdivTriangleSkeleton;

/// Skeleton Vertex ID
using SkVrtxId          = osp::StrongId<std::uint32_t, struct DummyForSkVrtxId>;

/// Skeleton Vertex ID owner; lifetime holds a refcount to a SkVrtxId
using SkVrtxOwner_t     = lgrn::IdOwner<SkVrtxId, SubdivTriangleSkeleton>;

using SkTriId           = osp::StrongId<std::uint32_t, struct DummyForSkTriId>;
using SkTriGroupId      = osp::StrongId<std::uint32_t, struct DummyForSkTriGroupId>;
using SkTriOwner_t      = lgrn::IdRefCount<SkTriId>::Owner_t;


using ChunkId = osp::StrongId<uint16_t, struct DummyForChunkId>;
using SharedVrtxId = osp::StrongId<uint32_t, struct DummyForSharedVrtxId>;

using SharedVrtxOwner_t = lgrn::IdRefCount<SharedVrtxId>::Owner_t;

// Index to a mesh vertex, unaware of vertex size;
using VertexIdx = std::uint32_t;

// IDs for any chunk's shared vertices; from 0 to m_chunkSharedCount
using ChunkLocalSharedId = osp::StrongId<uint16_t, struct DummyForChunkLocalSharedId>;

// IDs for any chunk's fill vertices; from 0 to m_chunkSharedCount
enum class ChunkLocalFillId : uint16_t {};





};
