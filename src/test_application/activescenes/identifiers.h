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

// Identifiers made for OSP_ACQUIRE_* and OSP_UNPACK_* macros
// Used to declare variable names for TopDataIds and TagIds
// #define OSP_[DATA/TAGS]_NAME <# of identifiers>, a, b, c, d, ...

// Tag naming:
//
// tg...Evt: Event, tasks with these tags are enqueued externally
// tg...New: Adds new instances
// tg...Del: Deletes instances
// tg...Mod: Modifies some data
// tg...Req: Requires some data after its modified
// tg...Prv: Requires previous from last update
// tg...Clr: Clears a queue after its used
//
// * Tasks with a tg...Req tag will run AFTER its corresponding tg...Mod tag
// * New often depends on Delete, as deleting instances first leaves empty
//   spaces in a container, best immediately filled with new instances
//

// Scene sessions

#define OSP_DATA_TESTAPP_COMMON_SCENE 8, \
    idDeltaTimeIn, idActiveIds, idBasic, idDrawing, idDrawingRes, idDelEnts, idDelTotal, idNMesh
#define OSP_TAGS_TESTAPP_COMMON_SCENE 34, \
    tgCleanupEvt,       tgResyncEvt,        tgSyncEvt,          tgSceneEvt,         tgTimeEvt,      \
    tgEntDel,           tgEntNew,           tgEntReq,                                               \
    tgDelEntMod,        tgDelEntReq,        tgDelEntClr,                                            \
    tgDelTotalMod,      tgDelTotalReq,      tgDelTotalClr,                                          \
    tgTransformMod,     tgTransformDel,     tgTransformNew,     tgTransformReq,                     \
    tgHierMod,          tgHierModEnd,       tgHierDel,          tgHierNew,          tgHierReq,      \
    tgDrawDel,          tgDrawMod,          tgDrawReq,                                              \
    tgMeshDel,          tgMeshMod,          tgMeshReq,          tgMeshClr,                          \
    tgTexDel,           tgTexMod,           tgTexReq,           tgTexClr



#define OSP_DATA_TESTAPP_MATERIAL 2, \
    idMatEnts, idMatDirty
#define OSP_TAGS_TESTAPP_MATERIAL 4, \
    tgMatDel, tgMatMod, tgMatReq, tgMatClr



#define OSP_DATA_TESTAPP_MATERIAL 2, \
    idMatEnts, idMatDirty
#define OSP_TAGS_TESTAPP_MATERIAL 4, \
    tgMatDel, tgMatMod, tgMatReq, tgMatClr



#define OSP_DATA_TESTAPP_PHYSICS 3, \
    idPhys, idHierBody, idPhysIn
#define OSP_TAGS_TESTAPP_PHYSICS 6, \
    tgPhysPrv,          tgPhysDel,          tgPhysMod,          tgPhysReq,      \
    tgPhysTransformMod, tgPhysTransformReq



#define OSP_DATA_TESTAPP_SHAPE_SPAWN 2, \
    idSpawner, idSpawnerEnts
#define OSP_TAGS_TESTAPP_SHAPE_SPAWN 5, \
    tgSpawnMod,         tgSpawnReq,         tgSpawnClr,         \
    tgSpawnEntMod,      tgSpawnEntReq



#define OSP_DATA_TESTAPP_PREFABS 1, \
    idPrefabInit
#define OSP_TAGS_TESTAPP_PREFABS 7, \
    tgPrefabMod,        tgPrefabReq,        tgPrefabClr,        \
    tgPrefabEntMod,     tgPrefabEntReq,                         \
    tgPfParentHierMod,  tgPfParentHierReq



#define OSP_DATA_TESTAPP_BOUNDS 2, \
    idBounds, idOutOfBounds
#define OSP_TAGS_TESTAPP_BOUNDS 5, \
    tgBoundsSetDel,     tgBoundsSetMod,     tgBoundsSetReq,     \
    tgOutOfBoundsPrv,   tgOutOfBoundsMod



#define OSP_DATA_TESTAPP_PARTS 6, \
    idScnParts, idPartInit, idUpdMach, idMachEvtTags, idMachUpdEnqueue, idtgNodeUpdEvt
#define OSP_TAGS_TESTAPP_PARTS 17, \
    tgPartMod,          tgPartReq,          tgPartClr,          \
    tgMapPartEntMod,    tgMapPartEntReq,                        \
    tgWeldMod,          tgWeldReq,          tgWeldClr,          \
    tgLinkMod,          tgLinkReq,                              \
    tgLinkMhUpdMod,     tgLinkMhUpdReq,                         \
    tgNodeAnyUpdMod,    tgNodeAnyUpdReq,                        \
    tgMachUpdEnqMod,    tgMachUpdEnqReq,    tgNodeUpdEvt



#define OSP_DATA_TESTAPP_VEHICLE_SPAWN 1, \
    idVehicleSpawn
#define OSP_TAGS_TESTAPP_VEHICLE_SPAWN 11, \
    tgVsBasicInMod,     tgVsBasicInReq,     tgVsBasicInClr,     \
    tgVsPartMod,        tgVsPartReq,                            \
    tgVsMapPartMachMod, tgVsMapPartMachReq,                     \
    tgVsWeldMod,        tgVsWeldReq,                            \
    tgVsPartPfMod,      tgVsPartPfReq



#define OSP_DATA_TESTAPP_VEHICLE_SPAWN_VB 1, \
    idVehicleSpawnVB
#define OSP_TAGS_TESTAPP_VEHICLE_SPAWN_VB 10, \
    tgVbSpBasicInMod,   tgVbSpBasicInReq,                       \
    tgVbPartMod,        tgVbPartReq,                            \
    tgVbWeldMod,        tgVbWeldReq,                            \
    tgVbMachMod,        tgVbMachReq,                            \
    tgVbNodeMod,        tgVbNodeReq


#define OSP_DATA_TESTAPP_TEST_VEHICLES 1, \
    idTVPartVehicle



#define OSP_DATA_TESTAPP_SIGNALS_FLOAT 2, \
    idSigValFloat,      idSigUpdFloat
#define OSP_TAGS_TESTAPP_SIGNALS_FLOAT 5, \
    tgSigFloatLinkMod,  tgSigFloatLinkReq,                      \
    tgSigFloatUpdMod,   tgSigFloatUpdReq,   tgSigFloatUpdEvt    \



#define OSP_DATA_TESTAPP_MACH_ROCKET 1, \
    idDummy
#define OSP_TAGS_TESTAPP_MACH_ROCKET 1, \
    tgMhRocketEvt

#define OSP_TAGS_TESTAPP_MACH_RCSDRIVER 1, \
    tgMhRcsDriverEvt

#define OSP_DATA_TESTAPP_NEWTON 1, \
    idNwt
#define OSP_TAGS_TESTAPP_NEWTON 5, \
    tgNwtBodyPrv,       tgNwtBodyDel,       tgNwtBodyMod,       tgNwtBodyReq,       tgNwtBodyClr


#define OSP_DATA_TESTAPP_NEWTON_FORCES 1, \
    idNwtFactors



#define OSP_DATA_TESTAPP_NEWTON_ACCEL 1, \
    idAcceleration



#define OSP_TAGS_TESTAPP_VEHICLE_SPAWN_NWT 4, \
    tgNwtVhWeldEntMod,  tgNwtVhWeldEntReq,                      \
    tgNwtVhHierMod,     tgNwtVhHierReq



#define OSP_DATA_TESTAPP_ROCKETS_NWT 1, \
    idRocketsNwt


//-----------------------------------------------------------------------------

// Universe sessions

#define OSP_DATA_TESTAPP_UNI_CORE 2, \
    idUniverse,         tgUniDeltaTimeIn
#define OSP_TAGS_TESTAPP_UNI_CORE 4, \
    tgUniUpdEvt,        tgUniTimeEvt,                           \
    tgUniTransferMod,   tgUniTransferReq

#define OSP_DATA_TESTAPP_UNI_SCENEFRAME 1, \
    idScnFrame
#define OSP_TAGS_TESTAPP_UNI_SCENEFRAME 2, \
    tgScnFramePosMod,   tgScnFramePosReq

#define OSP_DATA_TESTAPP_UNI_PLANETS 2, \
    idPlanetMainSpace, idSatSurfaceSpaces

//-----------------------------------------------------------------------------

// Renderer sessions, tend to exist only when the window is open

#define OSP_DATA_TESTAPP_APP 1, \
    idUserInput
#define OSP_TAGS_TESTAPP_APP 2, \
    tgRenderEvt, tgInputEvt



#define OSP_DATA_TESTAPP_APP_MAGNUM 3, \
    idUserInput, idActiveApp, idRenderGl
#define OSP_TAGS_TESTAPP_APP_MAGNUM 4, \
    tgRenderEvt, tgInputEvt, tgGlUse, tgCleanupMagnumEvt



#define OSP_DATA_TESTAPP_COMMON_RENDERER 3, \
    idScnRender, idGroupFwd, idCamera
#define OSP_TAGS_TESTAPP_COMMON_RENDERER 24, \
    tgDrawGlDel,        tgDrawGlMod,        tgDrawGlReq,        \
    tgMeshGlMod,        tgMeshGlReq,                            \
    tgTexGlMod,         tgTexGlReq,                             \
    tgEntTexMod,        tgEntTexReq,                            \
    tgEntMeshMod,       tgEntMeshReq,                           \
    tgCameraMod,        tgCameraReq,                            \
    tgGroupFwdDel,      tgGroupFwdMod,      tgGroupFwdReq,      \
    tgBindFboMod,       tgBindFboReq,                           \
    tgFwdRenderMod,     tgFwdRenderReq,                         \
    tgDrawTransformDel, tgDrawTransformNew, tgDrawTransformMod, tgDrawTransformReq



#define OSP_DATA_TESTAPP_CAMERA_CTRL 1, \
    idCamCtrl
#define OSP_TAGS_TESTAPP_CAMERA_CTRL 2, \
    tgCamCtrlMod,       tgCamCtrlReq



#define OSP_DATA_TESTAPP_SHADER_VISUALIZER 1, \
    idDrawShVisual



#define OSP_DATA_TESTAPP_SHADER_PHONG 1, \
    idDrawShPhong



#define OSP_DATA_TESTAPP_SHADER_FLAT 1, \
    idDrawShFlat



#define OSP_DATA_TESTAPP_VEHICLE_CONTROL 1, \
    idVhControls
#define OSP_TAGS_TESTAPP_VEHICLE_CONTROL 2, \
    tgSelUsrCtrlMod,    tgSelUsrCtrlReq
