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
#include "misc.h"

#include "../feature_interfaces.h"

#include <adera/drawing/CameraController.h>
#include <osp/activescene/basic_fn.h>
#include <osp/drawing/drawing_fn.h>

using namespace adera;
using namespace ftr_inter::stages;
using namespace ftr_inter;
using namespace osp::active;
using namespace osp::draw;
using namespace osp::fw;
using namespace osp;

using osp::input::EButtonControlIndex;

namespace adera
{


FeatureDef const ftrCameraFree = feature_def("CameraFree", [] (
        FeatureBuilder              &rFB,
        DependOn<FIWindowApp>       windowApp,
        DependOn<FIScene>           scn,
        DependOn<FICameraControl>   camCtrl)
{
    rFB.task()
        .name       ("Move Camera controller")
        .run_on     ({windowApp.pl.inputs(Run)})
        .sync_with  ({camCtrl.pl.camCtrl(Modify)})
        .args       ({                 camCtrl.di.camCtrl,           scn.di.deltaTimeIn })
        .func([] (ACtxCameraController& rCamCtrl, float const deltaTimeIn) noexcept
    {
        SysCameraController::update_view(rCamCtrl, deltaTimeIn);
        SysCameraController::update_move(rCamCtrl, deltaTimeIn, true);
    });
}); // ftrCameraFree




FeatureDef const ftrCursor = feature_def("Cursor", [] (
        FeatureBuilder              &rFB,
        Implement<FICursor>         cursor,
        DependOn<FIMainApp>         mainApp,
        DependOn<FIScene>           scn,
        DependOn<FICommonScene>     comScn,
        DependOn<FICameraControl>   camCtrl,
        DependOn<FISceneRenderer>   scnRender,
        entt::any                   userData)
{
    auto const [pkg, material] = entt::any_cast<TplPkgIdMaterialId>(userData);

    auto &rResources    = rFB.data_get< Resources >          (mainApp.di.resources);
    auto &rScnRender    = rFB.data_get< ACtxSceneRender >    (scnRender.di.scnRender);
    auto &rDrawing      = rFB.data_get< ACtxDrawing >        (comScn.di.drawing);
    auto &rDrawingRes   = rFB.data_get< ACtxDrawingRes >     (comScn.di.drawingRes);

    auto const cursorEnt = rFB.data_emplace<DrawEnt>(cursor.di.drawEnt, rScnRender.m_drawIds.create());
    rScnRender.resize_draw();

    rScnRender.m_mesh[cursorEnt] = SysRender::add_drawable_mesh(rDrawing, rDrawingRes, rResources, pkg, "cubewire");
    rScnRender.m_color[cursorEnt] = { 0.0f, 1.0f, 0.0f, 1.0f };
    rScnRender.m_visible.insert(cursorEnt);
    rScnRender.m_opaque.insert(cursorEnt);

    Material &rMat = rScnRender.m_materials[material];
    rMat.m_ents.insert(cursorEnt);

    rFB.task()
        .name       ("Move cursor")
        .run_on     ({scnRender.pl.render(Run)})
        .sync_with  ({camCtrl.pl.camCtrl(Ready), scnRender.pl.drawTransforms(Modify_), scnRender.pl.drawEntResized(Done)})
        .args       ({        cursor.di.drawEnt,                            camCtrl.di.camCtrl,                 scnRender.di.scnRender })
        .func([] (DrawEnt const cursorEnt, ACtxCameraController const& rCamCtrl, ACtxSceneRender& rScnRender) noexcept
    {
        rScnRender.m_drawTransform[cursorEnt] = Matrix4::translation(rCamCtrl.m_target.value());
    });

}); // ftrCursor


} // namespace adera
