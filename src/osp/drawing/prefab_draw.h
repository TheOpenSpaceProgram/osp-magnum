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
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHEfR DEALINGS IN THE
 * SOFTWARE.
 */
#pragma once

#include "drawing.h"
#include "../activescene/prefab_fn.h"
#include "../core/array_view.h"

namespace osp::draw
{

class SysPrefabDraw
{
    using ACtxBasic         = osp::active::ACtxBasic;
    using ACtxPrefabs       = osp::active::ACtxPrefabs;
    using ActiveEnt         = osp::active::ActiveEnt;
    using TmpPrefabRequest  = osp::active::TmpPrefabRequest;
public:

    static void init_drawents(
            ACtxPrefabs&                rPrefabs,
            Resources&                  rResources,
            ACtxBasic const&            rBasic,
            ACtxDrawing&                rDrawing,
            ACtxSceneRender&            rScnRender);

    static void resync_drawents(
            ACtxPrefabs&                rPrefabs,
            Resources&                  rResources,
            ACtxBasic const&            rBasic,
            ACtxDrawing&                rDrawing,
            ACtxSceneRender&            rScnRender);

    static void init_mesh_texture_material(
            ACtxPrefabs&                rPrefabs,
            Resources&                  rResources,
            ACtxBasic const&            rBasic,
            ACtxDrawing&                rDrawing,
            ACtxDrawingRes&             rDrawingRes,
            ACtxSceneRender&            rScnRender,
            MaterialId                  material = lgrn::id_null<MaterialId>());

    static void resync_mesh_texture_material(
            ACtxPrefabs&                rPrefabs,
            Resources&                  rResources,
            ACtxBasic const&            rBasic,
            ACtxDrawing&                rDrawing,
            ACtxDrawingRes&             rDrawingRes,
            ACtxSceneRender&            rScnRender,
            MaterialId                  material = lgrn::id_null<MaterialId>());
};

} // namespace osp::active
