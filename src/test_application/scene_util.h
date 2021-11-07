/**
 * Open Space Program
 * Copyright © 2019-2021 Open Space Program Project
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

#include <osp/scene.h>

#include <unordered_map>

namespace testapp
{

/**
 * @brief Maps arbitrary types in an osp::Scene to unique but convenient to
 *        access concrete types
 */
struct SceneMeta
{
    using SceneDataId = osp::SceneDataId;

    template<typename TYPE_T, typename ... ARGS_T>
    void set(osp::Scene &rScene, ARGS_T ... args)
    {
        SceneDataId id = rScene.emplace<TYPE_T>(std::forward(args)...);

        auto const& [it, success] = m_map.emplace(
                entt::type_seq<TYPE_T>().value(), id);
        if ( ! success)
        {
            throw std::runtime_error("Type already exists");
        }
    }

    template<typename TYPE_T>
    TYPE_T& get(osp::Scene &rScene)
    {
        return rScene.get<TYPE_T>(m_map.at(entt::type_seq<TYPE_T>().value()));
    }

    std::unordered_map<entt::id_type, SceneDataId> m_map;
};


void setup_scene_active(osp::Scene& rScene, SceneMeta& rMeta);

void setup_scene_physics(osp::Scene& rScene, SceneMeta& rMeta);

void setup_scene_drawable(osp::Scene& rScene, SceneMeta& rMeta);

//void setup_scene_opengl(osp::Scene& rScene, SceneMeta& rMeta);

void setup_scene_vehicles(osp::Scene& rScene, SceneMeta& rMeta);

void setup_scene_machines(osp::Scene& rScene, SceneMeta& rMeta);

void setup_scene_flight(osp::Scene& rScene, SceneMeta& rMeta);

}
