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
#include "misc.h"
#include "physics.h"

#include <adera/drawing/CameraController.h>
#include <osp/activescene/basic_fn.h>
#include <osp/core/Resources.h>
#include <osp/core/unpack.h>
#include <osp/drawing/drawing_fn.h>


using namespace adera;
using namespace osp;
using namespace osp::active;
using namespace osp::draw;

using osp::input::EButtonControlIndex;

// for the 0xrrggbb_rgbf and angle literalss
using namespace Magnum::Math::Literals;

namespace testapp::scenes
{

void create_materials(
        ArrayView<entt::any> const  topData,
        Session const&              sceneRenderer,
        int const                   count)
{
    OSP_DECLARE_GET_DATA_IDS(sceneRenderer, TESTAPP_DATA_SCENE_RENDERER);
    auto &rScnRender = top_get< ACtxSceneRender >(topData, idScnRender);

    for (int i = 0; i < count; ++i)
    {
        [[maybe_unused]] MaterialId const mat = rScnRender.m_materialIds.create();
    }

    rScnRender.m_materials.resize(count);
}




Session setup_camera_ctrl(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              windowApp,
        Session const&              sceneRenderer,
        Session const&              magnumScene)
{
    OSP_DECLARE_GET_DATA_IDS(windowApp,     TESTAPP_DATA_WINDOW_APP);
    OSP_DECLARE_GET_DATA_IDS(magnumScene,   TESTAPP_DATA_MAGNUM_SCENE);
    auto const tgScnRdr = sceneRenderer .get_pipelines<PlSceneRenderer>();
    auto const tgSR     = magnumScene   .get_pipelines<PlMagnumScene>();
    auto const tgWin    = windowApp     .get_pipelines<PlWindowApp>();

    auto &rUserInput = top_get< osp::input::UserInputHandler >(topData, idUserInput);

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_CAMERA_CTRL);
    auto const tgCmCt = out.create_pipelines<PlCameraCtrl>(rBuilder);

    top_emplace< ACtxCameraController > (topData, idCamCtrl, rUserInput);

    rBuilder.pipeline(tgCmCt.camCtrl).parent(tgWin.sync);

    rBuilder.task()
        .name       ("Position Rendering Camera according to Camera Controller")
        .run_on     ({tgScnRdr.render(Run)})
        .sync_with  ({tgCmCt.camCtrl(Ready), tgSR.camera(Modify)})
        .push_to    (out.m_tasks)
        .args       ({                           idCamCtrl,        idCamera })
        .func([] (ACtxCameraController const& rCamCtrl, Camera &rCamera) noexcept
    {
        rCamera.m_transform = rCamCtrl.m_transform;
    });

    return out;
} // setup_camera_ctrl




Session setup_camera_free(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              windowApp,
        Session const&              scene,
        Session const&              cameraCtrl)
{
    OSP_DECLARE_GET_DATA_IDS(scene,         TESTAPP_DATA_SCENE);
    OSP_DECLARE_GET_DATA_IDS(cameraCtrl,    TESTAPP_DATA_CAMERA_CTRL);

    auto const tgWin    = windowApp     .get_pipelines<PlWindowApp>();
    auto const tgCmCt   = cameraCtrl    .get_pipelines<PlCameraCtrl>();

    Session out;

    rBuilder.task()
        .name       ("Move Camera controller")
        .run_on     ({tgWin.inputs(Run)})
        .sync_with  ({tgCmCt.camCtrl(Modify)})
        .push_to    (out.m_tasks)
        .args       ({                 idCamCtrl,           idDeltaTimeIn })
        .func([] (ACtxCameraController& rCamCtrl, float const deltaTimeIn) noexcept
    {
        SysCameraController::update_view(rCamCtrl, deltaTimeIn);
        SysCameraController::update_move(rCamCtrl, deltaTimeIn, true);
    });

    return out;
} // setup_camera_free


Session setup_cursor(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              application,
        Session const&              sceneRenderer,
        Session const&              cameraCtrl,
        Session const&              commonScene,
        MaterialId const            material,
        PkgId const                 pkg)
{
    OSP_DECLARE_GET_DATA_IDS(application,   TESTAPP_DATA_APPLICATION);
    OSP_DECLARE_GET_DATA_IDS(sceneRenderer, TESTAPP_DATA_SCENE_RENDERER);
    OSP_DECLARE_GET_DATA_IDS(commonScene,   TESTAPP_DATA_COMMON_SCENE);
    OSP_DECLARE_GET_DATA_IDS(cameraCtrl,    TESTAPP_DATA_CAMERA_CTRL);
    auto const tgCmCt   = cameraCtrl    .get_pipelines<PlCameraCtrl>();
    auto const tgScnRdr = sceneRenderer .get_pipelines<PlSceneRenderer>();

    auto &rResources    = top_get< Resources >          (topData, idResources);
    auto &rScnRender    = top_get< ACtxSceneRender >    (topData, idScnRender);
    auto &rDrawing      = top_get< ACtxDrawing >        (topData, idDrawing);
    auto &rDrawingRes   = top_get< ACtxDrawingRes >     (topData, idDrawingRes);

    Session out;
    auto const [idCursorEnt] = out.acquire_data<1>(topData);

    auto const cursorEnt = top_emplace<DrawEnt>(topData, idCursorEnt, rScnRender.m_drawIds.create());
    rScnRender.resize_draw();

    rScnRender.m_mesh[cursorEnt] = SysRender::add_drawable_mesh(rDrawing, rDrawingRes, rResources, pkg, "cubewire");
    rScnRender.m_color[cursorEnt] = { 0.0f, 1.0f, 0.0f, 1.0f };
    rScnRender.m_visible.set(std::size_t(cursorEnt));
    rScnRender.m_opaque.set(std::size_t(cursorEnt));

    Material &rMat = rScnRender.m_materials[material];
    rMat.m_ents.set(std::size_t(cursorEnt));

    rBuilder.task()
        .name       ("Move cursor")
        .run_on     ({tgScnRdr.render(Run)})
        .sync_with  ({tgCmCt.camCtrl(Ready), tgScnRdr.drawTransforms(Modify_), tgScnRdr.drawEntResized(Done)})
        .push_to    (out.m_tasks)
        .args       ({        idCursorEnt,                            idCamCtrl,                 idScnRender })
        .func([] (DrawEnt const cursorEnt, ACtxCameraController const& rCamCtrl, ACtxSceneRender& rScnRender) noexcept
    {
        rScnRender.m_drawTransform[cursorEnt] = Matrix4::translation(rCamCtrl.m_target.value());
    });

    return out;
} // setup_cursor


} // namespace testapp::scenes
