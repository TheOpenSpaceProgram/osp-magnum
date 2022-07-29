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

#include <osp/Active/drawing.h>
#include <osp/Active/physics.h>

#include <longeron/id_management/registry_stl.hpp>

#include <entt/container/dense_map.hpp>

#include <string_view>

namespace testapp::scenes
{

/**
 * @brief Data needed to support physics in any scene
 *
 * Specific physics engine used (ake Newton world) is stored separately in
 * CommonScene. ie: rScene.get<ospnewton::ACtxNwtWorld>();
 */
struct ACtxTestPhys
{
    // Generic physics components and data
    osp::active::ACtxPhysics        m_physics;
    osp::active::ACtxHierBody       m_hierBody;

    // 'Per-thread' inputs fed into the physics engine. Only one here for now
    osp::active::ACtxPhysInputs     m_physIn;
};

struct NamedMeshes
{
    // Required for std::is_copy_assignable to work properly inside of entt::any
    NamedMeshes() = default;
    NamedMeshes(NamedMeshes const& copy) = delete;
    NamedMeshes(NamedMeshes&& move) = default;

    entt::dense_map<osp::phys::EShape,
                    osp::active::MeshIdOwner_t> m_shapeToMesh;
    entt::dense_map<std::string_view,
                    osp::active::MeshIdOwner_t> m_namedMeshs;
};

struct QuickPhysSceneRef
{
    lgrn::IdRegistryStl<osp::active::ActiveEnt> &m_rActiveIds;
    osp::active::ACtxBasic      &m_rBasic;
    ACtxTestPhys                &m_rTPhys;
    NamedMeshes const           &m_rNMesh;
    osp::active::ACtxDrawing    &m_rDrawing;
};

/**
 * @brief Convenient function to add a drawable and collidable primitive shape
 *
 * @return Newly created entity
 */
osp::active::ActiveEnt add_solid_quick(
        QuickPhysSceneRef               rScene,
        osp::active::ActiveEnt const    parent,
        osp::phys::EShape const         shape,
        osp::Matrix4 const              transform,
        int const                       material,
        float const                     mass);

/**
 * @brief Convenient function to create and throw a drawable physics entity of
 *        a single primative shape
 *
 * @return Newly created entity
 */
osp::active::ActiveEnt add_rigid_body_quick(
        QuickPhysSceneRef               rScene,
        osp::active::ActiveEnt const    parent,
        osp::Vector3 const              position,
        osp::Vector3 const              velocity,
        int const                       material,
        float const                     mass,
        osp::phys::EShape const         shape,
        osp::Vector3 const              size);


} // namespace testapp::scenes
