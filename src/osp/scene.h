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

#include <entt/core/any.hpp>

namespace osp
{

    enum class SceneDataId : uint32_t { };

    class Scene
    {

    public:

        template<typename TYPE_T>
        TYPE_T& get(SceneDataId id)
        {
            TYPE_T* pData = entt::any_cast<TYPE_T>(&m_data.at(size_t(id)));

            if (pData == nullptr)
            {
                throw std::runtime_error("Incorrect Scene data cast");
            }

            return *pData;
        }

        template<typename TYPE_T, typename ... ARGS_T>
        SceneDataId emplace(ARGS_T&& ... args)
        {
            size_t pos = m_data.size();
            m_data.emplace_back(std::in_place_type<TYPE_T>,
                                std::forward(args) ...);
            return SceneDataId(pos);
        }

    private:
        std::vector<entt::any> m_data;


    };
}
