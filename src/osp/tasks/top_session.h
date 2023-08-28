/**
 * Open Space Program
 * Copyright © 2019-2022 Open Space Program Project
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

#include "tasks.h"
#include "top_utils.h"

#include <entt/core/any.hpp>

#include <cassert>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

#include <osp/unpack.h>

#define OSP_AUX_DCDI_C(session, topData, count, ...) \
    session.m_data.resize(count); \
    osp::top_reserve(topData, 0, session.m_data.begin(), session.m_data.end()); \
    auto const [__VA_ARGS__] = osp::unpack<count>(session.m_data)
#define OSP_AUX_DCDI_B(x) x
#define OSP_AUX_DCDI_A(...) OSP_AUX_DCDI_B(OSP_AUX_DCDI_C(__VA_ARGS__))

/**
 * @brief
 */
#define OSP_DECLARE_CREATE_DATA_IDS(session, topData, arglist) OSP_AUX_DCDI_A(session, topData, arglist);


#define OSP_AUX_DGDI_C(session, count, ...) \
    auto const [__VA_ARGS__] = osp::unpack<count>(session.m_data)
#define OSP_AUX_DGDI_B(x) x
#define OSP_AUX_DGDI_A(...) OSP_AUX_DGDI_B(OSP_AUX_DGDI_C(__VA_ARGS__))

#define OSP_DECLARE_GET_DATA_IDS(session, arglist) OSP_AUX_DGDI_A(session, arglist);



namespace osp
{

/**
 * @brief A convenient group of Pipelines that work together to support a certain feature.
 *
 * Sessions only store vectors of integer IDs, and don't does not handle
 * ownership on its own. Close using osp::top_close_session before destruction.
 */
struct Session
{
    template <std::size_t N>
    [[nodiscard]] std::array<TopDataId, N> acquire_data(ArrayView<entt::any> topData)
    {
        std::array<TopDataId, N> out;
        top_reserve(topData, 0, std::begin(out), std::end(out));
        m_data.assign(std::begin(out), std::end(out));
        return out;
    }

    template<typename TGT_STRUCT_T, typename BUILDER_T>
    TGT_STRUCT_T create_pipelines(BUILDER_T &rBuilder)
    {
        static_assert(sizeof(TGT_STRUCT_T) % sizeof(PipelineDefBlank_t) == 0);
        constexpr std::size_t count = sizeof(TGT_STRUCT_T) / sizeof(PipelineDefBlank_t);

        std::type_info const& info = typeid(TGT_STRUCT_T);
        m_structHash = info.hash_code();
        m_structName = info.name();

        m_pipelines.resize(count);

        return rBuilder.template create_pipelines<TGT_STRUCT_T>(ArrayView<PipelineId>(m_pipelines.data(), count));
    }

    template<typename TGT_STRUCT_T>
    [[nodiscard]] TGT_STRUCT_T get_pipelines() const
    {
        static_assert(sizeof(TGT_STRUCT_T) % sizeof(PipelineId) == 0);
        constexpr std::size_t count = sizeof(TGT_STRUCT_T) / sizeof(PipelineDefBlank_t);

        [[maybe_unused]] std::type_info const& info = typeid(TGT_STRUCT_T);
        LGRN_ASSERTMV(m_structHash == info.hash_code() && count == m_pipelines.size(),
                      "get_pipeline must use the same struct previously given to get_pipelines",
                      m_structHash, m_structName,
                      info.hash_code(), info.name(),
                      m_pipelines.size());

        alignas(TGT_STRUCT_T) std::array<unsigned char, sizeof(TGT_STRUCT_T)> bytes;
        TGT_STRUCT_T *pOut = new(bytes.data()) TGT_STRUCT_T;\

        for (std::size_t i = 0; i < count; ++i)
        {
            unsigned char *pDefBytes = bytes.data() + sizeof(PipelineDefBlank_t)*i;
            *reinterpret_cast<PipelineId*>(pDefBytes + offsetof(PipelineDefBlank_t, m_value)) = m_pipelines[i];
        }

        return *pOut;
    }

    std::vector<TopDataId>  m_data;
    std::vector<PipelineId> m_pipelines;
    std::vector<TaskId>     m_tasks;

    PipelineId              m_cleanup { lgrn::id_null<PipelineId>() };

    std::size_t             m_structHash{0};
    std::string             m_structName;

}; // struct Session

struct SessionGroup
{
    std::vector<Session>    m_sessions;
    TaskEdges               m_edges;
};


} // namespace osp
