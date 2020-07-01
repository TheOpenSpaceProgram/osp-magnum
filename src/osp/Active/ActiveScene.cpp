#include <iostream>

#include <Magnum/GL/DefaultFramebuffer.h>

//#include "Machines/UserControl.h"
//#include "Machines/Rocket.h"
#include "DebugObject.h"

#include "ActiveScene.h"


namespace osp
{

// for the 0xrrggbb_rgbf and _deg literals
using namespace Magnum::Math::Literals;


void CompCamera::calculate_projection()
{

    m_projection = Matrix4::perspectiveProjection(
                                m_fov, m_viewport.x() / m_viewport.y(),
                                m_near, m_far);
}

void test_function()
{
    std::cout << "test function\n";
}


ActiveScene::ActiveScene(UserInputHandler& userInput) :
        m_hierarchyDirty(false),
        m_userInput(userInput),
        m_physics(*this),
        m_wire(*this)
{
    m_registry.on_construct<CompHierarchy>()
                    .connect<&ActiveScene::on_hierarchy_construct>(*this);

    m_registry.on_destroy<CompHierarchy>()
                    .connect<&ActiveScene::on_hierarchy_destruct>(*this);

    // "There is no need to store groups around for they are extremely cheap to
    //  construct, even though they can be copied without problems and reused
    //  freely. A group performs an initialization step the very first time
    //  it's requested and this could be quite costly. To avoid it, consider
    //  creating the group when no components have been assigned yet."
    m_registry.group<CompHierarchy, CompTransform>();

    // Create the root entity

    m_root = m_registry.create();
    CompHierarchy& hierarchy = m_registry.emplace<CompHierarchy>(m_root);
    hierarchy.m_name = "Root Entity";

}

ActiveScene::~ActiveScene()
{
    // FIXME: not clearing these manually causes a SIGABRT on destruction
    m_registry.clear<CompDebugObject>();
}


entt::entity ActiveScene::hier_create_child(entt::entity parent,
                                            std::string const& name)
{
    entt::entity child = m_registry.create();
    CompHierarchy& childHierarchy = m_registry.emplace<CompHierarchy>(child);
    //CompTransform& transform = m_registry.emplace<CompTransform>(ent);
    childHierarchy.m_name = name;

    CompHierarchy& parentHierarchy = m_registry.get<CompHierarchy>(parent);

    // set new child's parent
    childHierarchy.m_parent = parent;
    childHierarchy.m_level = parentHierarchy.m_level + 1;

    // If has siblings (not first child)
    if (parentHierarchy.m_childCount)
    {
        entt::entity const sibling = parentHierarchy.m_childFirst;
        CompHierarchy& siblingHierarchy
                = m_registry.get<CompHierarchy>(sibling);

        // Set new child and former first child as siblings
        siblingHierarchy.m_siblingPrev = child;
        childHierarchy.m_siblingNext = sibling;
    }

    // Set parent's first child to new child just created
    parentHierarchy.m_childFirst = child;
    parentHierarchy.m_childCount ++; // increase child count

    return child;
}

void ActiveScene::hier_set_parent_child(entt::entity parent, entt::entity child)
{
    //m_hierarchyDirty = true;
    // TODO
}


void ActiveScene::on_hierarchy_construct(entt::registry& reg, entt::entity ent)
{
    //std::cout << "hierarchy entity constructed\n";

    m_hierarchyDirty = true;
}

void ActiveScene::on_hierarchy_destruct(entt::registry& reg, entt::entity ent)
{
    std::cout << "hierarchy entity destructed\n";

}

void ActiveScene::update()
{
    // Update machine systems
    //for (AbstractSysMachine &sysMachine : m_update_sensor)
    //{
    //    sysMachine.update_sensor();
    //}

    //m_wire.update_propigate();

    //for (AbstractSysMachine &sysMachine : m_update_physics)
    //{
    //    sysMachine.update_physics(1.0f/60.0f);
    //}
    m_updateOrder.call();
}

void ActiveScene::floating_origin_translate_begin()
{

    // check if floating origin translation is requested
    m_floatingOriginInProgress = !(m_floatingOriginTranslate.isZero());
    if (m_floatingOriginInProgress)
    {
        std::cout << "Floating origin translation\n";
    }
}

void ActiveScene::update_hierarchy_transforms()
{
    // skip top level
    /*for(auto it = m_hierLevels.begin() + 1; it != m_hierLevels.end(); it ++)
    {
        for (entt::entity entity : *it)
        {

        }
    }*/
    auto group = m_registry.group<CompHierarchy, CompTransform>();

    if (m_hierarchyDirty)
    {
        // Sort by level, so that objects closer to the root are iterated first
        group.sort<CompHierarchy>([](CompHierarchy const& lhs,
                                     CompHierarchy const& rhs)
        {
            return lhs.m_level < rhs.m_level;
        }, entt::insertion_sort());
        //group.sortable();
        m_hierarchyDirty = false;
    }

    //std::cout << "size: " << group.size() << "\n";

    //bool translateAll = !(m_floatingOriginTranslate.isZero());

    for(auto entity: group)
    {
        //std::cout << "nice: " << group.get<CompHierarchy>(entity).m_name << "\n";

        CompHierarchy& hierarchy = group.get<CompHierarchy>(entity);
        CompTransform& transform = group.get<CompTransform>(entity);

        if (hierarchy.m_parent == m_root)
        {
            // top level object, parent is root

            if (m_floatingOriginInProgress && transform.m_enableFloatingOrigin)
            {
                // Do floating origin translation if enabled
                Vector3& translation = transform.m_transform[3].xyz();
                translation += m_floatingOriginTranslate;
            }

            transform.m_transformWorld = transform.m_transform;

        }
        else
        {
            CompTransform& parentTransform
                    = group.get<CompTransform>(hierarchy.m_parent);

            // set transform relative to parent
            transform.m_transformWorld = parentTransform.m_transformWorld
                                          * transform.m_transform;

        }
    }

    if (m_floatingOriginInProgress)
    {
        // everything was translated already, set back to zero
        m_floatingOriginTranslate = Vector3(0.0f, 0.0f, 0.0f);
        m_floatingOriginInProgress = false;
    }
}

void ActiveScene::floating_origin_translate(Vector3 const& amount)
{
    m_floatingOriginTranslate += amount;
    //m_floatingOriginDirty = true;
}

void ActiveScene::draw(entt::entity camera)
{
    // Start by getting camera transform inverse and projection matrix

    Matrix4 cameraProject;
    Matrix4 cameraInverse;

    {
        // TODO: check if camera has the right components
        CompCamera& cameraComp = m_registry.get<CompCamera>(camera);
        CompTransform& cameraTransform = m_registry.get<CompTransform>(camera);

        cameraProject = cameraComp.m_projection;
        cameraInverse = cameraTransform.m_transformWorld.inverted();
    }

    auto drawGroup = m_registry.group<CompDrawableDebug>(
                            entt::get<CompTransform>);

    Matrix4 entRelative;

    for(auto entity: drawGroup)
    {
        CompDrawableDebug& drawable = drawGroup.get<CompDrawableDebug>(entity);
        CompTransform& transform = drawGroup.get<CompTransform>(entity);

        entRelative = cameraInverse * transform.m_transformWorld;

        (*drawable.m_shader)
            .setDiffuseColor(drawable.m_color)
            .setAmbientColor(0x111111_rgbf)
            .setSpecularColor(0x330000_rgbf)
            .setLightPosition({10.0f, 15.0f, 5.0f})
            .setTransformationMatrix(entRelative)
            .setProjectionMatrix(cameraProject)
            .setNormalMatrix(entRelative.normalMatrix());


        drawable.m_mesh->draw(*(drawable.m_shader));

        //std::cout << "render! \n";
    }
}

AbstractSysMachine* ActiveScene::system_machine_find(const std::string &name)
{
    MapSysMachine::iterator sysIt = m_sysMachines.find(name);

    if (sysIt == m_sysMachines.end())
    {
        return nullptr;
    }
    else
    {
        return sysIt->second.get();
    }
}




}
