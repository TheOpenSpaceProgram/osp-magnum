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

#include <osp/tasks/tasks.h>

#include <array>

namespace testapp
{


enum class EStgFlag : uint8_t
{
    Wait_,
    Write
};
OSP_DECLARE_STAGE_NAMES(EStgFlag, "Wait", "Write");


enum class EStgEvnt : uint8_t
{
    Wait,
    Run
};
OSP_DECLARE_STAGE_NAMES(EStgEvnt, "Wait", "Run");


/**
 * @brief Temporary queue / events that are filled, used, then cleared right away
 */
enum class EStgIntr : uint8_t
{
    Resize,
    Modify_,
    Use_,
    Clear
};
OSP_DECLARE_STAGE_NAMES(EStgIntr, "Resize", "Modify", "Use", "Clear");


/**
 * @brief Continuous Containers, data that persists and is modified over time
 *
 */
enum class EStgCont : uint8_t
{
    Delete,
    ///< Remove elements from a container or mark them for deletion. This often involves reading
    ///< a set of elements to delete. This is run first since it leaves empty spaces for new
    ///< elements to fill directly after

    New,
    ///< Add new elements. Potentially resize the container to fit more elements

    Modify,
    ///< Modify existing elements

    Use
    ///< Container is ready to use
};
OSP_DECLARE_STAGE_NAMES(EStgCont, "Delete", "New", "Modify", "Use");


enum class EStgRender
{
    Bind,
    Draw,
    Unbind
};
OSP_DECLARE_STAGE_NAMES(EStgRender, "Bind", "Draw", "Unbind");


using osp::PipelineDef;

//-----------------------------------------------------------------------------

#define TESTAPP_DATA_SCENE 1, \
    idDeltaTimeIn
struct PlScene
{
    PipelineDef<EStgEvnt> cleanup           {"cleanup - Scene cleanup before destruction"};
    PipelineDef<EStgEvnt> resyncAll         {"resyncAll - Resynchronize with renderer"};

    PipelineDef<EStgEvnt> updTime           {"time - External Delta Time In"};
    PipelineDef<EStgEvnt> updActive         {"updActive - Updates on ActiveEnt and components"};
    PipelineDef<EStgEvnt> updDraw           {"updDraw - Updates on DrawEnt and components"};
};

#define TESTAPP_DATA_COMMON_SCENE 6, \
    idBasic, idDrawing, idDrawingRes, idActiveEntDel, idDrawEntDel, idNMesh
struct PlCommonScene
{
    PipelineDef<EStgCont> activeEnt         {"activeEnt"};
    PipelineDef<EStgFlag> activeEntResized  {"activeEntResized"};
    PipelineDef<EStgIntr> activeEntDelete   {"activeEntDelete"};

    PipelineDef<EStgCont> transform         {"transform"};
    PipelineDef<EStgCont> hierarchy         {"hierarchy"};

    PipelineDef<EStgCont> drawEnt           {"drawEnt"};
    PipelineDef<EStgFlag> drawEntResized    {"drawEntResized"};
    PipelineDef<EStgIntr> drawEntDelete     {"drawEntDelete"};

    PipelineDef<EStgCont> mesh              {"mesh"};
    PipelineDef<EStgCont> texture           {"texture"};

    PipelineDef<EStgIntr> entTextureDirty   {"entTextureDirty"};
    PipelineDef<EStgIntr> entMeshDirty      {"entMeshDirty"};

    PipelineDef<EStgEvnt> meshResDirty      {"meshResDirty"};
    PipelineDef<EStgEvnt> textureResDirty   {"textureResDirty"};

    PipelineDef<EStgCont> material          {"material"};
    PipelineDef<EStgIntr> materialDirty     {"materialDirty"};
};


#define TESTAPP_DATA_PHYSICS 3, \
    idPhys, idHierBody, idPhysIn
struct PlPhysics
{
    PipelineDef<EStgCont> physics           {"physics"};
};



#define TESTAPP_DATA_SHAPE_SPAWN 1, \
    idSpawner
struct PlShapeSpawn
{
    PipelineDef<EStgIntr> spawnRequest      {"spawnRequest"};
    PipelineDef<EStgIntr> spawnedEnts       {"spawnedEnts"};
};



#define TESTAPP_DATA_PREFABS 1, \
    idPrefabInit
#define OSP_TAGS_TESTAPP_PREFABS 7, \
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
struct PlNewton
{
    PipelineDef<EStgCont> nwtBody;
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
struct PlWindowApp
{
    PipelineDef<EStgEvnt> inputs            {"inputs - User inputs in"};
    PipelineDef<EStgEvnt> display           {"display - Display new frame"};
};



#define TESTAPP_DATA_MAGNUM 2, \
    idActiveApp, idRenderGl
struct PlMagnum
{
    PipelineDef<EStgEvnt> cleanup           {"cleanup Cleanup Magnum"};

    PipelineDef<EStgCont> meshGL            {"meshGL"};
    PipelineDef<EStgCont> textureGL         {"textureGL"};

    PipelineDef<EStgCont> entMeshGL         {"entMeshGL"};
    PipelineDef<EStgCont> entTextureGL      {"entTextureGL"};
};



#define TESTAPP_DATA_COMMON_RENDERER 3, \
    idScnRender, idGroupFwd, idCamera
struct PlSceneRenderer
{
    PipelineDef<EStgRender> fboRender       {"fboRender"};

    PipelineDef<EStgCont> scnRender         {"scnRender"};
    PipelineDef<EStgCont> group             {"group"};
    PipelineDef<EStgCont> groupEnts         {"groupEnts"};
    PipelineDef<EStgIntr> drawTransforms    {"drawTransforms"};
    PipelineDef<EStgCont> camera            {"camera"};
    PipelineDef<EStgCont> entMesh           {"entMesh"};
    PipelineDef<EStgCont> entTexture        {"entTexture"};
};



#define TESTAPP_DATA_CAMERA_CTRL 1, \
    idCamCtrl
struct PlCameraCtrl
{
    PipelineDef<EStgCont> camCtrl           {"camCtrl"};
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
