/**
 * Open Space Program
 * Copyright © 2019-2025 Open Space Program Project
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
#include "universe_scene.h"

#include "../feature_interfaces.h"

#include <adera/universe_demo/simulations.h>
#include <adera/universe_demo/simulations_glue.h>

#include <adera/drawing/CameraController.h>

#include <osp/drawing/drawing.h>
#include <osp/universe/coordinates.h>
#include <osp/universe/universe.h>
#include <osp/util/logging.h>

using namespace adera;
using namespace ftr_inter::stages;
using namespace ftr_inter;
using namespace osp::draw;
using namespace osp::fw;
using namespace osp::universe;
using namespace osp;


namespace adera
{

FeatureDef const ftrSceneInUniverse = feature_def("UniverseSceneFrame", [] (
        FeatureBuilder                  &rFB,
        Implement<FISceneInUniverse>    scnInUni,
        DependOn<FIUniScenes>           uniScenes,
        DependOn<FICommonScene>         comScn,
        DependOn<FIMainApp>             mainApp,
        DependOn<FIUniCore>             uniCore)
{
    rFB.data_emplace< SceneId > (scnInUni.di.sceneId);

    //floatingOrigin.di.translateScene

    // scene in universe moves -> floating origin translation
    //                         ->

    rFB.task()
        .name       ("translate scene according to universe")
        .sync_with  ({ uniScenes.pl.requestTranslate(UseOrRun), comScn.pl.translateOrigin(Modify_) })
        .args       ({             comScn.di.basic, uniScenes.di.scenes, scnInUni.di.sceneId})
        .func       ([] (active::ACtxBasic &rBasic, UCtxScenes &rScenes,     SceneId sceneId) noexcept
    {
        ConnectedScene const &connection = rScenes.connectionOf[sceneId];

        rBasic.m_translateOrigin += Vector3(connection.requestTranslate) / 1024.0f;
    });

}); // ftrUniverseSceneFrame



struct FIUniDebugDraw {
    struct DataIds {
        DataId planetDraw;
    };

    struct Pipelines {
        PipelineDef<EStgOptn> resync  {"resync - Resync planet drawer with universe"};
        PipelineDef<EStgCont> trackedSats  {"trackedSats"};
    };
};

struct PlanetDraw
{

    struct TrackedSatellite
    {
        DrawEnt drawEnt;
        bool    isTracking{false};
    };

    //bool connected = false;
    bool doResync = false;

    osp::KeyedVec<CoSpaceId, CoordTransformer>      cospaceTransformToScnOf;

    osp::KeyedVec<SatelliteId, TrackedSatellite>    trackedSats;
    lgrn::IdSetStl<DataAccessorId>                  trackedAccessors;

    std::vector<DataAccessorId> accessorsByCospace;

    DrawEntVec_t            drawEnts;
    std::array<DrawEnt, 3>  axis;
    DrawEnt                 attractor;
    MaterialId              planetMat;
    MaterialId              axisMat;

};

/**
 * @brief Traverses a descendant-count tree starting from a specific node (instead of just root).
 *
 * Similar to rerooting then iterating from the new root, but keeping the old parent-child
 * relations.
 *
 * This will conventionally iterate the given initial target node's descendants (with recursion),
 * but then iterates up the chain of parents towards the root to traverse the entire tree.
 *
 * Custom 'ascend' (child to parent) and 'descend' (parent to child) functions must be provided,
 * called accordingly when reaching a new node.
 *
 */
template <typename STATE_T, typename CUTIE_MARK_T>
struct TreeWalker
{
    using TreePos_t = UCtxCoordSpaces::TreePos_t;

    void run(TreePos_t const initTarget, STATE_T const& initState, TreePos_t const root = 0)
    {
        auto        const childLast = initTarget + 1 + rDescendants[initTarget];
        TreePos_t         child     = initTarget + 1;

        while (child != childLast)
        {
            // NOLINTNEXTLINE(readability-suspicious-call-argument) shush. it's called recursion >:3
            descend_recurse(child, initTarget, initState);
            child += 1 + rDescendants[child]; // next child
        }

        if (initTarget != 0)
        {
            ascend_recurse(root, initTarget, initState);
        }
        // else, target is the root. has no ancestors
    }

    void descend_recurse(TreePos_t const target, TreePos_t const from, STATE_T const& fromState)
    {
        STATE_T     const targetState   = mark.descend(target, from, fromState);
        auto        const childLast     = target + 1 + rDescendants[target];
        TreePos_t         child         = target + 1;

        while (child != childLast)
        {
            descend_recurse(child, target, targetState);
            child += 1 + rDescendants[child]; // next child
        }
    }

    /**
     * First called from run() with parent=root, recurses down the parent-child chain towards
     * initTarget, then calls ascend_aux() chained upwards (deepest call first)
     */
    STATE_T ascend_recurse(TreePos_t const parent, TreePos_t const initTarget, STATE_T const& initState)
    {
        LGRN_ASSERT(parent < initTarget);

        auto        const childLast = parent + 1 + rDescendants[parent];
        TreePos_t         child     = parent + 1;

        while (child != childLast)
        {
            TreePos_t const nextChild = child + 1 + rDescendants[child];

            LGRN_ASSERT(child <= initTarget);

            if (child == initTarget)
            {
                // Done searching. This is now the deepest possible recursive call.
                return ascend_aux(parent, child, initState); // 'towards parent, from child'
            }
            else if (initTarget < nextChild) // is 'initTarget' a descendent of 'child'?
            {
                STATE_T childState = ascend_recurse(parent, child, initState);
                return ascend_aux(parent, child, childState);
            }
            child = nextChild;
        }
        return {}; // unreachable
    }

    STATE_T ascend_aux(TreePos_t const target, TreePos_t const from, STATE_T const& fromState)
    {
        // 'from' is a child of 'target'. Iterate it's siblings
        STATE_T           targetState   = mark.ascend(target, from, fromState);
        auto        const childLast     = target + 1 + rDescendants[target];
        TreePos_t         child         = target + 1;

        while (child != childLast)
        {
            if (child != from) // don't accidentally go back down to where we ascended from
            {
                // NOLINTNEXTLINE(readability-suspicious-call-argument)
                descend_recurse(child, target, targetState);
            }
            child += 1 + rDescendants[child]; // next child
        }
        return targetState; // RVO
    }

    CUTIE_MARK_T mark;
    osp::KeyedVec<TreePos_t, std::uint32_t> const &rDescendants;
};



struct CospaceTransformCalculator
{
    using TreePos_t = UCtxCoordSpaces::TreePos_t;

    CoordTransformer ascend(TreePos_t const target, TreePos_t const from, CoordTransformer const& fromToScn)
    {
        CoSpaceId        const parent    = rCS.treeToId[target];
        CoSpaceId        const child     = rCS.treeToId[from];
        CospaceTransform const &parentTf = rCS.transformOf[parent];
        CospaceTransform const &childTf  = rCS.transformOf[child];

        CospaceRelationship const relation
        {
            .parentPrecision = parentTf.precision,
            .childPrecision  = childTf.precision,
            .childPos        = childTf.position,
            .childRot        = childTf.rotation,
        };

        CoordTransformer const targetToFrom = CoordTransformer::from_parent_to_child(relation);

        // targetToScn = fromToScn(targetToFrom)
        rCospaceTransformToScnOf[parent] = CoordTransformer::from_composite(fromToScn, targetToFrom);
        return rCospaceTransformToScnOf[parent];
    }

    CoordTransformer descend(TreePos_t const target, TreePos_t const from, CoordTransformer const& fromToScn)
    {
        CoSpaceId        const parent    = rCS.treeToId[from];
        CoSpaceId        const child     = rCS.treeToId[target];
        CospaceTransform const &parentTf = rCS.transformOf[parent];
        CospaceTransform const &childTf  = rCS.transformOf[child];

        // process satellites of parent.

        CospaceRelationship const relation
        {
            .parentPrecision = parentTf.precision,
            .childPrecision  = childTf.precision,
            .childPos        = childTf.position,
            .childRot        = childTf.rotation,
        };

        CoordTransformer const targetToFrom = CoordTransformer::from_child_to_parent(relation);

        // targetToScn = fromToScn(targetToFrom)
        rCospaceTransformToScnOf[child] = CoordTransformer::from_composite(fromToScn, targetToFrom);
        return rCospaceTransformToScnOf[child];
    }

    osp::KeyedVec<CoSpaceId, CoordTransformer>       &rCospaceTransformToScnOf;
    UCtxCoordSpaces                            const &rCS;
};

FeatureDef const ftrUniverseDebugDraw = feature_def("UniverseDebugDraw", [] (
        FeatureBuilder              &rFB,
        Implement<FIUniDebugDraw>   uniDebugDraw,
        DependOn<FIMainApp>         mainApp,
        DependOn<FIWindowApp>       windowApp,
        DependOn<FISceneRenderer>   scnRender,
        DependOn<FICommonScene>     comScn,
        DependOn<FISceneInUniverse> scnInUni,
        DependOn<FIUniCore>         uniCore,
        DependOn<FIUniScenes>       uniScenes,
        entt::any                   userData)
{
    auto const &params = entt::any_cast<PlanetDrawParams>(userData);

    rFB.pipeline(uniDebugDraw.pl.resync).parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(uniDebugDraw.pl.trackedSats).parent(mainApp.loopblks.mainLoop);

    auto &rPlanetDraw  = rFB.data_emplace<PlanetDraw>(uniDebugDraw.di.planetDraw);


    rPlanetDraw.planetMat = params.planetMat;

     rFB.task()
        .name       ("Read universe datasource changes")
        .sync_with  ({uniDebugDraw.pl.trackedSats(Modify), uniDebugDraw.pl.resync(ModifyOrSignal), uniCore.pl.accessorIds(ReadyB4New), uniCore.pl.satIds(ReadyB4New)})
        .args       ({uniDebugDraw.di.planetDraw,          uniCore.di.dataAccessors,               uniCore.di.satInst,        uniCore.di.dataSrcs, uniCore.di.coordSpaces})
        .func       ([] (PlanetDraw &rPlanetDraw, UCtxDataAccessors &rDataAccessors, UCtxSatellites &rSatInst, UCtxDataSources &rDataSrcs, UCtxCoordSpaces const& rCoordSpaces) noexcept
    {
        // TODO: right now this just tracks everything and checks every update. add conditions later

        rPlanetDraw.doResync = true;

        rPlanetDraw.accessorsByCospace.clear();
        for (DataAccessorId const accessorId : rDataAccessors.ids)
        {
            rPlanetDraw.accessorsByCospace.push_back(accessorId);
        }


        rPlanetDraw.trackedAccessors.clear();
        rPlanetDraw.trackedAccessors.resize(rDataAccessors.ids.capacity());
        for (DataAccessorId const accessorId : rDataAccessors.ids)
        {
            rPlanetDraw.trackedAccessors.emplace(accessorId);
        }

        rPlanetDraw.trackedSats.resize(rSatInst.ids.capacity());
        for (SatelliteId const satId : rSatInst.ids)
        {
            rPlanetDraw.trackedSats[satId].isTracking = true;
        }
    });

    rFB.task()
        .name       ("Create universe draw entities")
        .sync_with  ({windowApp.pl.sync(Run), uniDebugDraw.pl.resync(Run), scnRender.pl.drawEnt(New), uniDebugDraw.pl.trackedSats(Ready)})
        .args       ({    uniDebugDraw.di.planetDraw,          uniCore.di.dataAccessors,               uniCore.di.satInst, uniCore.di.dataSrcs,                       uniCore.di.compTypes,      scnRender.di.scnRender })
        .func       ([] (      PlanetDraw &rPlanetDraw, UCtxDataAccessors &rDataAccessors, UCtxSatellites &rSatInst, UCtxDataSources &rDataSrcs, UCtxComponentTypes const& compTypes, ACtxSceneRender &rScnRender) noexcept
    {
        if (rPlanetDraw.doResync)
        {
            for (SatelliteId const satId : rSatInst.ids)
            {
                PlanetDraw::TrackedSatellite &rTrackedSat = rPlanetDraw.trackedSats[satId];

                if (rTrackedSat.isTracking)
                {
                    if ( ! rTrackedSat.drawEnt.has_value() )
                    {
                        rTrackedSat.drawEnt = rScnRender.m_drawIds.create();
                    }
                }
            }
        }
    });

    rFB.task()
        .name       ("Add mesh and materials to universe stuff")
        .sync_with  ({windowApp.pl.sync(Run), uniDebugDraw.pl.resync(Run), scnRender.pl.drawEnt(Ready), scnRender.pl.mesh(New), scnRender.pl.material(New), uniDebugDraw.pl.trackedSats(Ready)})
        .args       ({    uniDebugDraw.di.planetDraw,               uniCore.di.satInst,      scnRender.di.scnRender, comScn.di.drawing, comScn.di.namedMeshes })
        .func       ([] (      PlanetDraw &rPlanetDraw, UCtxSatellites &rSatInst, ACtxSceneRender &rScnRender, ACtxDrawing& rDrawing, NamedMeshes& rNamedMeshes) noexcept
    {
        if (rPlanetDraw.doResync)
        {
            MeshId const sphereMeshId = rNamedMeshes.m_shapeToMesh.at(EShape::Sphere);

            for (SatelliteId const satId : rSatInst.ids)
            {
                PlanetDraw::TrackedSatellite &rTrackedSat = rPlanetDraw.trackedSats[satId];

                if (rTrackedSat.isTracking)
                {
                    rScnRender.m_visible.insert(rTrackedSat.drawEnt);
                    rScnRender.m_opaque .insert(rTrackedSat.drawEnt);

                    if ( ! rScnRender.m_mesh[rTrackedSat.drawEnt].has_value() )
                    {
                        rScnRender.m_mesh[rTrackedSat.drawEnt] = rDrawing.m_meshRefCounts.ref_add(sphereMeshId);
                        rScnRender.m_meshDirty.push_back(rTrackedSat.drawEnt);

                        rScnRender.m_color[rTrackedSat.drawEnt] = {1.0f, 1.0f, 1.0f, 1.0f};

                        rScnRender.m_materials[rPlanetDraw.planetMat].m_ents.insert(rTrackedSat.drawEnt);
                        rScnRender.m_materials[rPlanetDraw.planetMat].m_dirty.push_back(rTrackedSat.drawEnt);
                    }
                }
            }
        }
    });

    rFB.task()
        .name       ("write draw transforms")
        .sync_with  ({
            windowApp.pl.sync(Run),
            uniDebugDraw.pl.resync(Run),
            scnRender.pl.drawEnt(Ready),
            scnRender.pl.mesh(New),
            scnRender.pl.material(New),
            uniCore.pl.accessors(Ready),
            uniCore.pl.accessorIds(Ready),
            uniCore.pl.cospaceTransform(Ready),
            uniDebugDraw.pl.trackedSats(Ready)})
        .args       ({
            uniDebugDraw.di.planetDraw,
            uniCore.di.dataAccessors,
            uniCore.di.coordSpaces,
            uniCore.di.simulations,
            uniCore.di.stolenSats,
            uniCore.di.satInst,
            uniCore.di.dataSrcs,
            uniCore.di.compTypes,
            uniScenes.di.scenes,
            scnRender.di.scnRender,
            scnInUni.di.sceneId})
        .func       ([] (
            PlanetDraw &rPlanetDraw,
            UCtxDataAccessors &rDataAccessors,
            UCtxCoordSpaces const& rCoordSpaces,
            UCtxSimulations &rSimulations,
            UCtxStolenSatellites &rStolenSats,
            UCtxSatellites &rSatInst,
            UCtxDataSources &rDataSrcs,
            UCtxComponentTypes const& compTypes,
            UCtxScenes const &rScenes,
            ACtxSceneRender &rScnRender,
            SceneId const sceneId) noexcept
    {
        rPlanetDraw.cospaceTransformToScnOf.resize(rCoordSpaces.ids.capacity());

        TreeWalker<CoordTransformer, CospaceTransformCalculator> walker
        {
            .mark = CospaceTransformCalculator{ rPlanetDraw.cospaceTransformToScnOf, rCoordSpaces },
            .rDescendants = rCoordSpaces.treeDescendants
        };

        // writes to rPlanetDraw.cospaceTransformToScnOf

        walker.run(rCoordSpaces.treeposOf[rScenes.connectionOf[sceneId].cospace], {});

        DefaultComponents const &dc = compTypes.defaults;
        for (DataAccessorId const accessorId : rPlanetDraw.trackedAccessors)
        {
            DataAccessor &rAccessor = rDataAccessors.instances[accessorId];

            UCtxStolenSatellites::OfAccessor const& deleted = rStolenSats.of[accessorId];

            CoordTransformer const &transformer = rPlanetDraw.cospaceTransformToScnOf[rAccessor.cospace];

            if (rAccessor.iterMethod == DataAccessor::IterationMethod::SkipNullSatellites)
            {
                auto iter = rAccessor.iterate(std::array{
                        dc.posX,  dc.posY,  dc.posZ,            // 0, 1, 2
                        dc.velX,  dc.velY,  dc.velZ,            // 3, 4, 5
                        dc.velXd, dc.velYd, dc.velZd,           // 6, 7, 8
                        dc.rotX,  dc.rotY,  dc.rotZ, dc.rotW,   // 9, 10, 11, 12
                        dc.satId});                             // 13

                bool const hasPosXYZ  = iter.has(0) && iter.has(1) && iter.has(2);
                bool const hasVelXYZ  = iter.has(3) && iter.has(4) && iter.has(5);
                bool const hasVelXYZd = iter.has(6) && iter.has(7) && iter.has(8);
                bool const hasRotXYZW = iter.has(9) && iter.has(10) && iter.has(11) && iter.has(12);

                LGRN_ASSERTM(iter.has(13), "SatelliteId missing");

                float const timeBehindBy = rAccessor.owner.has_value() ? float(rSimulations.simulationOf[rAccessor.owner].timeBehindBy) * 0.001f : 0.0f;


                for (std::size_t i = 0; i < rAccessor.count; ++i, iter.next())
                {
                    SatelliteId const satId = iter.get<SatelliteId>(13);

                    if (deleted.dirty && deleted.sats.contains(satId))
                    {
                        continue;
                    }

                    Vector3 moved{0.0f, 0.0f, 0.0f};

                    if (hasVelXYZ)
                    {
                        Vector3 const velocity {iter.get<float>(3), iter.get<float>(4), iter.get<float>(5)};
                        moved = velocity * timeBehindBy;
                    }

                    if (hasVelXYZd)
                    {
                        Vector3d const velocity {iter.get<double>(6), iter.get<double>(7), iter.get<double>(8)};
                        moved = Vector3(velocity * timeBehindBy);
                    }

                    if (hasPosXYZ)
                    {
                        Vector3g const pos {iter.get<spaceint_t>(0), iter.get<spaceint_t>(1), iter.get<spaceint_t>(2)};
                        Vector3d const d = Vector3d(transformer.transform_position(pos));
                        Vector3 const qux = Vector3(d / 1024.0) + moved;

                        PlanetDraw::TrackedSatellite &rTrackedSat = rPlanetDraw.trackedSats[satId];

                        LGRN_ASSERT(rTrackedSat.drawEnt.has_value());

                        rScnRender.m_drawTransform[rTrackedSat.drawEnt].translation() = qux;
                    }
                    if (hasRotXYZW)
                    {
                        Quaternion const rot { {iter.get<float>(9), iter.get<float>(10), iter.get<float>(11)}, iter.get<float>(12)};

                        PlanetDraw::TrackedSatellite &rTrackedSat = rPlanetDraw.trackedSats[satId];

                        LGRN_ASSERT(rTrackedSat.drawEnt.has_value());

                        Matrix3 const foo = rot.toMatrix();
                        rScnRender.m_drawTransform[rTrackedSat.drawEnt][0].xyz() = foo[0];
                        rScnRender.m_drawTransform[rTrackedSat.drawEnt][1].xyz() = foo[1];
                        rScnRender.m_drawTransform[rTrackedSat.drawEnt][2].xyz() = foo[2];
                    }
                }
            }
        }
    });

    rFB.task()
        .name       ("resync done")
        .sync_with  ({windowApp.pl.sync(Run), uniDebugDraw.pl.resync(Done)})
        .args       ({uniDebugDraw.di.planetDraw})
        .func       ([] (PlanetDraw &rPlanetDraw) noexcept
    {
        // for each
        rPlanetDraw.doResync = false;
    });

}); // ftrUniverseDebugDraw


} // namespace adera
