#include "SysAreaAssociate.h"

#include "ActiveScene.h"


using namespace osp::active;

SysAreaAssociate::SysAreaAssociate(ActiveScene &rScene) :
       m_scene(rScene),
       m_updateScan(rScene.get_update_order(), "areascan", "", "",
                    std::bind(&SysAreaAssociate::update_scan, this))
{

}

void SysAreaAssociate::update_scan()
{
    std::cout << "mothmoth\n";
}

void SysAreaAssociate::connect(universe::Satellite sat)
{
    using namespace osp::universe;

    //auto &reg = m_scene.get_registry();

    //ucomp::ActiveArea &areaAA = reg.get<>

    m_areaSat = sat;
}

void SysAreaAssociate::activate_func_add(universe::ITypeSatellite* type,
                                         LoadStrategy function)
{
    m_loadFunctions[type] = function;
}



