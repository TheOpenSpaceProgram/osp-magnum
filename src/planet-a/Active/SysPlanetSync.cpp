#include "SysPlanetSync.h"

#include "SysPlanetA.h"

#include "../Satellites/SatPlanet.h"

#include <osp/Active/ActiveScene.h>
#include <osp/Active/SysHierarchy.h>
#include <osp/Active/SysAreaAssociate.h>

#include <osp/Active/custom_mesh.h>

#include <osp/logging.h>

using namespace planeta::active;

using planeta::universe::UCompPlanet;

using osp::universe::Universe;
using osp::universe::Satellite;

using osp::active::ActiveScene;
using osp::active::ActiveEnt;

using osp::active::ACompActivatedSat;
using osp::active::ACompAreaLink;
using osp::active::ACompFloatingOrigin;
using osp::active::ACompTransform;
using osp::active::SysAreaAssociate;
using osp::active::SysHierarchy;

using osp::Vector3;
using osp::Matrix4;

ActiveEnt SysPlanetSync::activate(
        ActiveScene &rScene, Universe &rUni,
        Satellite areaSat, Satellite tgtSat)
{

    OSP_LOG_INFO("Activating a planet");

    auto &loadMePlanet = rUni.get_reg().get<universe::UCompPlanet>(tgtSat);

    // Convert position of the satellite to position in scene
    Vector3 positionInScene = rUni.sat_calc_pos_meters(areaSat, tgtSat).value();

    // Create planet entity and add components to it

    ActiveEnt root = rScene.hier_get_root();
    ActiveEnt planetEnt = SysHierarchy::create_child(rScene, root, "Planet");

    auto &rPlanetTransform = rScene.get_registry()
                             .emplace<ACompTransform>(planetEnt);
    rPlanetTransform.m_transform = Matrix4::translation(positionInScene);
    rScene.reg_emplace<ACompFloatingOrigin>(planetEnt);
    rScene.reg_emplace<ACompActivatedSat>(planetEnt, tgtSat);

    rScene.reg_emplace<ACompPlanetSurface>(
                planetEnt, ACompPlanetSurface{loadMePlanet.m_radius});

    using osp::active::ACtxCustomMeshes;
    using osp::active::ACompCustomMesh;
    using Magnum::Trade::MeshData;

    //auto &rCustomMeshs = rScene.get_registry().ctx<ACtxCustomMeshes>();

    //auto &rTerrain = rScene.reg_emplace<ACompTriTerrain>(planetEnt);
    //auto &rTerrainMesh = rScene.reg_emplace<ACompTriTerrainMesh>(planetEnt);

    //rScene.reg_emplace<ACompCustomMesh>(
    //        planetEnt,
    //        ACompCustomMesh{rCustomMeshs.emplace(std::move(data))});

    return planetEnt;
}


void SysPlanetSync::update_activate(ActiveScene& rScene)
{
    ACompAreaLink *pLink = SysAreaAssociate::try_get_area_link(rScene);

    if (pLink == nullptr)
    {
        return;
    }

    Universe &rUni = pLink->get_universe();
    auto &rSync = rScene.get_registry().ctx<ACtxSyncPlanets>();

    // Delete planets that have exited the ActiveArea
    for (Satellite sat : pLink->m_leave)
    {
        if ( ! rUni.get_reg().all_of<UCompPlanet>(sat))
        {
            continue;
        }

        auto foundEnt = rSync.m_inArea.find(sat);
        SysHierarchy::mark_delete_cut(rScene, foundEnt->second);
        rSync.m_inArea.erase(foundEnt);
    }

    // Activate planets that have just entered the ActiveArea
    for (Satellite sat : pLink->m_enter)
    {
        if ( ! rUni.get_reg().all_of<UCompPlanet>(sat))
        {
            continue;
        }

        ActiveEnt ent = activate(rScene, rUni, pLink->m_areaSat, sat);
        rSync.m_inArea[sat] = ent;
    }
}
