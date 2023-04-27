/**
 * Open Space Program
 * Copyright © 2019-2023 Open Space Program Project
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
#include "testapp.h"

#include <osp/tasks/top_utils.h>

#include <osp/Resource/resources.h>
#include <osp/Resource/ImporterData.h>

namespace testapp
{

void close_sessions(TestAppTasks &rTestApp, osp::SessionGroup &rSessions)
{
    rSessions.m_edges.m_semaphoreEdges      .clear();
    rSessions.m_edges.m_targetDependEdges   .clear();
    rSessions.m_edges.m_targetFulfillEdges  .clear();

    if ( rSessions.m_sessions.empty() || ! rTestApp.m_graph.has_value() )
    {
        return;
    }

    TestApp testapp;
    testapp.m_topData.clear();

    osp::top_close_session(rTestApp.m_tasks, rTestApp.m_graph.value(), rTestApp.m_taskData, rTestApp.m_topData, rTestApp.m_exec, rSessions.m_sessions);

    rSessions.m_sessions.clear();
}


void close_session(TestAppTasks &rTestApp, osp::Session &rSession)
{
    osp::top_close_session(rTestApp.m_tasks, rTestApp.m_graph.value(), rTestApp.m_taskData, rTestApp.m_topData, rTestApp.m_exec, osp::ArrayView<osp::Session>(&rSession, 1));
}


template<typename FUNC_T>
static void resource_for_each_type(osp::ResTypeId const type, osp::Resources& rResources, FUNC_T&& do_thing)
{
    lgrn::IdRegistry<osp::ResId> const &rReg = rResources.ids(type);
    for (std::size_t i = 0; i < rReg.capacity(); ++i)
    {
        if (rReg.exists(osp::ResId(i)))
        {
            do_thing(osp::ResId(i));
        }
    }
}

void clear_resource_owners(TestApp& rTestApp)
{
    using namespace osp::restypes;

    auto &rResources = osp::top_get<osp::Resources>(rTestApp.m_topData, rTestApp.m_idResources);

    // Texture resources contain osp::TextureImgSource, which refererence counts
    // their associated image data
    resource_for_each_type(gc_texture, rResources, [&rResources] (osp::ResId const id)
    {
        auto * const pData = rResources.data_try_get<osp::TextureImgSource>(gc_texture, id);
        if (pData != nullptr)
        {
            rResources.owner_destroy(gc_image, std::move(*pData));
        }
    });

    // Importer data own a lot of other resources
    resource_for_each_type(gc_importer, rResources, [&rResources] (osp::ResId const id)
    {
        auto * const pData = rResources.data_try_get<osp::ImporterData>(gc_importer, id);
        if (pData != nullptr)
        {
            for (osp::ResIdOwner_t &rOwner : std::move(pData->m_images))
            {
                rResources.owner_destroy(gc_image, std::move(rOwner));
            }

            for (osp::ResIdOwner_t &rOwner : std::move(pData->m_textures))
            {
                rResources.owner_destroy(gc_texture, std::move(rOwner));
            }

            for (osp::ResIdOwner_t &rOwner : std::move(pData->m_meshes))
            {
                rResources.owner_destroy(gc_mesh, std::move(rOwner));
            }
        }
    });
}

} // namespace testapp
