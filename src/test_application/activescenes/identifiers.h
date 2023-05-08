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

#include <osp/tasks/top_tasks.h>

namespace testapp
{

// Notes:
// * "Delete" tasks often run before "New" tasks, since deletion leaves empty spaces in arrays
//   that are best populated with new instances right away.
//   * "Delete" task fulfills _del and _mod
//   * "New" task depends on _del, fulfills _mod and _new
//

// Scene sessions

// Persistent:
//
// DeleteTask --->(_del)---> NewTask --->(_mod)---> UseTask --->(_use)
//      |                                   ^
//      +-----------------------------------+
//

// Transient: containers filled and cleared right away
//
// PushValuesTask --->(_mod)---> UseValuesTask --->(_use)---> ClearValuesTask --->(_clr)
//

#define TESTAPP_DATA_SCENE 1, \
    idDeltaTimeIn
struct TgtScene
{
    osp::TargetId cleanup;
    osp::TargetId time;
    osp::TargetId sync;
    osp::TargetId resyncAll;
};

#define TESTAPP_DATA_COMMON_SCENE 6, \
    idBasic, idDrawing, idDrawingRes, idActiveEntDel, idDrawEntDel, idNMesh
struct TgtCommonScene
{
    osp::TargetId      activeEnt_del,      activeEnt_new,      activeEnt_mod,      activeEnt_use;
    osp::TargetId   delActiveEnt_mod,   delActiveEnt_use,   delActiveEnt_clr;
    osp::TargetId      transform_del,      transform_new,      transform_mod,      transform_use;
    osp::TargetId           hier_del,           hier_new,           hier_mod,           hier_use;
    osp::TargetId        drawEnt_del,        drawEnt_new,        drawEnt_mod,        drawEnt_use;
    osp::TargetId     delDrawEnt_mod,     delDrawEnt_use,     delDrawEnt_clr;
    osp::TargetId           mesh_del,           mesh_new,           mesh_mod,           mesh_use;
    osp::TargetId        texture_del,        texture_new,        texture_mod,        texture_use;
    osp::TargetId       material_del,       material_new,       material_mod,       material_use,       material_clr;
};


#define TESTAPP_DATA_PHYSICS 3, \
    idPhys, idHierBody, idPhysIn
struct TgtPhysics
{

    osp::TargetId        physics_del,        physics_new,        physics_mod,        physics_use;
};



#define TESTAPP_DATA_SHAPE_SPAWN 1, \
    idSpawner
struct TgtShapeSpawn
{
    osp::TargetId   spawnRequest_mod,   spawnRequest_use,   spawnRequest_clr;
    osp::TargetId    spawnedEnts_mod,    spawnedEnts_use;  // spawnRequest_clr;
};



#define TESTAPP_DATA_PREFABS 1, \
    idPrefabInit
#define OS, tP_TAGS_TESTAPP_PREFABS 7, \
    tgPrefabMod,        tgPrefabReq,        tgPrefabClr,        \
    tgPrefabEntMod,     tgPrefabEntReq,                         \
    tgPfParentHierMod,  tgPfParentHierReq



#define TESTAPP_DATA_BOUNDS 2, \
    idBounds, idOutOfBounds
#define OSP_TAGS_TESTAPP_BOUNDS 5, \
    tgBoundsSetDel,     tgBoundsSetMod,     tgBoundsSetReq,     \
    tgOutOfBoundsPrv,   tgOutOfBoundsMod



#define TESTAPP_DATA_PARTS 6, \
    idScnParts, idPartInit, idUpdMach, idMachEvtTags, idMachUpdEnqueue, idtgNodeUpdEvt
#define OSP_TAGS_TESTAPP_PARTS 17, \
    tgPartMod,          tgPartReq,          tgPartClr,          \
    tgMapPartEntMod,    tgMapPartEntReq,                        \
    tgWeldMod,          tgWeldReq,          tgWeldClr,          \
    tgLinkMod,          tgLinkReq,                              \
    tgLinkMhUpdMod,     tgLinkMhUpdReq,                         \
    tgNodeAnyUpdMod,    tgNodeAnyUpdReq,                        \
    tgMachUpdEnqMod,    tgMachUpdEnqReq,    tgNodeUpdEvt



#define TESTAPP_DATA_VEHICLE_SPAWN 1, \
    idVehicleSpawn
#define OSP_TAGS_TESTAPP_VEHICLE_SPAWN 11, \
    tgVsBasicInMod,     tgVsBasicInReq,     tgVsBasicInClr,     \
    tgVsPartMod,        tgVsPartReq,                            \
    tgVsMapPartMachMod, tgVsMapPartMachReq,                     \
    tgVsWeldMod,        tgVsWeldReq,                            \
    tgVsPartPfMod,      tgVsPartPfReq



#define TESTAPP_DATA_VEHICLE_SPAWN_VB 1, \
    idVehicleSpawnVB
#define OSP_TAGS_TESTAPP_VEHICLE_SPAWN_VB 10, \
    tgVbSpBasicInMod,   tgVbSpBasicInReq,                       \
    tgVbPartMod,        tgVbPartReq,                            \
    tgVbWeldMod,        tgVbWeldReq,                            \
    tgVbMachMod,        tgVbMachReq,                            \
    tgVbNodeMod,        tgVbNodeReq


#define TESTAPP_DATA_TEST_VEHICLES 1, \
    idTVPartVehicle



#define TESTAPP_DATA_SIGNALS_FLOAT 2, \
    idSigValFloat,      idSigUpdFloat
#define OSP_TAGS_TESTAPP_SIGNALS_FLOAT 5, \
    tgSigFloatLinkMod,  tgSigFloatLinkReq,                      \
    tgSigFloatUpdMod,   tgSigFloatUpdReq,   tgSigFloatUpdEvt    \



#define TESTAPP_DATA_MACH_ROCKET 1, \
    idDummy
#define OSP_TAGS_TESTAPP_MACH_ROCKET 1, \
    tgMhRocketEvt

#define OSP_TAGS_TESTAPP_MACH_RCSDRIVER 1, \
    tgMhRcsDriverEvt

#define TESTAPP_DATA_NEWTON 1, \
    idNwt
struct TgtNewton
{
    osp::TargetId        nwtBody_del,        nwtBody_new,        nwtBody_mod,        nwtBody_use;
};

#define TESTAPP_DATA_NEWTON_FORCES 1, \
    idNwtFactors



#define TESTAPP_DATA_NEWTON_ACCEL 1, \
    idAcceleration



#define OSP_TAGS_TESTAPP_VEHICLE_SPAWN_NWT 4, \
    tgNwtVhWeldEntMod,  tgNwtVhWeldEntReq,                      \
    tgNwtVhHierMod,     tgNwtVhHierReq



#define TESTAPP_DATA_ROCKETS_NWT 1, \
    idRocketsNwt


//-----------------------------------------------------------------------------

// Universe sessions

#define TESTAPP_DATA_UNI_CORE 2, \
    idUniverse,         tgUniDeltaTimeIn
#define OSP_TAGS_TESTAPP_UNI_CORE 4, \
    tgUniUpdEvt,        tgUniTimeEvt,                           \
    tgUniTransferMod,   tgUniTransferReq

#define TESTAPP_DATA_UNI_SCENEFRAME 1, \
    idScnFrame
#define OSP_TAGS_TESTAPP_UNI_SCENEFRAME 2, \
    tgScnFramePosMod,   tgScnFramePosReq

#define TESTAPP_DATA_UNI_PLANETS 2, \
    idPlanetMainSpace, idSatSurfaceSpaces

//-----------------------------------------------------------------------------

// Renderer sessions, tend to exist only when the window is open

#define TESTAPP_DATA_WINDOW_APP 1, \
    idUserInput
struct TgtWindowApp
{
    osp::TargetId input;
    osp::TargetId render;
};



#define TESTAPP_DATA_MAGNUM 2, \
    idActiveApp, idRenderGl
struct TgtMagnum
{
    osp::TargetId cleanup;

    osp::TargetId meshGL_mod, meshGL_use;
    osp::TargetId textureGL_mod, textureGL_use;
};



#define TESTAPP_DATA_COMMON_RENDERER 3, \
    idScnRender, idGroupFwd, idCamera
struct TgtSceneRenderer
{
    osp::TargetId fboRender;
    osp::TargetId fboRenderDone;

    osp::TargetId      scnRender_del,      scnRender_new,      scnRender_mod,      scnRender_use;
    osp::TargetId          group_mod,          group_use;
    osp::TargetId      groupEnts_del,      groupEnts_new,      groupEnts_mod,      groupEnts_use;
    osp::TargetId  drawTransform_new,  drawTransform_mod,  drawTransform_use;
    osp::TargetId         camera_mod,         camera_use;
    osp::TargetId        entMesh_new,        entMesh_mod,        entMesh_use;
    osp::TargetId     entTexture_new,     entTexture_mod,     entTexture_use;
};



#define TESTAPP_DATA_CAMERA_CTRL 1, \
    idCamCtrl
struct TgtCameraCtrl
{
    osp::TargetId     cameraCtrl_mod,     cameraCtrl_use;
};


#define TESTAPP_DATA_SHADER_VISUALIZER 1, \
    idDrawShVisual



#define TESTAPP_DATA_SHADER_PHONG 1, \
    idDrawShPhong



#define TESTAPP_DATA_SHADER_FLAT 1, \
    idDrawShFlat



#define TESTAPP_DATA_INDICATOR 1, \
    idIndicator



#define TESTAPP_DATA_VEHICLE_CONTROL 1, \
    idVhControls
#define OSP_TAGS_TESTAPP_VEHICLE_CONTROL 2, \
    tgSelUsrCtrlMod,    tgSelUsrCtrlReq

} // namespace testapp
