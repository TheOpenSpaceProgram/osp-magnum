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

#include <osp/tasks/worker.h>

namespace testapp
{

struct CommonMaterials
{
    int     m_materialCount     {0};
    int     m_matCommon         {m_materialCount++};
    int     m_matVisualizer     {m_materialCount++};
};

struct CommonTestSceneTags
{
    // ID registry generates entity IDs, and keeps track of which ones exist
    osp::MainDataId m_activeIds;

    // Basic and Drawing components
    osp::MainDataId m_basic;
    osp::MainDataId m_drawing;
    osp::MainDataId m_drawingRes;
    osp::MainDataId m_materials;

    // Entity delete list/queue
    osp::MainDataId m_delete;
    osp::MainDataId m_deleteTotal;
};

struct CommonTestScene
{
    static CommonTestSceneTags setup_data(osp::MainDataSpan_t& rMainData, osp::MainDataIt_t& rItCurr);



    /**
     * @brief Delete descendents of entities to delete
     *
     * Reads m_delete, and populates m_deleteTotal with existing m_delete and
     * descendents added
     */
    void update_hierarchy_delete();

    /**
     * @brief Delete components of deleted entities in m_deleteTotal
     */
    void update_delete();

    /**
     * @brief Set all meshes, textures, and materials as dirty
     *
     * Intended to synchronize a newly added renderer
     */
    void set_all_dirty();
};


} // namespace testapp
