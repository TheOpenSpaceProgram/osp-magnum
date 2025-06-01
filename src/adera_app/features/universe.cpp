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
#include "universe.h"
#include "misc.h"

#include "../feature_interfaces.h"

#include <adera/universe_demo/simulations.h>
#include <adera/drawing/CameraController.h>

#include <osp/core/math_2pow.h>
#include <osp/drawing/drawing.h>
#include <osp/universe/coordinates.h>
#include <osp/universe/universe.h>
#include <osp/util/logging.h>

#include <random>
#include <unordered_set>


using namespace adera;
using namespace ftr_inter::stages;
using namespace ftr_inter;
using namespace osp::draw;
using namespace osp::fw;
using namespace osp::universe;
using namespace osp;

using Corrade::Containers::Array;

namespace adera
{
#if 0  // SYNCEXEC

// Universe Scenario

struct FIUniCore {
    struct DataIds {
        DataId coordSpaces;
        DataId compTypes;
        DataId compDefaults;
        DataId dataAccessors;
        DataId deletedSats;
        DataId dataSrcs;
        DataId satInst;
        DataId simulations;
        DataId time;
        DataId intakes;
        DataId deltaTimeIn;
    };

    struct Pipelines {
        PipelineDef<EStgOptn> update            {"update            - Universe update"};
        PipelineDef<EStgIntr> transfer          {"transfer"};
    };
};

FeatureDef const ftrUniverseCore = feature_def("UniverseCore", [] (
        FeatureBuilder              &rFB,
        Implement<FIUniCore>        uniCore,
        entt::any                   userData)
{
    rFB.data_emplace< float >       (uniCore.di.deltaTimeIn, 1.0f / 60.0f);

    auto &rCoordSpaces      = rFB.data_emplace< UCtxCoordSpaces >       (uniCore.di.coordSpaces);
    auto &rCompTypes        = rFB.data_emplace< UCtxComponentTypes >    (uniCore.di.compTypes);
    auto &rCompDefaults     = rFB.data_emplace< UCtxDefaultComponents > (uniCore.di.compDefaults);
    auto &rDataAccessors    = rFB.data_emplace< UCtxDataAccessors >     (uniCore.di.dataAccessors);
    auto &rDeletedSats      = rFB.data_emplace< UCtxDeletedSatellites > (uniCore.di.deletedSats);
    auto &rDataSrcs         = rFB.data_emplace< UCtxDataSources >       (uniCore.di.dataSrcs);
    auto &rSatInst          = rFB.data_emplace< UCtxSatelliteInstances >(uniCore.di.satInst);
    auto &rSimulations      = rFB.data_emplace< UCtxSimulations >       (uniCore.di.simulations);
    auto &rTime             = rFB.data_emplace< UCtxTime >              (uniCore.di.time);

    auto &rIntakes          = rFB.data_emplace< UCtxIntakes >           (uniCore.di.intakes);

    {
        auto gen = rCompTypes.ids.generator();
        rCompDefaults = {
            gen.create(), gen.create(), gen.create(), gen.create(), gen.create(),
            gen.create(), gen.create(), gen.create(), gen.create(), gen.create(),
            gen.create(), gen.create(), gen.create()};
    }

    auto const updateOn = entt::any_cast<PipelineId>(userData);

    rFB.pipeline(uniCore.pl.update).parent(updateOn);
    rFB.pipeline(uniCore.pl.transfer).parent(uniCore.pl.update);

}); // setup_uni_core


FeatureDef const ftrUniverseSceneFrame = feature_def("UniverseSceneFrame", [] (
        FeatureBuilder              &rFB,
        Implement<FIUniSceneFrame>  uniScnFrame,
        DependOn<FIUniCore>         uniCore)
{
    rFB.data_emplace< SceneFrame > (uniScnFrame.di.scnFrame);
    rFB.pipeline(uniScnFrame.pl.sceneFrame).parent(uniCore.pl.update);
}); // ftrUniverseSceneFrame


using adera::sims::CirclePathSim;
using adera::sims::ConstantSpinSim;

struct FITestSims {
    struct DataIds {
        DataId circleSim;
        DataId spinSim;
    };

    struct Pipelines { };
};

template <typename T>
DataAccessor::Component make_comp(T const* ptr, std::ptrdiff_t stride)
{
    return {reinterpret_cast<std::byte const*>(ptr), stride};
}

DataSourceId find_datasource(UCtxDataSources const& dataSrcs, DataSource const& query)
{
    std::size_t const size = query.entries.size();
    for (DataSourceId const dataSrcId : dataSrcs.ids)
    {
        DataSource const& candidate = dataSrcs.instances[dataSrcId];
        if (candidate.entries.size() != size)
        {
            continue;
        }

        if (std::memcmp(candidate.entries.data(), query.entries.data(), size * sizeof(DataSource::Entry)) != 0)
        {
            continue;
        }

        return dataSrcId;
    }
    return {};
}

FeatureDef const ftrUniverseTestPlanets = feature_def("UniverseTestPlanets", [] (
        FeatureBuilder              &rFB,
        Implement<FIUniPlanets>     uniPlanets,
        Implement<FITestSims>       sim,
        DependOn<FIUniSceneFrame>   uniScnFrame,
        DependOn<FIUniCore>         uniCore)
{
    using CoSpaceIdVec_t = std::vector<CoSpaceId>;

    auto &rCoordSpaces      = rFB.data_get< UCtxCoordSpaces >       (uniCore.di.coordSpaces);
    auto &rCompTypes        = rFB.data_get< UCtxComponentTypes >    (uniCore.di.compTypes);
    auto &rCompDefaults     = rFB.data_get< UCtxDefaultComponents > (uniCore.di.compDefaults);
    auto &rDataAccessors    = rFB.data_get< UCtxDataAccessors >     (uniCore.di.dataAccessors);
    auto &rDeletedSats      = rFB.data_get< UCtxDeletedSatellites > (uniCore.di.deletedSats);
    auto &rDataSrcs         = rFB.data_get< UCtxDataSources >       (uniCore.di.dataSrcs);
    auto &rSatInst          = rFB.data_get< UCtxSatelliteInstances >(uniCore.di.satInst);
    auto &rSimulations      = rFB.data_get< UCtxSimulations >       (uniCore.di.simulations);
    auto &rTime             = rFB.data_get< UCtxTime >              (uniCore.di.time);
    auto &rIntakes          = rFB.data_get< UCtxIntakes >           (uniCore.di.intakes);

    // Create coordinate spaces
    CoSpaceId const mainSpace = rCoordSpaces.ids.create();

    {
        // add 1 satellite
        SatelliteId const satId = rSatInst.ids.create();

        DataSourceId const srcId = rDataSrcs.ids.create();
        rDataSrcs.instances.resize(rDataSrcs.ids.capacity());

        DataSource &rDataSrc = rDataSrcs.instances[srcId];

        // Make circular path simulator for the satellite's position
        auto &rCircleSim = rFB.data_emplace< CirclePathSim >(sim.di.circleSim);
        rCircleSim.m_id = rSimulations.ids.create();
        rCircleSim.m_data.resize(1);
        rCircleSim.m_data[0].radius     = 4.0 * 1024.0;
        rCircleSim.m_data[0].initTime   = 0;
        rCircleSim.m_data[0].period     = 10000;
        rCircleSim.m_data[0].id         = satId;

        // setup data accessor to expose position and stuff
        {
            DataAccessor accessor;

            accessor.cospace = mainSpace;
            accessor.owner = rCircleSim.m_id;
            accessor.count = 1;

            constexpr std::size_t stride = sizeof(CirclePathSim::SatData);
            CirclePathSim::SatData const *pFirst = rCircleSim.m_data.data();
            accessor.components[rCompDefaults.posX]  = make_comp(&pFirst->position.data()[0], stride);
            accessor.components[rCompDefaults.posY]  = make_comp(&pFirst->position.data()[1], stride);
            accessor.components[rCompDefaults.posZ]  = make_comp(&pFirst->position.data()[2], stride);
            accessor.components[rCompDefaults.satId] = make_comp(&pFirst->id, stride);

            rCircleSim.m_accessor = std::make_shared<DataAccessor>(std::move(accessor));
            DataAccessorId const accessorId = rDataAccessors.ids.create();
            rDataAccessors.instances.resize(rDataAccessors.ids.size());
            rDataAccessors.instances[accessorId] = rCircleSim.m_accessor;

            DataSource::ComponentTypeSet_t comps;
            comps.emplace(rCompDefaults.posX);
            comps.emplace(rCompDefaults.posY);
            comps.emplace(rCompDefaults.posZ);
            comps.emplace(rCompDefaults.satId);
            rDataSrc.entries.push_back(DataSource::Entry{comps, accessorId});
        }

        // Make constant spin simulator to control the satellite's rotation
        auto &rSpinSim = rFB.data_emplace< ConstantSpinSim >(sim.di.spinSim);
        rSpinSim.m_id = rSimulations.ids.create();
        rSpinSim.m_data.resize(1);
        rSpinSim.m_data[0].initTime   = 0;
        rSpinSim.m_data[0].period     = 1000;
        rSpinSim.m_data[0].axis       = {0.0f, 0.0f, 1.0f};
        rSpinSim.m_data[0].id         = satId;

        {
            DataAccessor accessor;

            accessor.cospace = mainSpace;
            accessor.owner = rSpinSim.m_id;
            accessor.count = 1;

            constexpr std::size_t stride = sizeof(ConstantSpinSim::SatData);
            ConstantSpinSim::SatData const *pFirst = rSpinSim.m_data.data();

            accessor.components[rCompDefaults.rotX]  = make_comp(&pFirst->rot.data()[0], stride);
            accessor.components[rCompDefaults.rotY]  = make_comp(&pFirst->rot.data()[1], stride);
            accessor.components[rCompDefaults.rotZ]  = make_comp(&pFirst->rot.data()[2], stride);
            accessor.components[rCompDefaults.rotW]  = make_comp(&pFirst->rot.data()[3], stride);
            accessor.components[rCompDefaults.satId] = make_comp(&pFirst->id, stride);

            rSpinSim.m_accessor = std::make_shared<DataAccessor>(std::move(accessor));
            DataAccessorId const accessorId = rDataAccessors.ids.create();
            rDataAccessors.instances.resize(rDataAccessors.ids.size());
            rDataAccessors.instances[accessorId] = rSpinSim.m_accessor;

            DataSource::ComponentTypeSet_t comps;
            comps.emplace(rCompDefaults.rotX);
            comps.emplace(rCompDefaults.rotY);
            comps.emplace(rCompDefaults.rotZ);
            comps.emplace(rCompDefaults.rotW);
            comps.emplace(rCompDefaults.satId);
            rDataSrc.entries.push_back(DataSource::Entry{comps, accessorId});
        }

        rSatInst.dataSrc.resize(rSatInst.ids.capacity());
        rSatInst.dataSrc[satId] = srcId;
    }

    rDeletedSats.sets.resize(rDataAccessors.ids.capacity());

    constexpr int           precision       = 10;
    constexpr int           planetCount     = 64;
    constexpr int           seed            = 1337;
    constexpr spaceint_t    maxDist         = math::mul_2pow<spaceint_t, int>(20000ul, precision);
    constexpr float         maxVel          = 800.0f;


    std::vector<CoSpaceId> satSurfaceSpaces(planetCount);
    rCoordSpaces.ids.create(satSurfaceSpaces.begin(), satSurfaceSpaces.end());

    // finalize coordinate space tree
    auto const cospaceCapacity = rCoordSpaces.ids.capacity();
    rCoordSpaces.transforms     .resize(cospaceCapacity);
    rCoordSpaces.idParent       .resize(cospaceCapacity);
    rCoordSpaces.idToTreePos    .resize(cospaceCapacity);

    //rCoordSpaces.transforms[mainSpace].;
    rCoordSpaces.treeDescendants.assign({0u});
    rCoordSpaces.treeToId.assign({mainSpace});
    rCoordSpaces.idToTreePos[mainSpace] = 0;

    rFB.task()
        .name       ("Update planets")
        .run_on     (uniCore.pl.update(Run))
        .sync_with  ({uniScnFrame.pl.sceneFrame(Modify)})
        .args       ({            sim.di.circleSim,            sim.di.spinSim })
        .func       ([] (CirclePathSim &rCircleSim, ConstantSpinSim &rSpinSim) noexcept
    {
        static std::uint64_t time = 0;

        time+=15;
        rCircleSim.update(time);
        rSpinSim.update(time);
    });
}); // ftrUniverseTestPlanets

struct FIUniPlanetsDraw {
    struct DataIds {
        DataId planetDraw;
    };

    struct Pipelines {
        PipelineDef<EStgOptn> resync  {"resync - Resync planet drawer with universe"};
    };
};

struct PlanetDraw
{

    struct SatelliteToDraw
    {
        DrawEnt drawEnt;
    };

    struct TrackedCoordspace
    {
        std::unordered_set<DataAccessorId> accessors;
        osp::KeyedVec<SatelliteId, std::optional<SatelliteToDraw>> sats;
    };

    bool connected = false;
    bool doResync = false;


    // [coordspace][satset]-> (list of dataacessors)

    //std::unordered_map<SatelliteSetId, TrackedSatSet> trackedsatInst;
    std::unordered_map<CoSpaceId, TrackedCoordspace> cospaces;

    DrawEntVec_t            drawEnts;
    std::array<DrawEnt, 3>  axis;
    DrawEnt                 attractor;
    MaterialId              planetMat;
    MaterialId              axisMat;

};


FeatureDef const ftrUniverseTestPlanetsDraw = feature_def("UniverseTestPlanetsDraw", [] (
        FeatureBuilder              &rFB,
        Implement<FIUniPlanetsDraw> uniPlanetsDraw,
        DependOn<FIWindowApp>       windowApp,
        DependOn<FISceneRenderer>   scnRender,
        DependOn<FICameraControl>   camCtrl,
        DependOn<FICommonScene>     comScn,
        DependOn<FIUniCore>         uniCore,
        DependOn<FIUniSceneFrame>   uniScnFrame,
        DependOn<FIUniPlanets>      uniPlanets,
        entt::any                   userData)
{
    auto const &params = entt::any_cast<PlanetDrawParams>(userData);

    rFB.pipeline(uniPlanetsDraw.pl.resync) .parent(windowApp.pl.sync);

    auto &rPlanetDraw  = rFB.data_emplace<PlanetDraw>(uniPlanetsDraw.di.planetDraw);


    rPlanetDraw.planetMat = params.planetMat;


    rFB.task()
        .name       ("Read universe changes")
        .run_on     ({windowApp.pl.sync(Run)})
        .sync_with  ({uniPlanetsDraw.pl.resync(Schedule)})
        .args       ({uniPlanetsDraw.di.planetDraw,          uniCore.di.dataAccessors,               uniCore.di.satInst,        uniCore.di.dataSrcs,                   uniCore.di.compDefaults})
        .func       ([] (  PlanetDraw &rPlanetDraw, UCtxDataAccessors &rDataAccessors, UCtxSatelliteInstances &rSatInst, UCtxDataSources &rDataSrcs, UCtxDefaultComponents const& compDefaults) noexcept
    {
        if ( ! rPlanetDraw.connected)
        {
            rPlanetDraw.connected = true;

            DataSource::ComponentTypeSet_t compsRequired;
            compsRequired.emplace(compDefaults.posX);
            compsRequired.emplace(compDefaults.posY);
            compsRequired.emplace(compDefaults.posZ);

            rPlanetDraw.cospaces.clear();

            for (DataSourceId const id : rDataSrcs.ids)
            {
                DataSource const rSrc = rDataSrcs.instances[id];
                DataSource::ComponentTypeSet_t compsRemaining = compsRequired;

                for (DataSource::Entry const& entry : rSrc.entries)
                {
                    std::shared_ptr<DataAccessor> pAccessor = rDataAccessors.instances[entry.accessor].lock();

                    bool hasComponentsWeCareAbout = true;

                    // TODO
                    // for (ComponentTypeId const comp : entry.components)
                    // {
                    // }

                    if (hasComponentsWeCareAbout)
                    {
                        PlanetDraw::TrackedCoordspace &rTrack = rPlanetDraw.cospaces[pAccessor->cospace];
                        rTrack.accessors.emplace(entry.accessor);
                    }
                }
            }

            rPlanetDraw.doResync = true;

            // TODO: Subscribe to events
            // TODO: somehow modify pipelines to connect to universe?
        }

        // on subscriber added, just push them all events so they don't directly just read?

        // deal with events. satellites added/removed/transfered and stuff

        // loop through all dataaccessors that we can and actually read positions
        // the problem here is the 'resync' stage.
        // how to initially discover all the stuff in the universe?
    });

    rFB.task()
        .name       ("create draw entities")
        .run_on     ({windowApp.pl.sync(Run)})
        .sync_with  ({uniPlanetsDraw.pl.resync(Run), scnRender.pl.drawEntResized(ModifyOrSignal), scnRender.pl.drawEnt(New)})
        .args       ({    uniPlanetsDraw.di.planetDraw,  uniCore.di.dataAccessors,   uniCore.di.satInst, uniCore.di.dataSrcs,            uniCore.di.compDefaults, scnRender.di.scnRender })
        .func       ([] (      PlanetDraw &rPlanetDraw, UCtxDataAccessors &rDataAccessors, UCtxSatelliteInstances &rSatInst, UCtxDataSources &rDataSrcs, UCtxDefaultComponents const& compDefaults, ACtxSceneRender &rScnRender) noexcept
    {
        if (rPlanetDraw.doResync)
        {
            // create draw ents?

            auto drawEntGen = rScnRender.m_drawIds.generator();

            for (auto & [cospaceId, rTrackedCospace] : rPlanetDraw.cospaces)
            {
                rTrackedCospace.sats.resize(rSatInst.ids.capacity());
                for (SatelliteId const satId : rSatInst.ids)
                {
                    PlanetDraw::SatelliteToDraw &rSatToDraw = rTrackedCospace.sats[satId].emplace(PlanetDraw::SatelliteToDraw{});
                    rSatToDraw.drawEnt = drawEntGen.create();
                }
            }
        }

    });

    rFB.task()
        .name       ("Add mesh and materials to universe stuff")
        .run_on     ({windowApp.pl.sync(Run)})
        .sync_with  ({uniPlanetsDraw.pl.resync(Run), scnRender.pl.drawEntResized(Done), scnRender.pl.drawEnt(Ready), scnRender.pl.entMesh(New), scnRender.pl.material(New)})
        .args       ({    uniPlanetsDraw.di.planetDraw,          uniCore.di.dataAccessors,               uniCore.di.satInst,        uniCore.di.dataSrcs,            uniCore.di.compDefaults, scnRender.di.scnRender, comScn.di.drawing, comScn.di.namedMeshes })
        .func       ([] (      PlanetDraw &rPlanetDraw, UCtxDataAccessors &rDataAccessors, UCtxSatelliteInstances &rSatInst, UCtxDataSources &rDataSrcs, UCtxDefaultComponents const& compDefaults, ACtxSceneRender &rScnRender, ACtxDrawing& rDrawing, NamedMeshes& rNamedMeshes) noexcept
    {
        if (rPlanetDraw.doResync)
        {
            // eat everything
            //std::cout << "eat";

            // maybe for each satellite type?
            // or each coordspace?
            //
            // which one makes the most sense?
            // usually want to specify a coordspace. 'what is here'
            // write to roughly the same physical space.
            // better to have per satset/coordspace pair. or datasource
            // (satset,coordspace)-> (list of dataacessors)

            // loop through all dataacessors, and write positions and stuff
            //
            for (auto & [cospaceId, rTrackedCospace] : rPlanetDraw.cospaces)
            {
                for (SatelliteId const satId : rSatInst.ids)
                {
                    PlanetDraw::SatelliteToDraw &rSatToDraw = rTrackedCospace.sats[satId].value();
                    rScnRender.m_visible.insert(rSatToDraw.drawEnt);
                    rScnRender.m_opaque .insert(rSatToDraw.drawEnt);
                    MeshId const sphereMeshId = rNamedMeshes.m_shapeToMesh.at(EShape::Sphere);
                    rScnRender.m_mesh[rSatToDraw.drawEnt] = rDrawing.m_meshRefCounts.ref_add(sphereMeshId);
                    rScnRender.m_meshDirty.push_back(rSatToDraw.drawEnt);

                    rScnRender.m_color[rSatToDraw.drawEnt] = {1.0f, 1.0f, 1.0f, 1.0f};

                    rScnRender.m_materials[rPlanetDraw.planetMat].m_ents.insert(rSatToDraw.drawEnt);
                    rScnRender.m_materials[rPlanetDraw.planetMat].m_dirty.push_back(rSatToDraw.drawEnt);
                }
            }
        }
    });

    rFB.task()
        .name       ("write draw transforms")
        .run_on     ({windowApp.pl.sync(Run)})
        .sync_with  ({uniPlanetsDraw.pl.resync(Run), scnRender.pl.drawEntResized(Done), scnRender.pl.drawEnt(Ready), scnRender.pl.entMesh(New), scnRender.pl.material(New)})
        .args       ({    uniPlanetsDraw.di.planetDraw,          uniCore.di.dataAccessors,              uniCore.di.deletedSats,               uniCore.di.satInst,        uniCore.di.dataSrcs,                   uniCore.di.compDefaults,      scnRender.di.scnRender })
        .func       ([] (      PlanetDraw &rPlanetDraw, UCtxDataAccessors &rDataAccessors, UCtxDeletedSatellites &rDeletedSats, UCtxSatelliteInstances &rSatInst, UCtxDataSources &rDataSrcs, UCtxDefaultComponents const& compDefaults, ACtxSceneRender &rScnRender) noexcept
    {
        for (auto & [cospaceId, rTrackedCospace] : rPlanetDraw.cospaces)
        {
            for (DataAccessorId const accessorId : rTrackedCospace.accessors)
            {
                std::shared_ptr<DataAccessor> rInstance = rDataAccessors.instances[accessorId].lock();

                UCtxDeletedSatellites::Accessor const& deleted = rDeletedSats.sets[accessorId];

                if (rInstance->iterMethod == DataAccessor::IterationMethod::SkipNullSatellites)
                {
                    static constexpr auto indices = std::array<std::size_t const, 8u>{0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u};
                    auto const [idxPosX, idxPosY, idxPosZ, idxRotX, idxRotY, idxRotZ, idxRotW, idxSatId] = indices;

                    auto iter = rInstance->iterate(std::array{
                            compDefaults.posX, compDefaults.posY, compDefaults.posZ,
                            compDefaults.rotX, compDefaults.rotY, compDefaults.rotZ, compDefaults.rotW,
                            compDefaults.satId});

                    bool const hasPosXYZ  = iter.has(idxPosX) && iter.has(idxPosY) && iter.has(idxPosZ);
                    bool const hasRotXYZW = iter.has(idxRotX) && iter.has(idxRotY) && iter.has(idxRotZ) && iter.has(idxRotW);

                    LGRN_ASSERTM(iter.has(idxSatId), "other iteration methods not yet supported");



                    for (std::size_t i = 0; i < rInstance->count; ++i)
                    {
                        SatelliteId const satId = iter.get<SatelliteId>(idxSatId);

                        if (deleted.dirty && deleted.set.contains(satId))
                        {
                            continue;
                        }

                        if (hasPosXYZ)
                        {
                            Vector3g const pos {iter.get<spaceint_t>(idxPosX),
                                                iter.get<spaceint_t>(idxPosY),
                                                iter.get<spaceint_t>(idxPosZ)};

                            Vector3d const d = Vector3d(pos);
                            Vector3 const qux = Vector3(d / 1024.0);

                            PlanetDraw::SatelliteToDraw const &rSatToDraw = rTrackedCospace.sats[satId].value();

                            LGRN_ASSERT(rSatToDraw.drawEnt.has_value());

                            rScnRender.m_drawTransform[rSatToDraw.drawEnt].translation() = qux;
                        }

                        if (hasRotXYZW)
                        {
                            Quaternion const rot { {iter.get<float>(idxRotX), iter.get<float>(idxRotY), iter.get<float>(idxRotZ)}, iter.get<float>(idxRotW)};

                            PlanetDraw::SatelliteToDraw const &rSatToDraw = rTrackedCospace.sats[satId].value();

                            LGRN_ASSERT(rSatToDraw.drawEnt.has_value());

                            Matrix3 const foo = rot.toMatrix();
                            rScnRender.m_drawTransform[rSatToDraw.drawEnt][0].xyz() = foo[0];
                            rScnRender.m_drawTransform[rSatToDraw.drawEnt][1].xyz() = foo[1];
                            rScnRender.m_drawTransform[rSatToDraw.drawEnt][2].xyz() = foo[2];
                        }

                        iter.next();
                    }
                }
            }
        }
    });

    rFB.task()
        .name       ("resync done")
        .run_on     ({windowApp.pl.sync(Run)})
        .sync_with  ({uniPlanetsDraw.pl.resync(Done)})
        .args       ({uniPlanetsDraw.di.planetDraw })
        .func       ([] (PlanetDraw &rPlanetDraw) noexcept
    {
        // for each
        rPlanetDraw.doResync = false;
    });



        #if 0
    auto const params = entt::any_cast<PlanetDrawParams>(userData);

    rFB.data_emplace<PlanetDraw>(uniPlanetsDraw.di.planetDraw, PlanetDraw{
        .planetMat = params.planetMat,
        .axisMat   = params.axisMat,
    });

    rFB.task()
        .name       ("Position SceneFrame center to Camera Controller target")
        .run_on     ({windowApp.pl.inputs(Run)})
        .sync_with  ({camCtrl.pl.camCtrl(Ready), uniScnFrame.pl.sceneFrame(Modify)})
        .args       ({               camCtrl.di.camCtrl, uniScnFrame.di.scnFrame })
        .func       ([] (ACtxCameraController& rCamCtrl,   SceneFrame& rScnFrame) noexcept
    {
        if ( ! rCamCtrl.m_target.has_value())
        {
            return;
        }
        Vector3 &rCamPl = rCamCtrl.m_target.value();

        // check origin translation
        // ADL used for Magnum::Math::sign/floor/abs
        float const maxDist = 512.0f;
        Vector3 const translate = sign(rCamPl) * floor(abs(rCamPl) / maxDist) * maxDist;

        if ( ! translate.isZero())
        {
            rCamCtrl.m_transform.translation() -= translate;
            rCamPl -= translate;

            // a bit janky to modify universe stuff directly here, but it works lol
            Vector3 const rotated = Quaternion(rScnFrame.m_rotation).transformVector(translate);
            rScnFrame.m_position += Vector3g(math::mul_2pow<Vector3, int>(rotated, rScnFrame.m_precision));
        }

        rScnFrame.m_scenePosition = Vector3g(math::mul_2pow<Vector3, int>(rCamCtrl.m_target.value(), rScnFrame.m_precision));

    });

    rFB.task()
        .name       ("Resync test planets, create DrawEnts")
        .run_on     ({windowApp.pl.resync(Run)})
        .sync_with  ({scnRender.pl.drawEntResized(ModifyOrSignal)})
        .args       ({    scnRender.di.scnRender, uniPlanetsDraw.di.planetDraw, uniCore.di.universe,   uniPlanets.di.planetMainSpace})
        .func([]    (ACtxSceneRender& rScnRender,      PlanetDraw& rPlanetDraw, Universe& rUniverse, CoSpaceId const planetMainSpace) noexcept
    {
        CoSpaceCommon &rMainSpace = rUniverse.m_coordCommon[planetMainSpace];

        rPlanetDraw.drawEnts.resize(rMainSpace.m_satCount, lgrn::id_null<DrawEnt>());

        rScnRender.m_drawIds.create(rPlanetDraw.drawEnts   .begin(), rPlanetDraw.drawEnts   .end());
        rScnRender.m_drawIds.create(rPlanetDraw.axis       .begin(), rPlanetDraw.axis       .end());
        rPlanetDraw.attractor = rScnRender.m_drawIds.create();
    });

    rFB.task()
        .name       ("Resync test planets, add mesh and material")
        .run_on     ({windowApp.pl.resync(Run)})
        .sync_with  ({scnRender.pl.drawEntResized(Done), scnRender.pl.materialDirty(Modify_), scnRender.pl.meshDirty(Modify_)})
        .args       ({       comScn.di.drawing,      scnRender.di.scnRender,     comScn.di.namedMeshes, uniPlanetsDraw.di.planetDraw, uniCore.di.universe,   uniPlanets.di.planetMainSpace})
        .func       ([] (ACtxDrawing& rDrawing, ACtxSceneRender& rScnRender, NamedMeshes& rNamedMeshes,      PlanetDraw& rPlanetDraw, Universe& rUniverse, CoSpaceId const planetMainSpace) noexcept
    {
        CoSpaceCommon &rMainSpace = rUniverse.m_coordCommon[planetMainSpace];

        Material &rMatPlanet = rScnRender.m_materials[rPlanetDraw.planetMat];
        Material &rMatAxis   = rScnRender.m_materials[rPlanetDraw.axisMat];

        MeshId const sphereMeshId = rNamedMeshes.m_shapeToMesh.at(EShape::Sphere);
        MeshId const cubeMeshId   = rNamedMeshes.m_shapeToMesh.at(EShape::Box);

        for (std::size_t i = 0; i < rMainSpace.m_satCount; ++i)
        {
            DrawEnt const drawEnt = rPlanetDraw.drawEnts[i];

            rScnRender.m_mesh[drawEnt] = rDrawing.m_meshRefCounts.ref_add(sphereMeshId);
            rScnRender.m_meshDirty.push_back(drawEnt);
            rScnRender.m_visible.insert(drawEnt);
            rScnRender.m_opaque.insert(drawEnt);
            rMatPlanet.m_ents.insert(drawEnt);
            rMatPlanet.m_dirty.push_back(drawEnt);
        }

        rScnRender.m_mesh[rPlanetDraw.attractor] = rDrawing.m_meshRefCounts.ref_add(sphereMeshId);
        rScnRender.m_meshDirty.push_back(rPlanetDraw.attractor);
        rScnRender.m_visible.insert(rPlanetDraw.attractor);
        rScnRender.m_opaque.insert(rPlanetDraw.attractor);
        rMatPlanet.m_ents.insert(rPlanetDraw.attractor);
        rMatPlanet.m_dirty.push_back(rPlanetDraw.attractor);

        for (DrawEnt const drawEnt : rPlanetDraw.axis)
        {
            rScnRender.m_mesh[drawEnt] = rDrawing.m_meshRefCounts.ref_add(cubeMeshId);
            rScnRender.m_meshDirty.push_back(drawEnt);
            rScnRender.m_visible.insert(drawEnt);
            rScnRender.m_opaque.insert(drawEnt);
            rMatAxis.m_ents.insert(drawEnt);
            rMatAxis.m_dirty.push_back(drawEnt);
        }

        rScnRender.m_color[rPlanetDraw.axis[0]] = {1.0f, 0.0f, 0.0f, 1.0f};
        rScnRender.m_color[rPlanetDraw.axis[1]] = {0.0f, 1.0f, 0.0f, 1.0f};
        rScnRender.m_color[rPlanetDraw.axis[2]] = {0.0f, 0.0f, 1.0f, 1.0f};
    });

    rFB.task()
        .name       ("Reposition test planet DrawEnts")
        .run_on     ({scnRender.pl.render(Run)})
        .sync_with  ({scnRender.pl.drawTransforms(Modify), scnRender.pl.drawEntResized(Done), camCtrl.pl.camCtrl(Ready), uniScnFrame.pl.sceneFrame(Modify)})
        .args       ({       comScn.di.drawing,      scnRender.di.scnRender, uniPlanetsDraw.di.planetDraw, uniCore.di.universe,     uniScnFrame.di.scnFrame,   uniPlanets.di.planetMainSpace})
        .func       ([] (ACtxDrawing& rDrawing, ACtxSceneRender& rScnRender,      PlanetDraw& rPlanetDraw, Universe& rUniverse, SceneFrame const& rScnFrame, CoSpaceId const planetMainSpace) noexcept
    {
        CoSpaceCommon &rMainSpace = rUniverse.m_coordCommon[planetMainSpace];
        auto const [x, y, z]        = sat_views(rMainSpace.m_satPositions, rMainSpace.m_data, rMainSpace.m_satCount);
        auto const [qx, qy, qz, qw] = sat_views(rMainSpace.m_satRotations, rMainSpace.m_data, rMainSpace.m_satCount);

        // Calculate transform from universe to area/local-space for rendering.
        // This can be generalized by finding a common ancestor within the tree
        // of coordinate spaces. Since there's only two possibilities, an if
        // statement works.
        CoordTransformer mainToArea;
        if (rScnFrame.m_parent == planetMainSpace)
        {
            mainToArea = coord_parent_to_child(rMainSpace, rScnFrame);
        }
        else
        {
            CoSpaceId const landedId = rScnFrame.m_parent;
            CoSpaceCommon &rLanded = rUniverse.m_coordCommon[landedId];

            CoSpaceTransform const landedTf     = coord_get_transform(rLanded, rLanded, x, y, z, qx, qy, qz, qw);
            CoordTransformer const mainToLanded = coord_parent_to_child(rMainSpace, landedTf);
            CoordTransformer const landedToArea = coord_parent_to_child(landedTf, rScnFrame);

            mainToArea = coord_composite(landedToArea, mainToLanded);
        }
        Quaternion const mainToAreaRot{mainToArea.rotation()};

        float const scale = math::mul_2pow<float, int>(1.0f, -rMainSpace.m_precision);

        Vector3 const attractorPos = Vector3(mainToArea.transform_position({0, 0, 0})) * scale;

        // Attractor
        rScnRender.m_drawTransform[rPlanetDraw.attractor]
            = Matrix4::translation(attractorPos)
            * Matrix4{mainToAreaRot.toMatrix()}
            * Matrix4::scaling({500, 500, 500});

        rScnRender.m_drawTransform[rPlanetDraw.axis[0]]
            = Matrix4::translation(attractorPos)
            * Matrix4{mainToAreaRot.toMatrix()}
            * Matrix4::scaling({500000, 10, 10});
        rScnRender.m_drawTransform[rPlanetDraw.axis[1]]
            = Matrix4::translation(attractorPos)
            * Matrix4{mainToAreaRot.toMatrix()}
            * Matrix4::scaling({10, 500000, 10});
        rScnRender.m_drawTransform[rPlanetDraw.axis[2]]
            = Matrix4::translation(attractorPos)
            * Matrix4{mainToAreaRot.toMatrix()}
            * Matrix4::scaling({10, 10, 500000});

        for (std::size_t i = 0; i < rMainSpace.m_satCount; ++i)
        {
            Vector3g const relative = mainToArea.transform_position({x[i], y[i], z[i]});
            Vector3 const relativeMeters = Vector3(relative) * scale;

            Quaterniond const rot{{qx[i], qy[i], qz[i]}, qw[i]};

            DrawEnt const drawEnt = rPlanetDraw.drawEnts[i];

            rScnRender.m_drawTransform[drawEnt]
                = Matrix4::translation(relativeMeters)
                * Matrix4::scaling({200, 200, 200})
                * Matrix4{(mainToAreaRot * Quaternion{rot}).toMatrix()};
        }
    });
#endif
}); // setup_testplanets_draw



// Solar System Scenario

constexpr unsigned int c_planetCount = 5;

FeatureDef const ftrSolarSystem = feature_def("SolarSystem", [] (
        FeatureBuilder              &rFB,
        Implement<FISolarSys>       solarSys,
        DependOn<FIUniCore>         uniCore,
        DependOn<FIUniSceneFrame>   uniScnFrame)
{
        #if 0
    using CoSpaceIdVec_t = std::vector<CoSpaceId>;
    using Corrade::Containers::Array;

    auto& rUniverse = rFB.data_get< Universe >(uniCore.di.universe);

    constexpr int precision = 10;

    // Create coordinate spaces
    CoSpaceId const mainSpace = rUniverse.m_coordIds.create();
    std::vector<CoSpaceId> satSurfaceSpaces(c_planetCount);
    rUniverse.m_coordIds.create(satSurfaceSpaces.begin(), satSurfaceSpaces.end());

    rUniverse.m_coordCommon.resize(rUniverse.m_coordIds.capacity());

    CoSpaceCommon& rMainSpaceCommon = rUniverse.m_coordCommon[mainSpace];
    rMainSpaceCommon.m_satCount = c_planetCount;
    rMainSpaceCommon.m_satCapacity = c_planetCount;

    auto& rCoordNBody = rFB.data_emplace< osp::KeyedVec<CoSpaceId, CoSpaceNBody> >(solarSys.di.coordNBody);
    rCoordNBody.resize(rUniverse.m_coordIds.capacity());

    // Associate each planet satellite with their surface coordinate space
    for (SatId satId = 0; satId < c_planetCount; ++satId)
    {
        CoSpaceId const surfaceSpaceId = satSurfaceSpaces[satId];
        CoSpaceCommon& rCommon = rUniverse.m_coordCommon[surfaceSpaceId];
        rCommon.m_parent = mainSpace;
        rCommon.m_parentSat = satId;
    }

    // Coordinate space data is a single allocation partitioned to hold positions, velocities, and
    // rotations.
    // TODO: Alignment is needed for SIMD (not yet implemented). see Corrade alignedAlloc

    // Positions and velocities are arranged as XXXX... YYYY... ZZZZ...
    osp::BufferFormatBuilder fb;
    rMainSpaceCommon.m_satPositions [0] = fb.insert_block<spaceint_t>(c_planetCount);
    rMainSpaceCommon.m_satPositions [1] = fb.insert_block<spaceint_t>(c_planetCount);
    rMainSpaceCommon.m_satPositions [2] = fb.insert_block<spaceint_t>(c_planetCount);
    rMainSpaceCommon.m_satVelocities[0] = fb.insert_block<double>(c_planetCount);
    rMainSpaceCommon.m_satVelocities[1] = fb.insert_block<double>(c_planetCount);
    rMainSpaceCommon.m_satVelocities[2] = fb.insert_block<double>(c_planetCount);

    // Rotations use XYZWXYZWXYZWXYZW...
    fb.insert_interleave(c_planetCount, rMainSpaceCommon.m_satRotations[0],
                                        rMainSpaceCommon.m_satRotations[1],
                                        rMainSpaceCommon.m_satRotations[2],
                                        rMainSpaceCommon.m_satRotations[3]);

    rCoordNBody[mainSpace].mass = fb.insert_block<float>(c_planetCount);
    rCoordNBody[mainSpace].radius = fb.insert_block<float>(c_planetCount);
    rCoordNBody[mainSpace].color = fb.insert_block<Magnum::Color3>(c_planetCount);

    // Allocate data for all planets
    rMainSpaceCommon.m_data = Array<std::byte>{ Corrade::NoInit, fb.total_size() };

    std::size_t nextBody = 0;
    auto const add_body = [&rMainSpaceCommon, &nextBody, &rCoordNBody, &mainSpace] (
        Vector3l position,
        Vector3d velocity,
        Quaternion rotation,
        float mass,
        float radius,
        Magnum::Color3 color)
    {
        auto const [x, y, z] = sat_views(rMainSpaceCommon.m_satPositions, rMainSpaceCommon.m_data, c_planetCount);
        auto const [vx, vy, vz] = sat_views(rMainSpaceCommon.m_satVelocities, rMainSpaceCommon.m_data, c_planetCount);
        auto const [qx, qy, qz, qw] = sat_views(rMainSpaceCommon.m_satRotations, rMainSpaceCommon.m_data, c_planetCount);

        auto const massView = rCoordNBody[mainSpace].mass.view(arrayView(rMainSpaceCommon.m_data), c_planetCount);
        auto const radiusView = rCoordNBody[mainSpace].radius.view(arrayView(rMainSpaceCommon.m_data), c_planetCount);
        auto const colorView = rCoordNBody[mainSpace].color.view(arrayView(rMainSpaceCommon.m_data), c_planetCount);

        x[nextBody] = position.x();
        y[nextBody] = position.y();
        z[nextBody] = position.z();

        vx[nextBody] = velocity.x();
        vy[nextBody] = velocity.y();
        vz[nextBody] = velocity.z();

        qx[nextBody] = rotation.vector().x();
        qy[nextBody] = rotation.vector().y();
        qz[nextBody] = rotation.vector().z();
        qw[nextBody] = rotation.scalar();

        massView[nextBody] = mass;
        radiusView[nextBody] = radius;
        colorView[nextBody] = color;

        ++nextBody;
    };

    // Sun
    add_body(
        { 0, 0, 0 },
        { 0.0, 0.0, 0.0 },
        Quaternion::rotation(Rad{ 0.0f }, Vector3{ 1.0f, 0.0f, 0.0f }),
        1.0f * std::pow(10, 1),
        1000.0f,
        { 1.0f, 1.0f, 0.0f });

    // Blue Planet
    add_body(
        { 0, math::mul_2pow<spaceint_t, int>(10, precision), 0 },
        { 1.0, 0.0, 0.0 },
        Quaternion::rotation(Rad{ 0.0f }, Vector3{ 1.0f, 0.0f, 0.0f }),
        0.0000000001f,
        500.0f,
        { 0.0f, 0.0f, 1.0f });

    // Red Planet
    add_body(
        { 0, math::mul_2pow<spaceint_t, int>(5, precision), 0 },
        { 1.414213562, 0.0, 0.0 },
        Quaternion::rotation(Rad{ 0.0f }, Vector3{ 1.0f, 0.0f, 0.0f }),
        0.0000000001f,
        250.0f,
        { 1.0f, 0.0f, 0.0f });

    // Green Planet
    add_body(
        { 0, math::mul_2pow<spaceint_t, int>(7, precision), 0 },
        { 1.154700538, 0.0, 0.0 },
        Quaternion::rotation(Rad{ 0.0f }, Vector3{ 1.0f, 0.0f, 0.0f }),
        0.0000000001f,
        600.0f,
        { 0.0f, 1.0f, 0.0f });

    // Orange Planet
    add_body(
        { 0, math::mul_2pow<spaceint_t, int>(12, precision), 0 },
        { 0.912870929, 0.0, 0.0 },
        Quaternion::rotation(Rad{ 0.0f }, Vector3{ 1.0f, 0.0f, 0.0f }),
        0.0000000001f,
        550.0f,
        { 1.0f, 0.5f, 0.0f });

    rFB.data_emplace< CoSpaceId >       (solarSys.di.planetMainSpace, mainSpace);
    rFB.data_emplace< CoSpaceIdVec_t >  (solarSys.di.satSurfaceSpaces, std::move(satSurfaceSpaces));

    // Set initial scene frame

    auto& rScnFrame = rFB.data_get<SceneFrame>(uniScnFrame.di.scnFrame);
    rScnFrame.m_parent = mainSpace;
    rScnFrame.m_position = math::mul_2pow<Vector3g, int>({ 400, 400, 400 }, precision);

    rFB.task()
        .name       ("Update planets")
        .run_on     (uniCore.pl.update(Run))
        .sync_with  ({ uniScnFrame.pl.sceneFrame(Modify) })
        .args       ({  uniCore.di.universe,     solarSys.di.planetMainSpace, uniScnFrame.di.scnFrame,            solarSys.di.satSurfaceSpaces,     uniCore.di.deltaTimeIn,                              solarSys.di.coordNBody })
        .func       ([](Universe& rUniverse, CoSpaceId const planetMainSpace,   SceneFrame& rScnFrame, CoSpaceIdVec_t const& rSatSurfaceSpaces, float const uniDeltaTimeIn, osp::KeyedVec<CoSpaceId, CoSpaceNBody>& rCoordNBody) noexcept
    {
        CoSpaceCommon& rMainSpaceCommon = rUniverse.m_coordCommon[planetMainSpace];

        auto const scale = osp::math::mul_2pow<double, int>(1.0, -rMainSpaceCommon.m_precision);
        double const scaleDelta = uniDeltaTimeIn / scale;

        auto const [x, y, z] = sat_views(rMainSpaceCommon.m_satPositions, rMainSpaceCommon.m_data, rMainSpaceCommon.m_satCount);
        auto const [vx, vy, vz] = sat_views(rMainSpaceCommon.m_satVelocities, rMainSpaceCommon.m_data, rMainSpaceCommon.m_satCount);

        auto const massView = rCoordNBody[planetMainSpace].mass.view(arrayView(rMainSpaceCommon.m_data), c_planetCount);

        for (std::size_t i = 0; i < rMainSpaceCommon.m_satCount; ++i) {
            x[i] += vx[i] * scaleDelta;
            y[i] += vy[i] * scaleDelta;
            z[i] += vz[i] * scaleDelta;

            for (std::size_t j = 0; j < rMainSpaceCommon.m_satCount; ++j) {
                if (i == j) { continue; }

                double iMass = massView[i];
                double jMass = massView[j];

                Vector3d const iPos = Vector3d(Vector3g(x[i], y[i], z[i])) * scale;
                Vector3d const jPos = Vector3d(Vector3g(x[j], y[j], z[j])) * scale;

                double r = (jPos - iPos).length();
                Vector3d direction = (jPos - iPos).normalized();

                double forceMagnitude = (iMass * jMass) / (r * r);
                Vector3d force = direction * forceMagnitude;
                Vector3d acceleration = (force / iMass);

                vx[i] += acceleration.x() * uniDeltaTimeIn;
                vy[i] += acceleration.y() * uniDeltaTimeIn;
                vz[i] += acceleration.z() * uniDeltaTimeIn;
            }
        }
    });
#endif
}); // ftrSolarSystemPlanets



FeatureDef const ftrSolarSystemDraw = feature_def("SolarSystemDraw", [] (
        FeatureBuilder              &rFB,
        Implement<FIUniPlanetsDraw> uniPlanetsDraw,
        DependOn<FIWindowApp>       windowApp,
        DependOn<FISceneRenderer>   scnRender,
        DependOn<FICameraControl>   camCtrl,
        DependOn<FICommonScene>     comScn,
        DependOn<FIUniCore>         uniCore,
        DependOn<FIUniSceneFrame>   uniScnFrame,
        DependOn<FISolarSys>        solarSys,
        entt::any                   userData)
{
        #if 0
    auto const params = entt::any_cast<PlanetDrawParams>(userData);

    rFB.data_emplace<PlanetDraw>(uniPlanetsDraw.di.planetDraw, PlanetDraw{
        .planetMat = params.planetMat,
        .axisMat   = params.axisMat,
    });

    rFB.task()
        .name       ("Position SceneFrame center to Camera Controller target")
        .run_on     ({ windowApp.pl.inputs(Run) })
        .sync_with  ({ camCtrl.pl.camCtrl(Ready), uniScnFrame.pl.sceneFrame(Modify) })
        .args       ({              camCtrl.di.camCtrl, uniScnFrame.di.scnFrame })
        .func       ([](ACtxCameraController& rCamCtrl,   SceneFrame& rScnFrame) noexcept
    {
        if (!rCamCtrl.m_target.has_value())
        {
            return;
        }
        Vector3& rCamPl = rCamCtrl.m_target.value();

        // check origin translation
        // ADL used for Magnum::Math::sign/floor/abs
        float const maxDist = 512.0f;
        Vector3 const translate = sign(rCamPl) * floor(abs(rCamPl) / maxDist) * maxDist;

        if (!translate.isZero())
        {
            rCamCtrl.m_transform.translation() -= translate;
            rCamPl -= translate;

            // a bit janky to modify universe stuff directly here, but it works lol
            Vector3 const rotated = Quaternion(rScnFrame.m_rotation).transformVector(translate);
            rScnFrame.m_position += Vector3g(math::mul_2pow<Vector3, int>(rotated, rScnFrame.m_precision));
        }

        rScnFrame.m_scenePosition = Vector3g(math::mul_2pow<Vector3, int>(rCamCtrl.m_target.value(), rScnFrame.m_precision));
    });

    rFB.task()
        .name       ("Resync test planets, create DrawEnts")
        .run_on     ({ windowApp.pl.resync(Run) })
        .sync_with  ({ scnRender.pl.drawEntResized(ModifyOrSignal) })
        .args       ({       scnRender.di.scnRender, uniPlanetsDraw.di.planetDraw, uniCore.di.universe,     solarSys.di.planetMainSpace })
        .func       ([](ACtxSceneRender& rScnRender,      PlanetDraw& rPlanetDraw, Universe& rUniverse, CoSpaceId const planetMainSpace) noexcept
    {
        CoSpaceCommon& rMainSpace = rUniverse.m_coordCommon[planetMainSpace];

        rPlanetDraw.drawEnts.resize(rMainSpace.m_satCount, lgrn::id_null<DrawEnt>());
        rScnRender.m_drawIds.create(rPlanetDraw.drawEnts.begin(), rPlanetDraw.drawEnts.end());
    });

    rFB.task()
        .name       ("Resync test planets, add mesh and material")
        .run_on     ({ windowApp.pl.resync(Run) })
        .sync_with  ({ scnRender.pl.drawEntResized(Done), scnRender.pl.materialDirty(Modify_), scnRender.pl.meshDirty(Modify_) })
        .args       ({      comScn.di.drawing,      scnRender.di.scnRender,     comScn.di.namedMeshes, uniPlanetsDraw.di.planetDraw, uniCore.di.universe,     solarSys.di.planetMainSpace,                              solarSys.di.coordNBody })
        .func       ([](ACtxDrawing& rDrawing, ACtxSceneRender& rScnRender, NamedMeshes& rNamedMeshes,      PlanetDraw& rPlanetDraw, Universe& rUniverse, CoSpaceId const planetMainSpace, osp::KeyedVec<CoSpaceId, CoSpaceNBody>& rCoordNBody) noexcept
    {
        CoSpaceCommon& rMainSpace = rUniverse.m_coordCommon[planetMainSpace];

        Material& rMatPlanet = rScnRender.m_materials[rPlanetDraw.planetMat];

        MeshId const sphereMeshId = rNamedMeshes.m_shapeToMesh.at(EShape::Sphere);

        CoSpaceCommon& rMainSpaceCommon = rUniverse.m_coordCommon[planetMainSpace];
        auto const colorView = rCoordNBody[planetMainSpace].color.view(arrayView(rMainSpaceCommon.m_data), c_planetCount);

        for (std::size_t i = 0; i < rMainSpace.m_satCount; ++i)
        {
            DrawEnt const drawEnt = rPlanetDraw.drawEnts[i];

            rScnRender.m_mesh[drawEnt] = rDrawing.m_meshRefCounts.ref_add(sphereMeshId);
            rScnRender.m_meshDirty.push_back(drawEnt);
            rScnRender.m_visible.insert(drawEnt);
            rScnRender.m_opaque.insert(drawEnt);
            rMatPlanet.m_ents.insert(drawEnt);
            rMatPlanet.m_dirty.push_back(drawEnt);

            rScnRender.m_color[drawEnt] = colorView[i];
        }
    });

    rFB.task()
        .name       ("Reposition test planet DrawEnts")
        .run_on     ({ scnRender.pl.render(Run) })
        .sync_with  ({ scnRender.pl.drawTransforms(Modify), scnRender.pl.drawEntResized(Done), camCtrl.pl.camCtrl(Ready), uniScnFrame.pl.sceneFrame(Modify) })
        .args       ({       comScn.di.drawing,      scnRender.di.scnRender, uniPlanetsDraw.di.planetDraw, uniCore.di.universe,     uniScnFrame.di.scnFrame,     solarSys.di.planetMainSpace,                              solarSys.di.coordNBody })
        .func       ([] (ACtxDrawing& rDrawing, ACtxSceneRender& rScnRender,      PlanetDraw& rPlanetDraw, Universe& rUniverse, SceneFrame const& rScnFrame, CoSpaceId const planetMainSpace, osp::KeyedVec<CoSpaceId, CoSpaceNBody>& rCoordNBody) noexcept
    {
        CoSpaceCommon& rMainSpace = rUniverse.m_coordCommon[planetMainSpace];
        auto const [x, y, z] = sat_views(rMainSpace.m_satPositions, rMainSpace.m_data, rMainSpace.m_satCount);
        auto const [qx, qy, qz, qw] = sat_views(rMainSpace.m_satRotations, rMainSpace.m_data, rMainSpace.m_satCount);
        auto const radiusView = rCoordNBody[planetMainSpace].radius.view(arrayView(rMainSpace.m_data), c_planetCount);

        // Calculate transform from universe to area/local-space for rendering.
        // This can be generalized by finding a common ancestor within the tree
        // of coordinate spaces. Since there's only two possibilities, an if
        // statement works.
        CoordTransformer mainToArea;
        if (rScnFrame.m_parent == planetMainSpace)
        {
            mainToArea = coord_parent_to_child(rMainSpace, rScnFrame);
        }
        else
        {
            CoSpaceId const landedId = rScnFrame.m_parent;
            CoSpaceCommon& rLanded = rUniverse.m_coordCommon[landedId];

            CoSpaceTransform const landedTf = coord_get_transform(rLanded, rLanded, x, y, z, qx, qy, qz, qw);
            CoordTransformer const mainToLanded = coord_parent_to_child(rMainSpace, landedTf);
            CoordTransformer const landedToArea = coord_parent_to_child(landedTf, rScnFrame);

            mainToArea = coord_composite(landedToArea, mainToLanded);
        }
        Quaternion const mainToAreaRot{ mainToArea.rotation() };

        float const f = math::mul_2pow<float, int>(1.0f, -rMainSpace.m_precision);

        for (std::size_t i = 0; i < rMainSpace.m_satCount; ++i)
        {
            Vector3g const relative = mainToArea.transform_position({ x[i], y[i], z[i] });
            Vector3 const relativeMeters = Vector3(relative);

            Quaterniond const rot{ {qx[i], qy[i], qz[i]}, qw[i] };

            DrawEnt const drawEnt = rPlanetDraw.drawEnts[i];

            float radius = radiusView[i];

            rScnRender.m_drawTransform[drawEnt] =
                Matrix4::translation(Vector3{ (float)x[i], (float)y[i], (float)z[i] })
                * Matrix4::scaling({ radius, radius, radius })
                * Matrix4 {
                (mainToAreaRot * Quaternion{ rot }).toMatrix()
            };
        }
    });
#endif
}); // ftrSolarSystemDraw

#endif
} // namespace adera
