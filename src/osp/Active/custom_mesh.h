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

#include <osp/id_registry.h>

#include <Magnum/Trade/MeshData.h>

#include <optional>
#include <variant>
#include <vector>

namespace osp::active
{

enum class CustomMeshId : uint32_t {};
using CustomMeshStorage_t = osp::UniqueIdRegistry<CustomMeshId>::Storage_t;

struct ACtxCustomMeshes
{

    struct Create
    {
        CustomMeshId m_id;
    };

    struct Delete
    {
        CustomMeshStorage_t m_id;
    };

    struct BufferUpdate
    {
        size_t m_offset;
        size_t m_size;
        CustomMeshId m_mesh;
    };

    struct VertexBufferUpdate : public BufferUpdate { };

    struct IndexBufferUpdate : public BufferUpdate { };

    using Command_t = std::variant<Create, Delete, VertexBufferUpdate,
                                   IndexBufferUpdate>;

    template<typename ... ARGS_T>
    CustomMeshStorage_t emplace(ARGS_T&& ... args)
    {
        CustomMeshStorage_t storage = m_meshIds.create();
        size_t const idInt = size_t(storage.value());

        m_meshDatas.resize(std::max(m_meshDatas.size(), idInt + 1));
        m_meshDatas[idInt].emplace(std::forward(args)...);

        return storage;
    }

    std::vector<Command_t> m_commands;
    osp::UniqueIdRegistry<CustomMeshId> m_meshIds;
    //IdRefCount<CustomMeshId> m_meshIdRefs;
    std::vector< std::optional<Magnum::Trade::MeshData> > m_meshDatas;
};

struct ACompCustomMesh
{
    CustomMeshStorage_t m_id;
};


}
