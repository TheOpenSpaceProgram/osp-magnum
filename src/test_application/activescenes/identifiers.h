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
// Used to set counts and declare variable names for TopDataIds and TagIds
// #define OSP_[DATA/TAGS]_NAME <# of identifiers>, a, b, c, d, ...

// Scene sessions

#define OSP_DATA_TESTAPP_COMMON_SCENE 7, \
    idDeltaTimeIn, idActiveIds, idBasic, idDrawing, idDrawingRes, idDelEnts, idDelTotal

#define OSP_TAGS_TESTAPP_COMMON_SCENE 34, \
    tgCleanupEvt,       tgResyncEvt,        tgSyncEvt,          tgSceneEvt,         tgTimeEvt,  \
    tgEntDel,           tgEntNew,           tgEntReq,                                           \
    tgDelEntMod,        tgDelEntReq,        tgDelEntClr,                                        \
    tgDelTotalMod,      tgDelTotalReq,      tgDelTotalClr,                                      \
    tgTransformMod,     tgTransformDel,     tgTransformNew,     tgTransformReq,                 \
    tgHierMod,          tgHierModEnd,       tgHierDel,          tgHierNew,          tgHierReq,  \
    tgDrawDel,          tgDrawMod,          tgDrawReq,                                          \
    tgMeshDel,          tgMeshMod,          tgMeshReq,          tgMeshClr,                      \
    tgTexDel,           tgTexMod,           tgTexReq,           tgTexClr



#define OSP_DATA_TESTAPP_MATERIAL 2, \
    idMatEnts, idMatDirty

#define OSP_TAGS_TESTAPP_MATERIAL 4, \
    tgMatDel, tgMatMod, tgMatReq, tgMatClr



#define OSP_DATA_TESTAPP_MATERIAL 2, \
    idMatEnts, idMatDirty

#define OSP_TAGS_TESTAPP_MATERIAL 4, \
    tgMatDel, tgMatMod, tgMatReq, tgMatClr



#define OSP_DATA_TESTAPP_PHYSICS 2, \
    idTPhys, idNMesh

#define OSP_TAGS_TESTAPP_PHYSICS 5, \
    tgPhysBodyDel,      tgPhysBodyMod,      tgPhysBodyReq,      \
    tgPhysMod,          tgPhysReq



#define OSP_DATA_TESTAPP_NEWTON 1, \
    idNwt



#define OSP_DATA_TESTAPP_SHAPE_SPAWN 2, \
    idSpawner, idSpawnerEnts

#define OSP_TAGS_TESTAPP_SHAPE_SPAWN 3, \
    tgSpawnMod,         tgSpawnReq,         tgSpawnClr



#define OSP_DATA_TESTAPP_PREFABS 1, \
    idPrefabInit

#define OSP_TAGS_TESTAPP_PREFABS 5, \
    tgPrefabMod,        tgPrefabReq,        tgPrefabClr,        \
    tgPrefabEntMod,     tgPrefabEntReq



#define OSP_DATA_TESTAPP_GRAVITY 1, \
    idGravity

#define OSP_TAGS_TESTAPP_GRAVITY 3, \
    tgGravityReq,       tgGravityDel,       tgGravityNew



#define OSP_DATA_TESTAPP_BOUNDS 1, \
    idBounds

#define OSP_TAGS_TESTAPP_BOUNDS 3, \
    tgBoundsReq,        tgBoundsDel,        tgBoundsNew



#define OSP_DATA_TESTAPP_PARTS 3, \
    idScnParts, idPartInit, idUpdMachines

#define OSP_TAGS_TESTAPP_PARTS 3, \
    tgPartInitMod,      tgPartInitReq,      tgPartInitClr



#define OSP_DATA_TESTAPP_SIGNALS_FLOAT 2, \
    idSigValFloat,      idSigUpdFloat

#define OSP_TAGS_TESTAPP_SIGNALS_FLOAT 3, \
    tgP


//-----------------------------------------------------------------------------

// Renderer sessions, tend to exist only when the window is open

#define OSP_DATA_TESTAPP_APP 1, \
    idUserInput

#define OSP_TAGS_TESTAPP_APP 2, \
    tgRenderEvt, tgInputEvt



#define OSP_DATA_TESTAPP_APP_MAGNUM 3, \
    idUserInput, idActiveApp, idRenderGl

#define OSP_TAGS_TESTAPP_APP_MAGNUM 3, \
    tgRenderEvt, tgInputEvt, tgGlUse



#define OSP_DATA_TESTAPP_COMMON_RENDERER 3, \
    idScnRender, idGroupFwd, idCamera

#define OSP_TAGS_TESTAPP_COMMON_RENDERER 18, \
    tgDrawGlDel,        tgDrawGlMod,        tgDrawGlReq,        \
    tgMeshGlMod,        tgMeshGlReq,                            \
    tgTexGlMod,         tgTexGlReq,                             \
    tgEntTexMod,        tgEntTexReq,                            \
    tgEntMeshMod,       tgEntMeshReq,                           \
    tgGroupFwdDel,      tgGroupFwdMod,      tgGroupFwdReq,      \
    tgDrawTransformDel, tgDrawTransformNew, tgDrawTransformMod, tgDrawTransformReq


#define OSP_DATA_TESTAPP_CAMERA_CTRL 1, \
    idCamCtrl

#define OSP_DATA_TESTAPP_SHADER_VISUALIZER 1, \
    idDrawVisual

#define OSP_TAGS_TESTAPP_SHADER_VISUALIZER 3, \
    tgRenderEvt, tgInputEvt, tgGlUse

