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

namespace testapp::scenes
{

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

    Session out;

    rBuilder.task()
        .name       ("Allocate Machine update bitset for MagicRocket")
        .run_on     ({tgScn.update(Run)})
        .sync_with  ({tgParts.machIds(Ready), tgParts.machUpdExtIn(New)})
        .push_to    (out.m_tasks)
        .args       ({idScnParts, idUpdMach})
        .func       ( [] (ACtxParts& rScnParts, MachineUpdater& rUpdMach) noexcept
    {
        rUpdMach.localDirty[gc_mtMagicRocket].ints().resize(rScnParts.machines.perType[gc_mtMagicRocket].localIds.vec().capacity());
    });

    return out;
} // setup_mach_rocket




struct ThrustIndicator
{
    MaterialId      material;
    Magnum::Color4  color;
    MeshIdOwner_t   mesh;

    KeyedVec<MachLocalId, DrawEnt> rktToDrawEnt;

    float           indicatorScale {0.0001f};
};

Session setup_thrust_indicators(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              application,
        Session const&              windowApp,
        Session const&              commonScene,
        Session const&              parts,
        Session const&              signalsFloat,
        Session const&              sceneRenderer,
        PkgId const                 pkg,
        MaterialId const            material)
{
    OSP_DECLARE_GET_DATA_IDS(application,    TESTAPP_DATA_APPLICATION);
    OSP_DECLARE_GET_DATA_IDS(commonScene,    TESTAPP_DATA_COMMON_SCENE);
    OSP_DECLARE_GET_DATA_IDS(parts,          TESTAPP_DATA_PARTS);
    OSP_DECLARE_GET_DATA_IDS(signalsFloat,   TESTAPP_DATA_SIGNALS_FLOAT)
    OSP_DECLARE_GET_DATA_IDS(sceneRenderer,  TESTAPP_DATA_SCENE_RENDERER);
    auto const tgWin    = windowApp     .get_pipelines<PlWindowApp>();
    auto const tgScnRdr = sceneRenderer .get_pipelines<PlSceneRenderer>();
    auto const tgParts  = parts         .get_pipelines<PlParts>();

    auto &rResources        = top_get< Resources >      (topData, idResources);
    auto &rBasic            = top_get< ACtxBasic >      (topData, idBasic);
    auto &rDrawing          = top_get< ACtxDrawing >    (topData, idDrawing);
    auto &rDrawingRes       = top_get< ACtxDrawingRes > (topData, idDrawingRes);
    auto &rScnRender        = top_get< ACtxSceneRender >(topData, idScnRender);
    auto &rDrawTfObservers  = top_get< DrawTfObservers >(topData, idDrawTfObservers);
    auto &rScnParts         = top_get< ACtxParts >      (topData, idScnParts);
    auto &rSigValFloat      = top_get< SignalValues_t<float> > (topData, idSigValFloat);

    Session out;
    auto const [idThrustIndicator] = out.acquire_data<1>(topData);
    auto &rThrustIndicator = top_emplace<ThrustIndicator>(topData, idThrustIndicator);

    rThrustIndicator.material   = material;
    rThrustIndicator.color      = { 1.0f, 0.2f, 0.8f, 1.0f };
    rThrustIndicator.mesh       = SysRender::add_drawable_mesh(rDrawing, rDrawingRes, rResources, pkg, "cone");

    rBuilder.task()
        .name       ("Create DrawEnts for Thrust indicators")
        .run_on     ({tgWin.sync(Run)})
        .sync_with  ({tgScnRdr.drawEntResized(ModifyOrSignal), tgScnRdr.drawEnt(New), tgParts.machIds(Ready)})
        .push_to    (out.m_tasks)
        .args       ({               idScnRender,                  idScnParts,                 idThrustIndicator})
        .func([]    (ACtxSceneRender &rScnRender,  ACtxParts const& rScnParts, ThrustIndicator& rThrustIndicator) noexcept
    {
        PerMachType const& rockets = rScnParts.machines.perType[gc_mtMagicRocket];

        rThrustIndicator.rktToDrawEnt.resize(rockets.localIds.capacity());

        for (MachLocalId const localId : rockets.localIds.bitview().zeros())
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
        .args       ({         idBasic,                 idScnRender,             idDrawing,                      idDrawingRes,                 idScnParts,                             idSigValFloat,                 idThrustIndicator})
        .func([]    (ACtxBasic& rBasic, ACtxSceneRender &rScnRender, ACtxDrawing& rDrawing, ACtxDrawingRes const& rDrawingRes, ACtxParts const& rScnParts, SignalValues_t<float> const& rSigValFloat, ThrustIndicator& rThrustIndicator) noexcept
    {
        Material            &rMat           = rScnRender.m_materials[rThrustIndicator.material];
        PerMachType const   &rockets        = rScnParts.machines.perType[gc_mtMagicRocket];
        Nodes const         &floats         = rScnParts.nodePerType[gc_ntSigFloat];

        for (MachLocalId const localId : rockets.localIds.bitview().zeros())
        {
            DrawEnt const   drawEnt         = rThrustIndicator.rktToDrawEnt[localId];

            MachAnyId const anyId           = rockets.localToAny[localId];
            PartId const    part            = rScnParts.machineToPart[anyId];
            ActiveEnt const partEnt         = rScnParts.partToActive[part];

            auto const&     portSpan        = floats.machToNode[anyId];
            NodeId const    throttleIn      = connected_node(portSpan, ports_magicrocket::gc_throttleIn.port);
            NodeId const    multiplierIn    = connected_node(portSpan, ports_magicrocket::gc_multiplierIn.port);

            float const     throttle        = std::clamp(rSigValFloat[throttleIn], 0.0f, 1.0f);
            float const     multiplier      = rSigValFloat[multiplierIn];
            float const     thrustMag       = throttle * multiplier;

            if (thrustMag == 0.0f)
            {
                rScnRender.m_visible.reset(drawEnt.value);
                continue;
            }

            if (!rMat.m_ents.test(drawEnt.value))
            {
                rMat.m_ents.set(drawEnt.value);
                rMat.m_dirty.push_back(drawEnt);
            }

            MeshIdOwner_t &rMeshOwner = rScnRender.m_mesh[drawEnt];
            if ( ! rMeshOwner.has_value() )
            {
                rScnRender.m_mesh[drawEnt] = rDrawing.m_meshRefCounts.ref_add(rThrustIndicator.mesh.value());
                rScnRender.m_meshDirty.push_back(drawEnt);
            }

            rScnRender.m_visible.set(drawEnt.value);
            rScnRender.m_opaque .set(drawEnt.value);

            rScnRender.m_color              [drawEnt] = rThrustIndicator.color;
            rScnRender.drawTfObserverEnable [partEnt] = 1;

            SysRender::needs_draw_transforms(rBasic.m_scnGraph, rScnRender.m_needDrawTf, partEnt);
        }
    });

    using UserData_t = DrawTfObservers::UserData_t;

    DrawTfObservers::Observer &rObserver = rDrawTfObservers.observers[0];

    rObserver.data = { &rThrustIndicator, &rScnParts, &rSigValFloat };
    rObserver.func = [] (ACtxSceneRender& rCtxScnRdr, Matrix4 const& drawTf, active::ActiveEnt ent, int depth, UserData_t data) noexcept
    {
        auto &rThrustIndicator          = *static_cast< ThrustIndicator* >          (data[0]);
        auto &rScnParts                 = *static_cast< ACtxParts* >                (data[1]);
        auto &rSigValFloat              = *static_cast< SignalValues_t<float>* >    (data[2]);

        PerMachType const   &rockets    = rScnParts.machines.perType[gc_mtMagicRocket];
        Nodes const         &floats     = rScnParts.nodePerType[gc_ntSigFloat];

        PartId const        part        = rScnParts.activeToPart[ent];
        ActiveEnt const     partEnt     = rScnParts.partToActive[part];

        for (MachinePair const pair : rScnParts.partToMachines[part])
        if (pair.type == gc_mtMagicRocket)
        {
            DrawEnt const   drawEnt         = rThrustIndicator.rktToDrawEnt[pair.local];
            MachAnyId const anyId           = rockets.localToAny[pair.local];

            auto const&     portSpan        = floats.machToNode[anyId];
            NodeId const    throttleIn      = connected_node(portSpan, ports_magicrocket::gc_throttleIn.port);
            NodeId const    multiplierIn    = connected_node(portSpan, ports_magicrocket::gc_multiplierIn.port);

            float const     throttle        = std::clamp(rSigValFloat[throttleIn], 0.0f, 1.0f);
            float const     multiplier      = rSigValFloat[multiplierIn];
            float const     thrustMag       = throttle * multiplier;

            rCtxScnRdr.m_drawTransform[drawEnt]
                    = drawTf
                    * Matrix4::scaling({1.0f, 1.0f, thrustMag * rThrustIndicator.indicatorScale})
                    * Matrix4::translation({0.0f, 0.0f, -1.0f})
                    * Matrix4::scaling({0.2f, 0.2f, 1.0f});
        }
    };

    rBuilder.task()
        .name       ("Clean up ThrustIndicator")
        .run_on     ({tgWin.cleanup(Run_)})
        .push_to    (out.m_tasks)
        .args       ({      idResources,             idDrawing,                 idThrustIndicator})
        .func([] (Resources& rResources, ACtxDrawing& rDrawing, ThrustIndicator& rThrustIndicator) noexcept
    {
        rDrawing.m_meshRefCounts.ref_release(std::move(rThrustIndicator.mesh));
    });

    return out;
} // setup_thrust_indicators




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

    Session out;

    rBuilder.task()
        .name       ("Allocate Machine update bitset for RcsDriver")
        .run_on     ({tgScn.update(Run)})
        .sync_with  ({tgParts.machIds(Ready), tgParts.machUpdExtIn(New)})
        .push_to    (out.m_tasks)
        .args       ({idScnParts, idUpdMach})
        .func       ( [] (ACtxParts& rScnParts, MachineUpdater& rUpdMach) noexcept
    {
        rUpdMach.localDirty[gc_mtRcsDriver].ints().resize(rScnParts.machines.perType[gc_mtRcsDriver].localIds.vec().capacity());
    });

    rBuilder.task()
        .name       ("RCS Drivers calculate new values")
        .run_on     ({tgParts.linkLoop(MachUpd)})
        .sync_with  ({tgParts.machUpdExtIn(Ready)})
        .push_to    (out.m_tasks)
        .args       ({      idScnParts,                idUpdMach,                       idSigValFloat,                    idSigUpdFloat})
        .func([] (ACtxParts& rScnParts, MachineUpdater& rUpdMach, SignalValues_t<float>& rSigValFloat, UpdateNodes<float>& rSigUpdFloat) noexcept
    {
        Nodes const &rFloatNodes = rScnParts.nodePerType[gc_ntSigFloat];
        PerMachType &rRockets    = rScnParts.machines.perType[gc_mtRcsDriver];

        for (MachLocalId const local : rUpdMach.localDirty[gc_mtRcsDriver].ones())
        {
            MachAnyId const mach     = rRockets.localToAny[local];
            auto const      portSpan = lgrn::Span<NodeId const>{rFloatNodes.machToNode[mach]};

            NodeId const thrNode = connected_node(portSpan, ports_rcsdriver::gc_throttleOut.port);
            if (thrNode == lgrn::id_null<NodeId>())
            {
                continue; // Throttle Output not connected, calculations below are useless
            }

            auto const rcs_read = [&rSigValFloat, portSpan] (float& rDstVar, PortEntry const& entry)
            {
                NodeId const node = connected_node(portSpan, entry.port);

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
} // setup_mach_rcsdriver




struct VehicleControls
{
    MachLocalId selectedUsrCtrl{lgrn::id_null<MachLocalId>()};

    input::EButtonControlIndex btnSwitch;
    input::EButtonControlIndex btnThrMax;
    input::EButtonControlIndex btnThrMin;
    input::EButtonControlIndex btnThrMore;
    input::EButtonControlIndex btnThrLess;
    input::EButtonControlIndex btnPitchUp;
    input::EButtonControlIndex btnPitchDn;
    input::EButtonControlIndex btnYawLf;
    input::EButtonControlIndex btnYawRt;
    input::EButtonControlIndex btnRollLf;
    input::EButtonControlIndex btnRollRt;
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
    OSP_DECLARE_GET_DATA_IDS(parts,         TESTAPP_DATA_PARTS);
    OSP_DECLARE_GET_DATA_IDS(windowApp,     TESTAPP_DATA_WINDOW_APP);
    OSP_DECLARE_GET_DATA_IDS(signalsFloat,  TESTAPP_DATA_SIGNALS_FLOAT);
    auto const tgWin    = windowApp     .get_pipelines<PlWindowApp>();
    auto const tgScn    = scene         .get_pipelines<PlScene>();
    auto const tgSgFlt  = signalsFloat  .get_pipelines<PlSignalsFloat>();

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_VEHICLE_CONTROL);
    auto const tgVhCtrl = out.create_pipelines<PlVehicleCtrl>(rBuilder);

    rBuilder.pipeline(tgVhCtrl.selectedVehicle).parent(tgScn.update);

    auto &rUserInput = top_get< input::UserInputHandler >(topData, idUserInput);

    // TODO: add cleanup task
    top_emplace<VehicleControls>(topData, idVhControls, VehicleControls{
        .btnSwitch  = rUserInput.button_subscribe("game_switch"),
        .btnThrMax  = rUserInput.button_subscribe("vehicle_thr_max"),
        .btnThrMin  = rUserInput.button_subscribe("vehicle_thr_min"),
        .btnThrMore = rUserInput.button_subscribe("vehicle_thr_more"),
        .btnThrLess = rUserInput.button_subscribe("vehicle_thr_less"),
        .btnPitchUp = rUserInput.button_subscribe("vehicle_pitch_up"),
        .btnPitchDn = rUserInput.button_subscribe("vehicle_pitch_dn"),
        .btnYawLf   = rUserInput.button_subscribe("vehicle_yaw_lf"),
        .btnYawRt   = rUserInput.button_subscribe("vehicle_yaw_rt"),
        .btnRollLf  = rUserInput.button_subscribe("vehicle_roll_lf"),
        .btnRollRt  = rUserInput.button_subscribe("vehicle_roll_rt")
    });

    rBuilder.task()
        .name       ("Select vehicle")
        .run_on     ({tgWin.inputs(Run)})
        .sync_with  ({tgVhCtrl.selectedVehicle(Modify)})
        .push_to    (out.m_tasks)
        .args       ({      idScnParts,                               idUserInput,                 idVhControls})
        .func([] (ACtxParts& rScnParts, input::UserInputHandler const &rUserInput, VehicleControls &rVhControls) noexcept
    {
        PerMachType &rUsrCtrl    = rScnParts.machines.perType[gc_mtUserCtrl];

        // Select a UsrCtrl machine when pressing the switch button
        if (rUserInput.button_state(rVhControls.btnSwitch).m_triggered)
        {
            ++rVhControls.selectedUsrCtrl;
            bool found = false;
            for (MachLocalId local = rVhControls.selectedUsrCtrl; local < rUsrCtrl.localIds.capacity(); ++local)
            {
                if (rUsrCtrl.localIds.exists(local))
                {
                    found = true;
                    rVhControls.selectedUsrCtrl = local;
                    break;
                }
            }

            if ( ! found )
            {
                rVhControls.selectedUsrCtrl = lgrn::id_null<MachLocalId>();
                OSP_LOG_INFO("Unselected vehicles");
            }
            else
            {
                OSP_LOG_INFO("Selected User Control: {}", rVhControls.selectedUsrCtrl);
            }
        }
    });

    rBuilder.task()
        .name       ("Write inputs to UserControl Machines")
        .run_on     ({tgScn.update(Run)})
        .sync_with  ({tgWin.inputs(Run), tgSgFlt.sigFloatUpdExtIn(Modify)})
        .push_to    (out.m_tasks)
        .args       ({      idScnParts,                idUpdMach,                       idSigValFloat,                    idSigUpdFloat,                               idUserInput,                 idVhControls,           idDeltaTimeIn})
        .func([] (ACtxParts& rScnParts, MachineUpdater& rUpdMach, SignalValues_t<float>& rSigValFloat, UpdateNodes<float>& rSigUpdFloat, input::UserInputHandler const& rUserInput, VehicleControls& rVhControls, float const deltaTimeIn) noexcept
    {
        VehicleControls& rVC = rVhControls;
        auto const held = [&rUserInput] (input::EButtonControlIndex idx, float val) -> float
        {
            return rUserInput.button_state(idx).m_held ? val : 0.0f;
        };

        if (rVC.selectedUsrCtrl == lgrn::id_null<MachLocalId>())
        {
            return; // No vehicle selected
        }

        Nodes const &rFloatNodes = rScnParts.nodePerType[gc_ntSigFloat];
        float const thrRate = deltaTimeIn;

        float const thrChange
            = held(rVC.btnThrMore, thrRate) - held(rVC.btnThrLess, thrRate)
            + held(rVC.btnThrMax, 1.0f)     - held(rVC.btnThrMin, 1.0f);

        Vector3 const attitude
        {
            held(rVC.btnPitchDn, 1.0f) - held(rVC.btnPitchUp, 1.0f),
            held(rVC.btnYawLf,   1.0f) - held(rVC.btnYawRt,   1.0f),
            held(rVC.btnRollRt,  1.0f) - held(rVC.btnRollLf,  1.0f)
        };

        PerMachType     &rUsrCtrl   = rScnParts.machines.perType[gc_mtUserCtrl];
        MachAnyId const mach        = rUsrCtrl.localToAny[rVC.selectedUsrCtrl];
        auto const      portSpan    = lgrn::Span<NodeId const>{rFloatNodes.machToNode[mach]};

        bool changed = false;
        auto const write_control = [&rSigValFloat, &rSigUpdFloat, &changed, portSpan] (PortEntry const& entry, float write, bool replace = true, float min = 0.0f, float max = 1.0f)
        {
            NodeId const node = connected_node(portSpan, entry.port);
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
    auto const tgCmCt   = cameraCtrl    .get_pipelines<PlCameraCtrl>();
    auto const tgPhys   = physics       .get_pipelines<PlPhysics>();
    auto const tgParts  = parts         .get_pipelines<PlParts>();

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
        .sync_with  ({tgCmCt.camCtrl(Modify), tgPhys.physUpdate(Done), tgParts.mapWeldActive(Ready)})
        .push_to    (out.m_tasks)
        .args       ({                 idCamCtrl,           idDeltaTimeIn,                 idBasic,                 idVhControls,                 idScnParts})
        .func([] (ACtxCameraController& rCamCtrl, float const deltaTimeIn, ACtxBasic const& rBasic, VehicleControls& rVhControls, ACtxParts const& rScnParts) noexcept
    {
        if (rVhControls.selectedUsrCtrl != lgrn::id_null<MachLocalId>())
        {
            // Follow selected UserControl machine

            // Obtain associated ActiveEnt
            // MachLocalId -> MachAnyId -> PartId -> RigidGroup -> ActiveEnt
            PerMachType const&  rUsrCtrls       = rScnParts.machines.perType.at(adera::gc_mtUserCtrl);
            MachAnyId const     selectedMach    = rUsrCtrls.localToAny        .at(rVhControls.selectedUsrCtrl);
            PartId const        selectedPart    = rScnParts.machineToPart     .at(selectedMach);
            WeldId const        weld            = rScnParts.partToWeld        .at(selectedPart);
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
