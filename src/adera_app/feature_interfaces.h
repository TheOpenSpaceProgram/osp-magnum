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
    ModifyOrSignal  = 0,
    Schedule        = 1,
    Run             = 2,
    Done            = 3
};
inline osp::PipelineTypeInfo const gc_infoForEStgOptn
{
    .debugName = "Optional",
    .stages = {{
        { .name = "Modify/Signal"                   },
        { .name = "Schedule",   .isSchedule = true  },
        { .name = "Run",        .useCancel = true   },
        { .name = "Done",                           }
    }},
    .initialStage = osp::StageId{0}
};

/**
 * @brief Intermediate container that is filled, used, then cleared right away
 */
enum class EStgIntr : uint8_t
{
    Resize      = 0,
    Modify_     = 1,
    Schedule_   = 2,
    UseOrRun    = 3,
    Clear       = 4
};
inline osp::PipelineTypeInfo const gc_infoForEStgIntr
{
    .debugName = "Intermediate container",
    .stages = {{
        { .name = "Resize"                          },
        { .name = "Modify"                          },
        { .name = "Schedule",   .isSchedule = true  },
        { .name = "UseOrRun",   .useCancel = true   },
        { .name = "Clear",      .useCancel = true   }
    }},
    .initialStage = osp::StageId{0}
};


/**
 * @brief Continuous Containers, data that persists and is modified over time
 *
 */
enum class EStgCont : uint8_t
{
    Delete      = 0,
    ///< Remove elements from a container or mark them for deletion. This often involves reading
    ///< a set of elements to delete. This is run first since it leaves empty spaces for new
    ///< elements to fill directly after

    Resize_     = 1,
    ///< Resize the container to fit more elements

    New         = 2,
    ///< Add new elements

    Modify      = 3,
    ///< Modify existing elements

    ScheduleC   = 4,

    Ready       = 5,
    ///< Container is ready to use

    ReadyWorkaround = 6,
};
inline osp::PipelineTypeInfo const gc_infoForEStgCont
{
    .debugName = "Continuous container",
    .stages = {{
        { .name = "Delete",                         },
        { .name = "Resize_",                        },
        { .name = "New",                            },
        { .name = "Modify",                         },
        { .name = "Schedule",   .isSchedule = true  },
        { .name = "Ready",                          },
        { .name = "ReadyWorkaround",                }
    }},
    .initialStage = osp::StageId{6}
};


enum class EStgEvnt : uint8_t
{
    Schedule__  = 0,
    Run_        = 1,
    Done_       = 2
};
inline osp::PipelineTypeInfo const gc_infoForEStgEvnt
{
    .debugName = "Event",
    .stages = {{
        { .name = "Schedule",   .isSchedule = true  },
        { .name = "Run",        .useCancel = true   },
        { .name = "Done",                           }
    }},
    .initialStage = osp::StageId{0}
};

enum class EStgFBO
{
    ScheduleFBO,
    Bind,
    Draw,
    Unbind
};
inline osp::PipelineTypeInfo const gc_infoForEStgFBO
{
    .debugName = "FrameBufferObject?",
    .stages = {{
        { .name = "ScheduleFBO", .isSchedule = true },
        { .name = "Bind",                           },
        { .name = "Draw",                           },
        { .name = "Unbind",                         }
    }},
    .initialStage = osp::StageId{0}
};


enum class EStgLink
{
    ScheduleLink,
    NodeUpd,
    MachUpd
};
inline osp::PipelineTypeInfo const gc_infoForEStgLink
{
    .debugName = "osp::link Nested update loop",
    .stages = {{
        { .name = "ScheduleLink", .isSchedule = true},
        { .name = "NodeUpd",      .useCancel = true },
        { .name = "MachUpd",      .useCancel = true }
    }},
    .initialStage = osp::StageId{0}
};


namespace stages
{
    using enum EStgOptn;
    using enum EStgCont;
    using enum EStgIntr;
    using enum EStgEvnt;
    using enum EStgFBO;
    using enum EStgLink;
} // namespace stages

inline void register_stage_enums()
{
    auto &rPltypeReg = osp::PipelineTypeIdReg::instance();
    rPltypeReg.assign_pltype_info<EStgOptn>(gc_infoForEStgOptn);
    rPltypeReg.assign_pltype_info<EStgEvnt>(gc_infoForEStgEvnt);
    rPltypeReg.assign_pltype_info<EStgIntr>(gc_infoForEStgIntr);
    rPltypeReg.assign_pltype_info<EStgCont>(gc_infoForEStgCont);
    rPltypeReg.assign_pltype_info<EStgFBO> (gc_infoForEStgFBO);
    rPltypeReg.assign_pltype_info<EStgLink>(gc_infoForEStgLink);
}


using osp::PipelineDef;
using osp::fw::DataId;
using osp::LoopBlockId;
using osp::TaskId;


//-----------------------------------------------------------------------------

struct FIMainApp {
    struct LoopBlockIds {
        LoopBlockId mainLoop;
    };

    struct DataIds {
        DataId appContexts;
        DataId resources;
        DataId mainLoopCtrl;
        DataId frameworkModify;
    };
    struct TaskIds {
        TaskId schedule;
        TaskId keepOpen;
    };

    struct Pipelines {
        PipelineDef<EStgOptn> keepOpen          {"keepOpen"};
    };
};

struct FICleanupContext {
    struct DataIds {
        DataId ranOnce;
    };

    struct TaskIds {
        TaskId blockSchedule;
        TaskId pipelineSchedule;
    };

    struct LoopBlockIds {
        LoopBlockId cleanup;
    };

    struct Pipelines {
        PipelineDef<EStgEvnt> cleanup           {"cleanup"};
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
        //PipelineDef<EStgCont> loopControl       {"loopControl"};
        PipelineDef<EStgOptn> update            {"update"};
    };
};


struct FICommonScene {
    struct DataIds {
        DataId basic;           ///< osp::active::ACtxBasic
        DataId drawing;         ///< osp::draw::ACtxDrawing
        DataId drawingRes;      ///< osp::draw::ACtxDrawingRes
        DataId activeEntDel;    ///< osp::active::ActiveEntVec_t
        DataId subtreeRootDel;  ///< osp::active::ActiveEntVec_t
        DataId namedMeshes;     ///< osp::draw::NamedMeshes
    };

    struct Pipelines {
        /// ACtxBasic::m_activeIds
        PipelineDef<EStgCont> activeEnt         {"activeEnt"};
        PipelineDef<EStgIntr> activeEntDelete   {"activeEntDelete"};
        PipelineDef<EStgIntr> subtreeRootDel    {"subtreeRootDel"};

        PipelineDef<EStgCont> transform         {"transform         - ACtxBasic::m_transform"};
        PipelineDef<EStgCont> hierarchy         {"hierarchy         - ACtxBasic::m_scnGraph"};

        /// drawing.m_meshIds
        PipelineDef<EStgCont> meshIds           {"meshIds"};
        /// drawing.m_texIds
        PipelineDef<EStgCont> texIds            {"texIds"};

        /// drawingRes.{m_resToTex, m_texToRes}
        PipelineDef<EStgCont> texToRes          {"texToRes"};
        /// drawingRes.{m_meshToRes, m_resToMesh}
        PipelineDef<EStgCont> meshToRes         {"meshToRes"};

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
        PipelineDef<EStgCont> ownedEnts         {"ownedEnts"};
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
        PipelineDef<EStgIntr> ownedEnts         {"ownedEnts"};

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
        PipelineDef<EStgIntr> outOfBounds       {"outOfBounds"};
    };
};


struct FILinks {

    struct LoopBlockIds {
        LoopBlockId link;
    };

    struct DataIds {
        DataId links;
        DataId updMach;
    };

    struct Pipelines {
        PipelineDef<EStgLink> linkLoop          {"linkLoop"};

        PipelineDef<EStgCont> machIds           {"machIds"};
        PipelineDef<EStgCont> nodeIds           {"nodeIds"};
        PipelineDef<EStgCont> connect           {"connect"};
        PipelineDef<EStgCont> machUpdExtIn      {"machUpdExtIn"};
    };
};


struct FIParts {


    struct DataIds {
        DataId scnParts;
    };

    struct Pipelines {
        PipelineDef<EStgCont> partIds           {"partIds           - ACtxParts::partIds"};
        PipelineDef<EStgCont> partPrefabs       {"partPrefabs       - ACtxParts::partPrefabs"};
        PipelineDef<EStgCont> partTransformWeld {"partTransformWeld - ACtxParts::partTransformWeld"};
        PipelineDef<EStgIntr> partDirty         {"partDirty         - ACtxParts::partDirty"};

        PipelineDef<EStgCont> weldIds           {"weldIds           - ACtxParts::weldIds"};
        PipelineDef<EStgIntr> weldDirty         {"weldDirty         - ACtxParts::weldDirty"};

        PipelineDef<EStgCont> mapWeldPart       {"mapPartWeld       - ACtxParts::weldToParts/partToWeld"};
        PipelineDef<EStgCont> mapPartMach       {"mapPartMach       - ACtxParts::partToMachines/machineToPart"};
        PipelineDef<EStgCont> mapPartActive     {"mapPartActive     - ACtxParts::partToActive/activeToPart"};
        PipelineDef<EStgCont> mapWeldActive     {"mapWeldActive     - ACtxParts::weldToActive"};


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

    struct Pipelines {
        PipelineDef<EStgCont> addedToHierarchy {"addedToHierarchy"};
    };
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
        DataId coordSpaces;
        DataId compTypes;
        DataId dataAccessors;
        DataId stolenSats;
        DataId dataSrcs;
        DataId satInst;
        DataId simulations;
    };

    struct Pipelines {
        PipelineDef<EStgOptn> update            {"update"};
        PipelineDef<EStgCont> satIds            {"satIds"};
        PipelineDef<EStgIntr> transfer          {"transfer"};
        PipelineDef<EStgCont> cospaceTransform  {"cospaceTransform"};
        PipelineDef<EStgCont> accessorsOfCospace{"accessorsOfCospace"};
        PipelineDef<EStgCont> accessorIds       {"accessorIds"};
        PipelineDef<EStgCont> accessors         {"accessors"};
        PipelineDef<EStgIntr> accessorDelete    {"accessorDelete"};
        PipelineDef<EStgCont> stolenSats        {"stolenSats"};
        PipelineDef<EStgCont> datasrcIds        {"datasrcIds"};
        PipelineDef<EStgCont> datasrcs          {"datasrcs"};
        PipelineDef<EStgCont> datasrcOf         {"datasrcOf"};
        PipelineDef<EStgIntr> datasrcChanges    {"datasrcChanges"};
        PipelineDef<EStgCont> simTimeBehindBy   {"simTimeBehindBy"};

    };
};

struct FIUniTransfers {
    struct DataIds {
        DataId intakes;
        DataId transferBufs;
    };

    struct Pipelines {
        PipelineDef<EStgIntr> requests          {"requests"};
        PipelineDef<EStgIntr> requestAccessorIds{"requestAccessorIds"};
        PipelineDef<EStgCont> midTransfer       {"midTransfer"};
        PipelineDef<EStgIntr> midTransferDelete {"midTransferDelete"};
    };
};

struct FISceneInUniverse {
    struct DataIds {
        DataId scnCospace;
    };

    struct Pipelines {
        //PipelineDef<EStgCont> sceneFrame        {"sceneFrame"};
    };
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

    struct TaskIds {
        TaskId scheduleInputs;
        TaskId scheduleSync;
        TaskId scheduleResync;
    };

    struct Pipelines {
        PipelineDef<EStgOptn> inputs            {"inputs"};
        PipelineDef<EStgOptn> sync              {"sync"};
        PipelineDef<EStgOptn> resync            {"resync"};
    };
};


struct FISceneRenderer {
    struct DataIds {
        DataId scnRender;       ///< osp::draw::ACtxSceneRender
        DataId drawTfObservers; ///< osp::draw::DrawTfObservers
        DataId drawEntDel;      ///< osp::draw::DrawEntVec_t
    };

    struct Pipelines {
        PipelineDef<EStgOptn> render            {"render"};

        /// scnRender.m_drawIds
        PipelineDef<EStgCont> drawEnt           {"drawEnt"};

        /// scnRender.{m_opaque, m_transparent, m_visible, m_color}
        PipelineDef<EStgCont> misc              {"misc"};

        /// scnRender.m_drawTransform
        PipelineDef<EStgCont> drawTransforms    {"drawTransforms"};

        /// scnRender.{m_needDrawTf, m_activeToDraw, drawTfObserverEnable}
        PipelineDef<EStgCont> activeDrawTfs     {"activeDrawTfs"};

        /// scnRender.m_diffuseTex
        PipelineDef<EStgCont> diffuseTex        {"diffuseTex"};
        /// scnRender.m_diffuseTexDirty
        PipelineDef<EStgIntr> diffuseTexDirty   {"diffuseTexDirty"};

        /// scnRender.m_mesh
        PipelineDef<EStgCont> mesh              {"mesh"};
        /// scnRender.m_meshDirty
        PipelineDef<EStgIntr> meshDirty         {"meshDirty"};

        /// scnRender.{m_materialIds, m_materials[#].m_ents}
        PipelineDef<EStgCont> material          {"material"};
        /// scnRender.m_materials[#].m_dirty
        PipelineDef<EStgIntr> materialDirty     {"materialDirty"};

        PipelineDef<EStgIntr> drawEntDelete     {"drawEntDelete"};
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
