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
#include "vehicles.h"

#include <adera/activescene/vehicles_vb_fn.h>
#include <adera/drawing/CameraController.h>
#include <adera/machines/links.h>

#include <osp/activescene/basic.h>
#include <osp/activescene/physics.h>
#include <osp/activescene/prefab_fn.h>
#include <osp/core/Resources.h>
#include <osp/drawing/drawing.h>
#include <osp/drawing/drawing_fn.h>
#include <osp/util/UserInputHandler.h>

using namespace adera;

using namespace osp::active;
using namespace osp::draw;
using namespace osp::link;
using namespace osp;

using namespace Magnum::Math::Literals;

namespace testapp::scenes
{


template <MachTypeId const& MachType_T>
TopTaskFunc_t gen_allocate_mach_bitsets()
{
    static TopTaskFunc_t const func = wrap_args([] (ACtxParts& rScnParts, MachineUpdater& rUpdMach) noexcept
    {
        rUpdMach.m_localDirty[MachType_T].ints().resize(rScnParts.m_machines.m_perType[MachType_T].m_localIds.vec().capacity());
    });

    return func;
}

Session setup_mach_rocket(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              scene,
        Session const&              parts,
        Session const&              signalsFloat)
{
    OSP_DECLARE_GET_DATA_IDS(signalsFloat,  TESTAPP_DATA_SIGNALS_FLOAT)
    OSP_DECLARE_GET_DATA_IDS(parts,         TESTAPP_DATA_PARTS);
    auto const tgScn    = scene         .get_pipelines<PlScene>();
    auto const tgParts  = parts         .get_pipelines<PlParts>();

    using namespace adera;

    Session out;

    rBuilder.task()
        .name       ("Allocate Machine update bitset for MagicRocket")
        .run_on     ({tgScn.update(Run)})
        .sync_with  ({tgParts.machIds(Ready), tgParts.machUpdExtIn(New)})
        .push_to    (out.m_tasks)
        .args       ({idScnParts, idUpdMach})
        .func_raw   (gen_allocate_mach_bitsets<gc_mtMagicRocket>());

    return out;
}

struct ThrustIndicator
{
    Magnum::Color4  color;
    MeshIdOwner_t   mesh;

    KeyedVec<MachLocalId, DrawEnt> rktToDrawEnt;
};

static constexpr float gc_indicatorScale = 0.0001f;

Session setup_thrust_indicators(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              application,
        Session const&              windowApp,
        Session const&              commonScene,
        Session const&              parts,
        Session const&              signalsFloat,
        Session const&              sceneRenderer,
        Session const&              cameraCtrl,
        Session const&              shFlat,
        TopDataId const             idResources,
        PkgId const                 pkg)
{
    OSP_DECLARE_GET_DATA_IDS(commonScene,    TESTAPP_DATA_COMMON_SCENE);
    OSP_DECLARE_GET_DATA_IDS(parts,          TESTAPP_DATA_PARTS);
    OSP_DECLARE_GET_DATA_IDS(signalsFloat,   TESTAPP_DATA_SIGNALS_FLOAT)
    OSP_DECLARE_GET_DATA_IDS(sceneRenderer,  TESTAPP_DATA_SCENE_RENDERER);
    auto const tgWin    = windowApp     .get_pipelines< PlWindowApp >();
    auto const tgParts  = parts         .get_pipelines<PlParts>();
    auto const tgScnRdr = sceneRenderer .get_pipelines< PlSceneRenderer >();

    auto &rResources    = top_get< Resources >      (topData, idResources);
    auto &rBasic        = top_get< ACtxBasic >      (topData, idBasic);
    auto &rDrawing      = top_get< ACtxDrawing >    (topData, idDrawing);
    auto &rDrawingRes   = top_get< ACtxDrawingRes > (topData, idDrawingRes);

    Session out;
    auto const [idThrustIndicator] = out.acquire_data<1>(topData);
    auto &rThrustIndicator = top_emplace<ThrustIndicator>(topData, idThrustIndicator);

    rThrustIndicator.color = { 1.0f, 0.0f, 0.0f, 1.0f };
    rThrustIndicator.mesh  = SysRender::add_drawable_mesh(rDrawing, rDrawingRes, rResources, pkg, "cone");

    using adera::gc_mtMagicRocket;
    using adera::ports_magicrocket::gc_throttleIn;
    using adera::ports_magicrocket::gc_multiplierIn;

    rBuilder.task()
        .name       ("Create DrawEnts for Thrust indicators")
        .run_on     ({tgWin.sync(Run)})
        .sync_with  ({tgScnRdr.drawEntResized(ModifyOrSignal), tgScnRdr.drawEnt(New)})
        .push_to    (out.m_tasks)
        .args       ({               idScnRender,                  idScnParts,                 idThrustIndicator})
        .func([]    (ACtxSceneRender &rScnRender,  ACtxParts const& rScnParts, ThrustIndicator& rThrustIndicator) noexcept
    {
        PerMachType const&  rockets = rScnParts.m_machines.m_perType[gc_mtMagicRocket];

        rThrustIndicator.rktToDrawEnt.resize(rockets.m_localIds.capacity());

        for (MachLocalId const localId : rockets.m_localIds.bitview().zeros())
        {
            DrawEnt& rDrawEnt = rThrustIndicator.rktToDrawEnt[localId];
            if (rDrawEnt == lgrn::id_null<DrawEnt>())
            {
                rDrawEnt = rScnRender.m_drawIds.create();
            }
        }
    });

    rBuilder.task()
        .name       ("Add mesh and materials to Thrust indicators")
        .run_on     ({tgWin.sync(Run)})
        .sync_with  ({tgScnRdr.drawEntResized(Done), tgScnRdr.drawEnt(Ready), tgScnRdr.entMesh(New), tgScnRdr.material(New), tgScnRdr.materialDirty(Modify_), tgScnRdr.entMeshDirty(Modify_)})
        .push_to    (out.m_tasks)
        .args       ({               idScnRender,             idDrawing,                      idDrawingRes,                 idScnParts,                             idSigValFloat,                  idThrustIndicator})
        .func([]    (ACtxSceneRender &rScnRender, ACtxDrawing& rDrawing, ACtxDrawingRes const& rDrawingRes, ACtxParts const& rScnParts, SignalValues_t<float> const& rSigValFloat,  ThrustIndicator& rThrustIndicator) noexcept
    {
        PerMachType const&  rockets = rScnParts.m_machines.m_perType[gc_mtMagicRocket];
        Nodes const&        floats  = rScnParts.m_nodePerType[gc_ntSigFloat];

        for (MachLocalId const localId : rockets.m_localIds.bitview().zeros())
        {
            DrawEnt const   drawEnt         = rThrustIndicator.rktToDrawEnt[localId];

            MachAnyId const anyId           = rockets.m_localToAny[localId];
            PartId const    part            = rScnParts.m_machineToPart[anyId];
            ActiveEnt const partEnt         = rScnParts.m_partToActive[part];

            auto const&     portSpan        = floats.m_machToNode[anyId];
            NodeId const    throttleIn      = connected_node(portSpan, gc_throttleIn.m_port);
            NodeId const    multiplierIn    = connected_node(portSpan, gc_multiplierIn.m_port);

            float const     throttle        = std::clamp(rSigValFloat[throttleIn], 0.0f, 1.0f);
            float const     multiplier      = rSigValFloat[multiplierIn];
            float const     thrustMag       = throttle * multiplier;

            if (thrustMag == 0.0f)
            {
                rScnRender.m_visible.reset(drawEnt.value);
                continue;
            }

            MeshIdOwner_t &rMeshOwner = rScnRender.m_mesh[drawEnt];
            if ( ! rMeshOwner.has_value() )
            {
                rScnRender.m_mesh[drawEnt] = rDrawing.m_meshRefCounts.ref_add(rThrustIndicator.mesh.value());
                rScnRender.m_meshDirty.push_back(drawEnt);
            }

            rScnRender.m_visible.set(drawEnt.value);
            rScnRender.m_opaque.set(drawEnt.value);

            //Matrix4 const& rocketDrawTf = rScnRender.m_drawTransform
        }
    });

#if 0 // RENDERENT
    OSP_SESSION_ACQUIRE_DATA(thrustIndicator, topData, TESTAPP_INDICATOR);
    thrustIndicator.m_tgCleanupEvt = tgCleanupMagnumEvt;

    auto &rIndicator   = top_emplace<IndicatorMesh>(topData, idIndicator);


    thrustIndicator.task() = rBuilder.task().assign({tgRenderEvt, tgGlUse, tgDrawTransformReq, tgBindFboReq, tgFwdRenderMod, tgCamCtrlReq}).data(
            "Render cursor",
            TopDataIds_t{          idRenderGl,              idCamera,                      idDrawingRes,                         idScnRender,                 idScnParts,                             idSigValFloat,              idDrawShFlat,                            idCamCtrl,               idIndicator},
            wrap_args([] (RenderGL& rRenderGl, Camera const& rCamera, ACtxDrawingRes const& rDrawingRes, ACtxSceneRenderGL const& rScnRender, ACtxParts const& rScnParts, SignalValues_t<float> const& rSigValFloat, ACtxDrawFlat& rDrawShFlat, ACtxCameraController const& rCamCtrl, IndicatorMesh& rIndicator) noexcept
    {

        ResId const     indicatorResId      = rDrawingRes.m_meshToRes.at(rIndicator.m_mesh.value());
        MeshGlId const  indicatorMeshGlId   = rRenderGl.m_resToMesh.at(indicatorResId);
        Mesh&           rIndicatorMeshGl    = rRenderGl.m_meshGl.get(indicatorMeshGlId);

        ViewProjMatrix viewProj{rCamera.m_transform.inverted(), rCamera.perspective()};

        //auto const matrix = viewProj.m_viewProj * Matrix4::translation(rCamCtrl.m_target.value());

        PerMachType const& rockets = rScnParts.m_machines.m_perType[gc_mtMagicRocket];
        Nodes const& floats = rScnParts.m_nodePerType[gc_ntSigFloat];

        for (MachLocalId const localId : rockets.m_localIds.bitview().zeros())
        {
            MachAnyId const anyId           = rockets.m_localToAny[localId];
            PartId const    part            = rScnParts.m_machineToPart[anyId];
            ActiveEnt const partEnt         = rScnParts.m_partToActive[part];

            auto const&     portSpan        = floats.m_machToNode[anyId];
            NodeId const    throttleIn      = connected_node(portSpan, gc_throttleIn.m_port);
            NodeId const    multiplierIn    = connected_node(portSpan, gc_multiplierIn.m_port);

            float const     throttle        = std::clamp(rSigValFloat[throttleIn], 0.0f, 1.0f);
            float const     multiplier      = rSigValFloat[multiplierIn];
            float const     thrustMag       = throttle * multiplier;

            if (thrustMag == 0.0f)
            {
                continue;
            }

            Matrix4 const& rocketDrawTf = rScnRender.m_drawTransform.get(partEnt);

            auto const& matrix = Matrix4::from(rocketDrawTf.rotationNormalized(), rocketDrawTf.translation())
                               * Matrix4::scaling({1.0f, 1.0f, thrustMag * indicatorScale})
                               * Matrix4::translation({0.0f, 0.0f, -1.0f})
                               * Matrix4::scaling({0.2f, 0.2f, 1.0f});

            rDrawShFlat.m_shaderUntextured
                .setColor(rIndicator.m_color)
                .setTransformationProjectionMatrix(viewProj.m_viewProj * matrix)
                .draw(rIndicatorMeshGl);
        }
    }));

    thrustIndicator.task() = rBuilder.task().assign({tgCleanupMagnumEvt}).data(
            "Clean up thrust indicator resource owners",
            TopDataIds_t{             idDrawing,               idIndicator},
            wrap_args([] (ACtxDrawing& rDrawing, IndicatorMesh& rIndicator) noexcept
    {
        rDrawing.m_meshRefCounts.ref_release(std::move(rIndicator.m_mesh));
    }));

    // Draw transforms in part entities are required for drawing indicators.
    // This solution of adding them here is a bit janky but it works.

    thrustIndicator.task() = rBuilder.task().assign({tgSyncEvt, tgPartReq}).data(
            "Add draw transforms to rocket entities",
            TopDataIds_t{                 idScnParts,                   idScnRender},
            wrap_args([] (ACtxParts const& rScnParts, ACtxSceneRenderGL& rScnRender) noexcept
    {
        for (PartId const part : rScnParts.m_partDirty)
        {
            // TODO: maybe first check if the part contains any rocket machines

            ActiveEnt const ent = rScnParts.m_partToActive[part];
            if ( ! rScnRender.m_drawTransform.contains(ent) )
            {
                rScnRender.m_drawTransform.emplace(ent);
            }
        }
    }));

    // Called when scene is reopened
    thrustIndicator.task() = rBuilder.task().assign({tgResyncEvt, tgPartReq}).data(
            "Add draw transforms to all rocket entities",
            TopDataIds_t{                 idScnParts,                   idScnRender},
            wrap_args([] (ACtxParts const& rScnParts, ACtxSceneRenderGL& rScnRender) noexcept
    {
        for (PartId const part : rScnParts.m_partIds.bitview().zeros())
        {
            ActiveEnt const ent = rScnParts.m_partToActive[part];
            if ( ! rScnRender.m_drawTransform.contains(ent) )
            {
                rScnRender.m_drawTransform.emplace(ent);
            }
        }
    }));
#endif
    return out;
}

Session setup_mach_rcsdriver(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              scene,
        Session const&              parts,
        Session const&              signalsFloat)
{
    OSP_DECLARE_GET_DATA_IDS(signalsFloat,  TESTAPP_DATA_SIGNALS_FLOAT)
    OSP_DECLARE_GET_DATA_IDS(parts,         TESTAPP_DATA_PARTS);
    auto const tgScn    = scene         .get_pipelines<PlScene>();
    auto const tgParts  = parts         .get_pipelines<PlParts>();

    using namespace adera;

    Session out;

    rBuilder.task()
        .name       ("Allocate Machine update bitset for RcsDriver")
        .run_on     ({tgScn.update(Run)})
        .sync_with  ({tgParts.machIds(Ready), tgParts.machUpdExtIn(New)})
        .push_to    (out.m_tasks)
        .args       ({idScnParts, idUpdMach})
        .func_raw   (gen_allocate_mach_bitsets<gc_mtRcsDriver>());

    rBuilder.task()
        .name       ("RCS Drivers calculate new values")
        .run_on     ({tgParts.linkLoop(MachUpd)})
        .sync_with  ({tgParts.machUpdExtIn(Ready)})
        .push_to    (out.m_tasks)
        .args       ({      idScnParts,                idUpdMach,                       idSigValFloat,                    idSigUpdFloat})
        .func([] (ACtxParts& rScnParts, MachineUpdater& rUpdMach, SignalValues_t<float>& rSigValFloat, UpdateNodes<float>& rSigUpdFloat) noexcept
    {
        Nodes const &rFloatNodes = rScnParts.m_nodePerType[gc_ntSigFloat];
        PerMachType &rRockets = rScnParts.m_machines.m_perType[gc_mtRcsDriver];

        for (MachLocalId const local : rUpdMach.m_localDirty[gc_mtRcsDriver].ones())
        {
            MachAnyId const mach     = rRockets.m_localToAny[local];
            auto const      portSpan = lgrn::Span<NodeId const>{rFloatNodes.m_machToNode[mach]};

            NodeId const thrNode = connected_node(portSpan, ports_rcsdriver::gc_throttleOut.m_port);
            if (thrNode == lgrn::id_null<NodeId>())
            {
                continue; // Throttle Output not connected, calculations below are useless
            }

            auto const rcs_read = [&rSigValFloat, portSpan] (float& rDstVar, PortEntry const& entry)
            {
                NodeId const node = connected_node(portSpan, entry.m_port);

                if (node != lgrn::id_null<NodeId>())
                {
                    rDstVar = rSigValFloat[node];
                }
            };

            Vector3 pos     {0.0f};
            Vector3 dir     {0.0f};
            Vector3 cmdLin  {0.0f};
            Vector3 cmdAng  {0.0f};

            rcs_read( pos.x(),    ports_rcsdriver::gc_posXIn    );
            rcs_read( pos.y(),    ports_rcsdriver::gc_posYIn    );
            rcs_read( pos.z(),    ports_rcsdriver::gc_posZIn    );
            rcs_read( dir.x(),    ports_rcsdriver::gc_dirXIn    );
            rcs_read( dir.y(),    ports_rcsdriver::gc_dirYIn    );
            rcs_read( dir.z(),    ports_rcsdriver::gc_dirZIn    );
            rcs_read( cmdLin.x(), ports_rcsdriver::gc_cmdLinXIn );
            rcs_read( cmdLin.y(), ports_rcsdriver::gc_cmdLinYIn );
            rcs_read( cmdLin.z(), ports_rcsdriver::gc_cmdLinZIn );
            rcs_read( cmdAng.x(), ports_rcsdriver::gc_cmdAngXIn );
            rcs_read( cmdAng.y(), ports_rcsdriver::gc_cmdAngYIn );
            rcs_read( cmdAng.z(), ports_rcsdriver::gc_cmdAngZIn );

            OSP_LOG_TRACE("RCS controller {} pitch = {}", local, cmdAng.x());
            OSP_LOG_TRACE("RCS controller {} yaw = {}", local, cmdAng.y());
            OSP_LOG_TRACE("RCS controller {} roll = {}", local, cmdAng.z());

            float const thrCurr = rSigValFloat[thrNode];
            float const thrNew = thruster_influence(pos, dir, cmdLin, cmdAng);

            if (thrCurr != thrNew)
            {
                rSigUpdFloat.assign(thrNode, thrNew);
                rUpdMach.requestMachineUpdateLoop = true;
            }
        }
    });

    return out;
}


struct VehicleTestControls
{
    MachLocalId m_selectedUsrCtrl{lgrn::id_null<MachLocalId>()};

    input::EButtonControlIndex m_btnSwitch;
    input::EButtonControlIndex m_btnThrMax;
    input::EButtonControlIndex m_btnThrMin;
    input::EButtonControlIndex m_btnThrMore;
    input::EButtonControlIndex m_btnThrLess;
    input::EButtonControlIndex m_btnPitchUp;
    input::EButtonControlIndex m_btnPitchDn;
    input::EButtonControlIndex m_btnYawLf;
    input::EButtonControlIndex m_btnYawRt;
    input::EButtonControlIndex m_btnRollLf;
    input::EButtonControlIndex m_btnRollRt;
};


Session setup_vehicle_control(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              windowApp,
        Session const&              scene,
        Session const&              parts,
        Session const&              signalsFloat)
{
    OSP_DECLARE_GET_DATA_IDS(scene,         TESTAPP_DATA_SCENE);
    //OSP_DECLARE_GET_DATA_IDS(signalsFloat,  TESTAPP_DATA_SIGNALS_FLOAT)
    OSP_DECLARE_GET_DATA_IDS(parts,         TESTAPP_DATA_PARTS);
    OSP_DECLARE_GET_DATA_IDS(windowApp,     TESTAPP_DATA_WINDOW_APP);
    OSP_DECLARE_GET_DATA_IDS(signalsFloat,  TESTAPP_DATA_SIGNALS_FLOAT);
    //OSP_DECLARE_GET_DATA_IDS(app,           TESTAPP_DATA_APP);
    auto const tgWin    = windowApp     .get_pipelines<PlWindowApp>();
    auto const tgScn    = scene         .get_pipelines<PlScene>();
    auto const tgSgFlt  = signalsFloat  .get_pipelines<PlSignalsFloat>();

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_VEHICLE_CONTROL);
    auto const tgVhCtrl = out.create_pipelines<PlVehicleCtrl>(rBuilder);

    rBuilder.pipeline(tgVhCtrl.selectedVehicle).parent(tgScn.update);

    auto &rUserInput = top_get< input::UserInputHandler >(topData, idUserInput);

    // TODO: add cleanup task
    top_emplace<VehicleTestControls>(topData, idVhControls, VehicleTestControls{
        .m_btnSwitch    = rUserInput.button_subscribe("game_switch"),
        .m_btnThrMax    = rUserInput.button_subscribe("vehicle_thr_max"),
        .m_btnThrMin    = rUserInput.button_subscribe("vehicle_thr_min"),
        .m_btnThrMore   = rUserInput.button_subscribe("vehicle_thr_more"),
        .m_btnThrLess   = rUserInput.button_subscribe("vehicle_thr_less"),
        .m_btnPitchUp   = rUserInput.button_subscribe("vehicle_pitch_up"),
        .m_btnPitchDn   = rUserInput.button_subscribe("vehicle_pitch_dn"),
        .m_btnYawLf     = rUserInput.button_subscribe("vehicle_yaw_lf"),
        .m_btnYawRt     = rUserInput.button_subscribe("vehicle_yaw_rt"),
        .m_btnRollLf    = rUserInput.button_subscribe("vehicle_roll_lf"),
        .m_btnRollRt    = rUserInput.button_subscribe("vehicle_roll_rt")
    });

    auto const idNull = lgrn::id_null<TopDataId>();

    rBuilder.task()
        .name       ("Select vehicle")
        .run_on     ({tgWin.inputs(Run)})
        .sync_with  ({tgVhCtrl.selectedVehicle(Modify)})
        .push_to    (out.m_tasks)
        .args       ({      idScnParts,                               idUserInput,                     idVhControls})
        .func([] (ACtxParts& rScnParts, input::UserInputHandler const &rUserInput, VehicleTestControls &rVhControls) noexcept
    {
        PerMachType &rUsrCtrl    = rScnParts.m_machines.m_perType[gc_mtUserCtrl];

        // Select a UsrCtrl machine when pressing the switch button
        if (rUserInput.button_state(rVhControls.m_btnSwitch).m_triggered)
        {
            ++rVhControls.m_selectedUsrCtrl;
            bool found = false;
            for (MachLocalId local = rVhControls.m_selectedUsrCtrl; local < rUsrCtrl.m_localIds.capacity(); ++local)
            {
                if (rUsrCtrl.m_localIds.exists(local))
                {
                    found = true;
                    rVhControls.m_selectedUsrCtrl = local;
                    break;
                }
            }

            if ( ! found )
            {
                rVhControls.m_selectedUsrCtrl = lgrn::id_null<MachLocalId>();
                OSP_LOG_INFO("Unselected vehicles");
            }
            else
            {
                OSP_LOG_INFO("Selected User Control: {}", rVhControls.m_selectedUsrCtrl);
            }
        }
    });


    rBuilder.task()
        .name       ("Write inputs to UserControl Machines")
        .run_on     ({tgScn.update(Run)})
        .sync_with  ({tgWin.inputs(Run), tgSgFlt.sigFloatUpdExtIn(Modify)})
        .push_to    (out.m_tasks)
        .args       ({      idScnParts,                idUpdMach,                       idSigValFloat,                    idSigUpdFloat,                               idUserInput,                     idVhControls,           idDeltaTimeIn})
        .func([] (ACtxParts& rScnParts, MachineUpdater& rUpdMach, SignalValues_t<float>& rSigValFloat, UpdateNodes<float>& rSigUpdFloat, input::UserInputHandler const &rUserInput, VehicleTestControls &rVhControls, float const deltaTimeIn) noexcept
    {
        if (rVhControls.m_selectedUsrCtrl == lgrn::id_null<MachLocalId>())
        {
            return; // No vehicle selected
        }

        Nodes const &rFloatNodes = rScnParts.m_nodePerType[gc_ntSigFloat];

        // Control selected UsrCtrl machine

        float const thrRate = deltaTimeIn;
        float const thrChange =
                  float(rUserInput.button_state(rVhControls.m_btnThrMore).m_held) * thrRate
                - float(rUserInput.button_state(rVhControls.m_btnThrLess).m_held) * thrRate
                + float(rUserInput.button_state(rVhControls.m_btnThrMax).m_held)
                - float(rUserInput.button_state(rVhControls.m_btnThrMin).m_held);

        Vector3 const attitude
        {
              float(rUserInput.button_state(rVhControls.m_btnPitchDn).m_held)
            - float(rUserInput.button_state(rVhControls.m_btnPitchUp).m_held),
              float(rUserInput.button_state(rVhControls.m_btnYawLf).m_held)
            - float(rUserInput.button_state(rVhControls.m_btnYawRt).m_held),
              float(rUserInput.button_state(rVhControls.m_btnRollRt).m_held)
            - float(rUserInput.button_state(rVhControls.m_btnRollLf).m_held)
        };

        PerMachType     &rUsrCtrl   = rScnParts.m_machines.m_perType[gc_mtUserCtrl];
        MachAnyId const mach        = rUsrCtrl.m_localToAny[rVhControls.m_selectedUsrCtrl];
        auto const      portSpan    = lgrn::Span<NodeId const>{rFloatNodes.m_machToNode[mach]};

        bool changed = false;
        auto const write_control = [&rSigValFloat, &rSigUpdFloat, &changed, portSpan] (PortEntry const& entry, float write, bool replace = true, float min = 0.0f, float max = 1.0f)
        {
            NodeId const node = connected_node(portSpan, entry.m_port);
            if (node == lgrn::id_null<NodeId>())
            {
                return; // not connected
            }

            float const oldVal = rSigValFloat[node];
            float const newVal = replace ? write : Magnum::Math::clamp(oldVal + write, min, max);

            if (oldVal != newVal)
            {
                rSigUpdFloat.assign(node, newVal);
                changed = true;
            }
        };

        write_control(ports_userctrl::gc_throttleOut,   thrChange, false);
        write_control(ports_userctrl::gc_pitchOut,      attitude.x());
        write_control(ports_userctrl::gc_yawOut,        attitude.y());
        write_control(ports_userctrl::gc_rollOut,       attitude.z());

        if (changed)
        {
            rUpdMach.requestMachineUpdateLoop = true;
        }
    });

    return out;
} // setup_vehicle_control



Session setup_camera_vehicle(
        TopTaskBuilder&             rBuilder,
        [[maybe_unused]] ArrayView<entt::any> const topData,
        Session const&              windowApp,
        Session const&              scene,
        Session const&              sceneRenderer,
        Session const&              commonScene,
        Session const&              physics,
        Session const&              parts,
        Session const&              cameraCtrl,
        Session const&              vehicleCtrl)
{
    OSP_DECLARE_GET_DATA_IDS(scene,         TESTAPP_DATA_SCENE);
    OSP_DECLARE_GET_DATA_IDS(commonScene,   TESTAPP_DATA_COMMON_SCENE);
    OSP_DECLARE_GET_DATA_IDS(parts,         TESTAPP_DATA_PARTS);
    OSP_DECLARE_GET_DATA_IDS(cameraCtrl,    TESTAPP_DATA_CAMERA_CTRL);
    OSP_DECLARE_GET_DATA_IDS(vehicleCtrl,   TESTAPP_DATA_VEHICLE_CONTROL);

    auto const tgWin    = windowApp     .get_pipelines<PlWindowApp>();
    auto const tgScnRdr = sceneRenderer .get_pipelines<PlSceneRenderer>();
    auto const tgCmCt   = cameraCtrl    .get_pipelines<PlCameraCtrl>();
    auto const tgVhCtrl = vehicleCtrl   .get_pipelines<PlVehicleCtrl>();
    auto const tgCS     = commonScene   .get_pipelines<PlCommonScene>();
    auto const tgPhys   = physics       .get_pipelines<PlPhysics>();

    Session out;

    // Don't add tgCS.transform(Modify) to sync_with, even though this uses transforms.
    // tgPhys.physUpdate(Done) assures physics transforms are done.
    //
    // tgCmCt.camCtrl(Ready) is needed by the shape thrower, which needs tgCS.transform(New),
    // causing a circular dependency. The transform pipeline probably needs to be split into
    // a few separate ones.
    rBuilder.task()
        .name       ("Update vehicle camera")
        .run_on     ({tgWin.sync(Run)})
        .sync_with  ({tgCmCt.camCtrl(Modify), tgPhys.physUpdate(Done)})
        .push_to    (out.m_tasks)
        .args       ({                 idCamCtrl,           idDeltaTimeIn,                 idBasic,                     idVhControls,                 idScnParts})
        .func([] (ACtxCameraController& rCamCtrl, float const deltaTimeIn, ACtxBasic const& rBasic, VehicleTestControls& rVhControls, ACtxParts const& rScnParts) noexcept
    {
        if (MachLocalId const selectedLocal = rVhControls.m_selectedUsrCtrl;
            selectedLocal != lgrn::id_null<MachLocalId>())
        {
            // Follow selected UserControl machine

            // Obtain associated ActiveEnt
            // MachLocalId -> MachAnyId -> PartId -> RigidGroup -> ActiveEnt
            PerMachType const&  rUsrCtrls       = rScnParts.m_machines.m_perType.at(adera::gc_mtUserCtrl);
            MachAnyId const     selectedMach    = rUsrCtrls.m_localToAny        .at(selectedLocal);
            PartId const        selectedPart    = rScnParts.m_machineToPart     .at(selectedMach);
            WeldId const        weld            = rScnParts.m_partToWeld        .at(selectedPart);
            ActiveEnt const     selectedEnt     = rScnParts.weldToActive        .at(weld);

            if (rBasic.m_transform.contains(selectedEnt))
            {
                rCamCtrl.m_target = rBasic.m_transform.get(selectedEnt).m_transform.translation();
            }
        }
        else
        {
            // Free cam when no vehicle selected
            SysCameraController::update_move(
                    rCamCtrl,
                    deltaTimeIn, true);
        }

        SysCameraController::update_view(rCamCtrl, deltaTimeIn);
    });

    return out;
} // setup_camera_vehicle



} // namespace testapp::scenes
