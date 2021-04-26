/**
 * Open Space Program
 * Copyright © 2019-2020 Open Space Program Project
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
#include <iostream>

#include "SysWire.h"
#include "ActiveScene.h"

using osp::active::SysWire;

void SysWire::add_functions(ActiveScene& rScene)
{
    rScene.debug_update_add(rScene.get_update_order(), "wire", "vehicle_modification", "physics",
                            &SysWire::update_wire);
}

void SysWire::setup_default(
        ActiveScene& rScene,
        std::vector<ACompWire::update_func_t> updCalculate,
        std::vector<ACompWire::update_func_t> updPropagate)
{
    rScene.reg_emplace<ACompWire>(rScene.hier_get_root(),
                                  ACompWire{std::move(updCalculate),
                                            std::move(updPropagate)});
}

void SysWire::update_wire(ActiveScene &rScene)
{
    auto view = rScene.get_registry().view<ACompWireNeedUpdate>();

    ActiveReg_t::poly_storage t = rScene.get_registry().storage(entt::type_id<ACompWireNeedUpdate>());


    auto const& wire = rScene.reg_get<ACompWire>(rScene.hier_get_root());

    int updateLimit = 16;

    while (!view.empty())
    {
        for (ACompWire::update_func_t update : wire.m_updCalculate)
        {
            update(rScene);
        }

        for (ACompWire::update_func_t update : wire.m_updPropagate)
        {
            update(rScene);
        }

        updateLimit --;
        if (0 == updateLimit)
        {
            SPDLOG_LOGGER_INFO(rScene.get_application().get_logger(),
                               "Wire update limit reached");
            break;
        }
    }
}
