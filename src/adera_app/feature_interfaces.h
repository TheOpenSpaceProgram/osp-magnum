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
#pragma once

#include <osp/framework/framework.h>

namespace ftr_inter
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
 *
 * This is used as a workaround for flaws in osp/tasks/execute.cpp being incapable of handling
 * certain patterns of task dependencies
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


enum class EStgLink
{
    ScheduleLink,
    NodeUpd,
    MachUpd
};
OSP_DECLARE_STAGE_NAMES(EStgLink, "Schedule", "NodeUpd", "MachUpd");
OSP_DECLARE_STAGE_SCHEDULE(EStgLink, EStgLink::ScheduleLink);

namespace stages
{
    using enum EStgOptn;
    using enum EStgCont;
    using enum EStgIntr;
    using enum EStgRevd;
    using enum EStgEvnt;
    using enum EStgFBO;
    using enum EStgLink;
} // namespace stages


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
    osp::PipelineInfo::register_stage_enum<EStgLink>();
}

using osp::PipelineDef;
using osp::fw::DataId;

//-----------------------------------------------------------------------------

struct FIMainApp {
    struct DataIds {
        DataId appContexts;
        DataId resources;
        DataId mainLoopCtrl;
        DataId frameworkModify;
    };

    struct Pipelines {
        PipelineDef<EStgOptn> mainLoop          {"mainLoop"};
        PipelineDef<EStgOptn> stupidWorkaround  {"stupidWorkaround - mainLoop needs a child or else something errors"};
    };
};

struct FICleanupContext {
    struct DataIds { };

    struct Pipelines {
        PipelineDef<EStgEvnt> cleanup           {"cleanup           - Scene cleanup before destruction"};
        PipelineDef<EStgOptn> cleanupWorkaround {"cleanupWorkaround"};
    };
};

//-----------------------------------------------------------------------------

struct FIEngineTest {
    struct DataIds {
        DataId bigStruct;
    };

    struct Pipelines { };
};

struct FIEngineTestRndr {
    struct DataIds {
        DataId renderer;
    };

    struct Pipelines { };
};



struct FIScene {
    struct DataIds {
        DataId deltaTimeIn;
        DataId loopControl;
    };

    struct Pipelines {
        PipelineDef<EStgCont> loopControl       {"loopControl"};
        PipelineDef<EStgOptn> update            {"update"};
    };
};


struct FICommonScene {
    struct DataIds {
        DataId basic;
        DataId drawing;
        DataId drawingRes;
        DataId activeEntDel;
        DataId drawEntDel;
        DataId namedMeshes;
    };

    struct Pipelines {
        PipelineDef<EStgCont> activeEnt         {"activeEnt         - ACtxBasic::m_activeIds"};
        PipelineDef<EStgOptn> activeEntResized  {"activeEntResized  - ACtxBasic::m_activeIds option to resize"};
        PipelineDef<EStgIntr> activeEntDelete   {"activeEntDelete   - comScn.di.activeEntDel, vector of ActiveEnts that need to be deleted"};

        PipelineDef<EStgCont> transform         {"transform         - ACtxBasic::m_transform"};
        PipelineDef<EStgCont> hierarchy         {"hierarchy         - ACtxBasic::m_scnGraph"};
    };
};


struct FIPhysics {
    struct DataIds {
        DataId phys;
        DataId hierBody;
        DataId physIn;
    };

    struct Pipelines {
        PipelineDef<EStgCont> physBody          {"physBody"};
        PipelineDef<EStgOptn> physUpdate        {"physUpdate"};
    };
};


struct FIPhysShapes {
    struct DataIds {
        DataId physShapes;
    };

    struct Pipelines {
        PipelineDef<EStgIntr> spawnRequest      {"spawnRequest      - Spawned shapes"};
        PipelineDef<EStgIntr> spawnedEnts       {"spawnedEnts"};
        PipelineDef<EStgRevd> ownedEnts         {"ownedEnts"};
    };
};


struct FIPhysShapesDraw {
    struct DataIds {
        DataId material;
    };

    struct Pipelines { };
};

struct FIThrower {
    struct DataIds {
        DataId button;
    };

    struct Pipelines { };
};


struct FIDroppers {
    struct DataIds {
        DataId timerA;
        DataId timerB;
    };

    struct Pipelines { };
};


struct FIPrefabs {
    struct DataIds {
        DataId prefabs;
    };

    struct Pipelines {
        PipelineDef<EStgIntr> spawnRequest      {"spawnRequest"};
        PipelineDef<EStgIntr> spawnedEnts       {"spawnedEnts"};
        PipelineDef<EStgRevd> ownedEnts         {"ownedEnts"};

        PipelineDef<EStgCont> instanceInfo      {"instanceInfo"};

        PipelineDef<EStgOptn> inSubtree         {"inSubtree"};
    };
};


struct FIPrefabDraw {
    struct DataIds {
        DataId material;
    };

    struct Pipelines { };
};


struct FIBounds {
    struct DataIds {
        DataId bounds;
        DataId outOfBounds;
    };

    struct Pipelines {
        PipelineDef<EStgCont> boundsSet         {"boundsSet"};
        PipelineDef<EStgRevd> outOfBounds       {"outOfBounds"};
    };
};


struct FIParts {
    struct DataIds {
        DataId scnParts;
        DataId updMach;
    };

    struct Pipelines {
        PipelineDef<EStgCont> partIds           {"partIds           - ACtxParts::partIds"};
        PipelineDef<EStgCont> partPrefabs       {"partPrefabs       - ACtxParts::partPrefabs"};
        PipelineDef<EStgCont> partTransformWeld {"partTransformWeld - ACtxParts::partTransformWeld"};
        PipelineDef<EStgIntr> partDirty         {"partDirty         - ACtxParts::partDirty"};

        PipelineDef<EStgCont> weldIds           {"weldIds           - ACtxParts::weldIds"};
        PipelineDef<EStgIntr> weldDirty         {"weldDirty         - ACtxParts::weldDirty"};

        PipelineDef<EStgCont> machIds           {"machIds           - ACtxParts::machines.ids"};
        PipelineDef<EStgCont> nodeIds           {"nodeIds           - ACtxParts::nodePerType[*].nodeIds"};
        PipelineDef<EStgCont> connect           {"connect           - ACtxParts::nodePerType[*].nodeToMach/machToNode"};

        PipelineDef<EStgCont> mapWeldPart       {"mapPartWeld       - ACtxParts::weldToParts/partToWeld"};
        PipelineDef<EStgCont> mapPartMach       {"mapPartMach       - ACtxParts::partToMachines/machineToPart"};
        PipelineDef<EStgCont> mapPartActive     {"mapPartActive     - ACtxParts::partToActive/activeToPart"};
        PipelineDef<EStgCont> mapWeldActive     {"mapWeldActive     - ACtxParts::weldToActive"};

        PipelineDef<EStgCont> machUpdExtIn      {"machUpdExtIn      -"};

        PipelineDef<EStgLink> linkLoop          {"linkLoop          - Link update loop"};
    };
};


struct FIVehicleSpawn {
    struct DataIds {
        DataId vehicleSpawn;
    };

    struct Pipelines {
        PipelineDef<EStgIntr> spawnRequest      {"spawnRequest      - ACtxVehicleSpawn::spawnRequest"};
        PipelineDef<EStgIntr> spawnedParts      {"spawnedParts      - ACtxVehicleSpawn::spawnedPart*"};
        PipelineDef<EStgIntr> spawnedWelds      {"spawnedWelds      - ACtxVehicleSpawn::spawnedWeld*"};
        PipelineDef<EStgIntr> rootEnts          {"rootEnts          - ACtxVehicleSpawn::rootEnts"};
        PipelineDef<EStgIntr> spawnedMachs      {"spawnedMachs      - ACtxVehicleSpawn::spawnedMachs"};
    };
};


struct FIVehicleSpawnVB {
    struct DataIds {
        DataId vehicleSpawnVB;
    };

    struct Pipelines {
        PipelineDef<EStgIntr> dataVB            {"dataVB            - ACtxVehicleSpawnVB::dataVB"};
        PipelineDef<EStgIntr> remapParts        {"remapParts        - ACtxVehicleSpawnVB::remapPart*"};
        PipelineDef<EStgIntr> remapWelds        {"remapWelds        - ACtxVehicleSpawnVB::remapWeld*"};
        PipelineDef<EStgIntr> remapMachs        {"remapMachs        - ACtxVehicleSpawnVB::remapMach*"};
        PipelineDef<EStgIntr> remapNodes        {"remapNodes        - ACtxVehicleSpawnVB::remapNode*"};
    };
};


struct FITestVehicles {
    struct DataIds {
        DataId prebuiltVehicles;
    };

    struct Pipelines { };
};


struct FISignalsFloat {
    struct DataIds {
        DataId sigValFloat;
        DataId sigUpdFloat;
    };

    struct Pipelines {
        PipelineDef<EStgCont> sigFloatValues    {"sigFloatValues    -"};
        PipelineDef<EStgCont> sigFloatUpdExtIn  {"sigFloatUpdExtIn  -"};
        PipelineDef<EStgCont> sigFloatUpdLoop   {"sigFloatUpdLoop   -"};
    };
};


//#define TESTAPP_DATA_NEWTON 1, \
//    idNwt
//struct PlNewton
//{
//    PipelineDef<EStgCont> nwtBody           {"nwtBody"};
//};

//#define TESTAPP_DATA_NEWTON_FORCES 1, \
//    idNwtFactors



//#define TESTAPP_DATA_NEWTON_ACCEL 1, \
//    idAcceleration

struct FIJolt {
    struct DataIds {
        DataId jolt;
    };

    struct Pipelines {
        PipelineDef<EStgCont> joltBody           {"joltBody"};
    };
};

struct FIPhysShapesJolt {
    struct DataIds {
        DataId factors;
    };

    struct Pipelines { };
};

struct FIVhclSpawnJolt {
    struct DataIds {
        DataId factors;
    };

    struct Pipelines { };
};

struct FIJoltConstAccel {
    struct DataIds {
        DataId accel;
    };

    struct Pipelines { };
};

//struct FIRocketsNwt {
//    struct DataIds {
//        DataId rocketsNwt;
//    };

//    struct Pipelines { };
//};


struct FIRocketsJolt {
    struct DataIds {
        DataId rocketsJolt;
        DataId factors;
    };

    struct Pipelines { };
};


struct FITerrain {
    struct DataIds {
        DataId terrainFrame;
        DataId terrain;
    };

    struct Pipelines {
        PipelineDef<EStgCont> skeleton          {"skeleton"};
        PipelineDef<EStgIntr> surfaceChanges    {"surfaceChanges"};
        PipelineDef<EStgCont> chunkMesh         {"chunkMesh"};
        PipelineDef<EStgCont> terrainFrame      {"terrainFrame"};
    };
};

struct FITerrainIco {
    struct DataIds {
        DataId terrainIco;
    };

    struct Pipelines { };
};

struct FITerrainDbgDraw {
    struct DataIds {
        DataId draw;
    };

    struct Pipelines { };
};




//-----------------------------------------------------------------------------

// Universe sessions

struct FIUniCore {
    struct DataIds {
        DataId universe;
        DataId deltaTimeIn;
    };

    struct Pipelines {
        PipelineDef<EStgOptn> update            {"update            - Universe update"};
        PipelineDef<EStgIntr> transfer          {"transfer"};
    };
};


struct FIUniSceneFrame {
    struct DataIds {
        DataId scnFrame;
    };

    struct Pipelines {
        PipelineDef<EStgCont> sceneFrame        {"sceneFrame"};
    };
};


struct FIUniPlanets {
    struct DataIds {
        DataId planetMainSpace;
        DataId satSurfaceSpaces;
    };

    struct Pipelines { };
};

struct FIUniPlanetsDraw {
    struct DataIds {
        DataId planetDraw;
    };

    struct Pipelines { };
};


//-----------------------------------------------------------------------------

// Solar System sessions

struct FISolarSys {
    struct DataIds {
        DataId planetMainSpace;
        DataId satSurfaceSpaces;
        DataId coordNBody;
    };

    struct Pipelines { };
};

struct FISolarSysDraw {
    struct DataIds {
        DataId planetDraw;
    };

    struct Pipelines { };
};

//-----------------------------------------------------------------------------

// Renderer sessions, tend to exist only when the window is open

struct FIWindowApp {
    struct DataIds {
        DataId windowAppLoopCtrl;
        DataId userInput;
    };

    struct Pipelines {
        PipelineDef<EStgOptn> inputs            {"inputs"};
        PipelineDef<EStgOptn> sync              {"sync"};
        PipelineDef<EStgOptn> resync            {"resync"};
    };
};


struct FISceneRenderer {
    struct DataIds {
        DataId scnRender;
        DataId drawTfObservers;
    };

    struct Pipelines {
        PipelineDef<EStgOptn> render            {"render            - "};

        PipelineDef<EStgCont> drawEnt           {"drawEnt           - "};
        PipelineDef<EStgOptn> drawEntResized    {"drawEntResized    - "};
        PipelineDef<EStgIntr> drawEntDelete     {"drawEntDelete     - Vector of DrawEnts that need to be deleted"};

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
};


struct FICameraControl {
    struct DataIds {
        DataId camCtrl;
    };

    struct Pipelines {
        PipelineDef<EStgCont> camCtrl           {"camCtrl"};
    };
};


struct FIIndicator {
    struct DataIds {
        DataId indicator;
    };

    struct Pipelines { };
};

struct FIRktIndicator {
    struct DataIds {
        DataId indicator;
    };

    struct Pipelines { };
};

struct FICursor {
    struct DataIds {
        DataId drawEnt;
    };

    struct Pipelines { };
};



struct FIVehicleControl {
    struct DataIds {
        DataId vhControls;
    };

    struct Pipelines {
        PipelineDef<EStgCont> selectedVehicle   {"selectedVehicle"};
    };
};


} // namespace ftr_inter
