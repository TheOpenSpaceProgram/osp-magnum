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


enum class EStgOptn : uint8_t
{
    ModifyOrSignal,
    Schedule,
    Run,
    Done
};
OSP_DECLARE_STAGE_NAMES(EStgOptn, "Modify/Signal", "Schedule", "Run", "Done");
OSP_DECLARE_STAGE_SCHEDULE(EStgOptn, EStgOptn::Schedule);


enum class EStgEvnt : uint8_t
{
    Run_,
    Done_
};
OSP_DECLARE_STAGE_NAMES(EStgEvnt, "Run", "Done");
OSP_DECLARE_STAGE_NO_SCHEDULE(EStgEvnt);

/**
 * @brief Intermediate container that is filled, used, then cleared right away
 */
enum class EStgIntr : uint8_t
{
    Resize,
    Modify_,
    Schedule_,
    UseOrRun,
    Clear
};
OSP_DECLARE_STAGE_NAMES(EStgIntr, "Resize", "Modify", "Schedule", "Use/Run", "Clear");
OSP_DECLARE_STAGE_SCHEDULE(EStgIntr, EStgIntr::Schedule_);

/**
 * @brief 'Reversed' Intermediate container
 */
enum class EStgRevd : uint8_t
{
    Schedule__,
    UseOrRun_,
    Clear_,
    Resize_,
    Modify__,
};
OSP_DECLARE_STAGE_NAMES(EStgRevd, "Schedule", "Use/Run", "Clear", "Resize", "Modify");
OSP_DECLARE_STAGE_SCHEDULE(EStgRevd, EStgRevd::Schedule__);

/**
 * @brief Continuous Containers, data that persists and is modified over time
 *
 */
enum class EStgCont : uint8_t
{
    Prev,
    ///< Previous state of container

    Delete,
    ///< Remove elements from a container or mark them for deletion. This often involves reading
    ///< a set of elements to delete. This is run first since it leaves empty spaces for new
    ///< elements to fill directly after

    New,
    ///< Add new elements. Potentially resize the container to fit more elements

    Modify,
    ///< Modify existing elements

    Ready
    ///< Container is ready to use
};
OSP_DECLARE_STAGE_NAMES(EStgCont, "Prev", "Delete", "New", "Modify", "Use");
OSP_DECLARE_STAGE_NO_SCHEDULE(EStgCont);

enum class EStgFBO
{
    Bind,
    Draw,
    Unbind
};
OSP_DECLARE_STAGE_NAMES(EStgFBO, "Bind", "Draw", "Unbind");
OSP_DECLARE_STAGE_NO_SCHEDULE(EStgFBO);

//-----------------------------------------------------------------------------

inline void register_stage_enums()
{
    osp::PipelineInfo::sm_stageNames.resize(32);
    osp::PipelineInfo::register_stage_enum<EStgOptn>();
    osp::PipelineInfo::register_stage_enum<EStgEvnt>();
    osp::PipelineInfo::register_stage_enum<EStgIntr>();
    osp::PipelineInfo::register_stage_enum<EStgRevd>();
    osp::PipelineInfo::register_stage_enum<EStgCont>();
    osp::PipelineInfo::register_stage_enum<EStgFBO>();
}

using osp::PipelineDef;

//-----------------------------------------------------------------------------

#define TESTAPP_DATA_APPLICATION 2, \
    idResources, idMainLoopCtrl
struct PlApplication
{
    PipelineDef<EStgOptn> mainLoop          {"mainLoop"};
};

//-----------------------------------------------------------------------------

#define TESTAPP_DATA_SCENE 1, \
    idDeltaTimeIn
struct PlScene
{
    PipelineDef<EStgEvnt> cleanup           {"cleanup - Scene cleanup before destruction"};
    PipelineDef<EStgOptn> update            {"update"};
};

#define TESTAPP_DATA_COMMON_SCENE 6, \
    idBasic, idDrawing, idDrawingRes, idActiveEntDel, idDrawEntDel, idNMesh
struct PlCommonScene
{
    PipelineDef<EStgCont> activeEnt         {"activeEnt - ActiveEnt ID Registry"};
    PipelineDef<EStgOptn> activeEntResized  {"activeEntResized"};
    PipelineDef<EStgIntr> activeEntDelete   {"activeEntDelete - Vector of ActiveEnts that need to be deleted"};

    PipelineDef<EStgCont> transform         {"transform"};
    PipelineDef<EStgCont> hierarchy         {"hierarchy"};
};


#define TESTAPP_DATA_PHYSICS 3, \
    idPhys, idHierBody, idPhysIn
struct PlPhysics
{
    PipelineDef<EStgCont> physBody          {"physBody"};
    PipelineDef<EStgOptn> physUpdate        {"physUpdate"};
};



#define TESTAPP_DATA_PHYS_SHAPES 1, \
    idPhysShapes
struct PlPhysShapes
{
    PipelineDef<EStgIntr> spawnRequest      {"spawnRequest - Spawned shapes "};
    PipelineDef<EStgIntr> spawnedEnts       {"spawnedEnts"};
    PipelineDef<EStgRevd> ownedEnts         {"ownedEnts"};
};



#define TESTAPP_DATA_PREFABS 1, \
    idPrefabInit
struct PlPrefabs
{
    PipelineDef<EStgIntr> spawnRequest      {"spawnRequest"};
    PipelineDef<EStgIntr> spawnedEnts       {"spawnedEnts"};
    PipelineDef<EStgRevd> ownedEnts         {"ownedEnts"};

    PipelineDef<EStgOptn> inSubtree         {"inSubtree"};
};



#define TESTAPP_DATA_BOUNDS 2, \
    idBounds, idOutOfBounds
struct PlBounds
{
    PipelineDef<EStgCont> boundsSet         {"boundsSet"};
    PipelineDef<EStgRevd> outOfBounds       {"outOfBounds"};
};



#define TESTAPP_DATA_PARTS 4, \
    idScnParts, idPartInit, idUpdMach, idMachEvtTags
struct PlParts
{
    PipelineDef<EStgIntr> part;
    PipelineDef<EStgIntr> mapPartEnt;
    PipelineDef<EStgIntr> weld;
    PipelineDef<EStgIntr> link;
    //PipelineDef<EStgIntr> linkMhUpd;
    //PipelineDef<EStgIntr> nodeAnyUpd;
};



#define TESTAPP_DATA_VEHICLE_SPAWN 1, \
    idVehicleSpawn
struct PlVehicleSpawn
{
    PipelineDef<EStgIntr> spawnRequest      {"VehicleSpawn boundsSet"};
    PipelineDef<EStgIntr> spawnedParts;
    PipelineDef<EStgIntr> spawnedWelds;
    PipelineDef<EStgIntr> rootEnts;

    PipelineDef<EStgIntr> mapPartMach;
};



#define TESTAPP_DATA_VEHICLE_SPAWN_VB 1, \
    idVehicleSpawnVB
struct PlVehicleSpawnVB
{
    PipelineDef<EStgIntr> dataVB;
    PipelineDef<EStgIntr> remapParts;
    PipelineDef<EStgIntr> remapWelds;
    PipelineDef<EStgIntr> remapMachs;
    PipelineDef<EStgIntr> remapNodes;
};
//#define OSP_TAGS_TESTAPP_VEHICLE_SPAWN_VB 10, \
//    tgVbSpBasicInMod,   tgVbSpBasicInReq,                       \
//    tgVbPartMod,        tgVbPartReq,                            \
//    tgVbWeldMod,        tgVbWeldReq,                            \
//    tgVbMachMod,        tgVbMachReq,                            \
//    tgVbNodeMod,        tgVbNodeReq


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
    PipelineDef<EStgCont> nwtBody           {"nwtBody"};
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
struct PlUniCore
{
    PipelineDef<EStgOptn> update            {"update - Universe update"};
    PipelineDef<EStgIntr> transfer          {"transfer"};
};

#define TESTAPP_DATA_UNI_SCENEFRAME 1, \
    idScnFrame
struct PlUniSceneFrame
{
    PipelineDef<EStgCont> sceneFrame        {"sceneFrame"};
};

#define TESTAPP_DATA_UNI_PLANETS 2, \
    idPlanetMainSpace, idSatSurfaceSpaces

//-----------------------------------------------------------------------------

// Renderer sessions, tend to exist only when the window is open

#define TESTAPP_DATA_WINDOW_APP 1, \
    idUserInput
struct PlWindowApp
{
    PipelineDef<EStgOptn> inputs            {"inputs"};
    PipelineDef<EStgOptn> sync              {"sync"};
    PipelineDef<EStgOptn> resync            {"resync"};

    PipelineDef<EStgEvnt> cleanup           {"cleanup - Cleanup renderer resources before destruction"};

};



#define TESTAPP_DATA_SCENE_RENDERER 1, \
    idScnRender
struct PlSceneRenderer
{
    PipelineDef<EStgOptn> render            {"render"};

    PipelineDef<EStgCont> drawEnt           {"drawEnt"};
    PipelineDef<EStgOptn> drawEntResized    {"drawEntResized"};
    PipelineDef<EStgIntr> drawEntDelete     {"drawEntDelete - Vector of DrawEnts that need to be deleted"};

    PipelineDef<EStgIntr> entTextureDirty   {"entTextureDirty"};
    PipelineDef<EStgIntr> entMeshDirty      {"entMeshDirty"};

    PipelineDef<EStgCont> material          {"material"};
    PipelineDef<EStgIntr> materialDirty     {"materialDirty"};

    PipelineDef<EStgIntr> drawTransforms    {"drawTransforms"};

    PipelineDef<EStgCont> group             {"group"};
    PipelineDef<EStgCont> groupEnts         {"groupEnts"};
    PipelineDef<EStgCont> entMesh           {"entMesh"};
    PipelineDef<EStgCont> entTexture        {"entTexture"};

    PipelineDef<EStgCont> mesh              {"mesh"};
    PipelineDef<EStgCont> texture           {"texture"};

    PipelineDef<EStgIntr> meshResDirty      {"meshResDirty"};
    PipelineDef<EStgIntr> textureResDirty   {"textureResDirty"};
};



#define TESTAPP_DATA_MAGNUM 2, \
    idActiveApp, idRenderGl
struct PlMagnum
{
    PipelineDef<EStgCont> meshGL            {"meshGL"};
    PipelineDef<EStgCont> textureGL         {"textureGL"};

    PipelineDef<EStgCont> entMeshGL         {"entMeshGL"};
    PipelineDef<EStgCont> entTextureGL      {"entTextureGL"};
};



#define TESTAPP_DATA_MAGNUM_SCENE 3, \
    idScnRenderGl, idGroupFwd, idCamera
struct PlMagnumScene
{
    PipelineDef<EStgFBO>  fbo               {"fboRender"};

    PipelineDef<EStgCont> camera            {"camera"};

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
