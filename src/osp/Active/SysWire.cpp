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

void SysWire::setup_default(
        ActiveScene& rScene,
        uint32_t machineTypeCount,
        std::vector<ACompWire::updfunc_t> updCalculate,
        std::vector<ACompWire::updfunc_t> updNodes)
{
    rScene.reg_emplace<ACompWire>(
            rScene.hier_get_root(),
            ACompWire{std::move(updCalculate),
                      std::move(updNodes),
                      std::vector<std::vector<ActiveEnt>>(machineTypeCount),
                      std::vector<std::mutex>(machineTypeCount)});
}

void SysWire::update_wire(ActiveScene &rScene)
{
    auto &wire = rScene.reg_get<ACompWire>(rScene.hier_get_root());

    int updateLimit = 16;

    while (wire.m_updateRequest)
    {
        wire.m_updateRequest = false;

        // Update all Nodes
        for (ACompWire::updfunc_t update : wire.m_updNodes)
        {
            update(rScene);
        }

        // Perform Calculation update for all Machines
        for (ACompWire::updfunc_t update : wire.m_updCalculate)
        {
            update(rScene);
        }

        // Clear update queues
        for (std::vector<ActiveEnt> &toUpdate : wire.m_entToCalculate)
        {
            toUpdate.clear();
        }

        // Stop updating if limit is reached
        updateLimit --;
        if (0 == updateLimit)
        {
            SPDLOG_LOGGER_INFO(rScene.get_application().get_logger(),
                               "Wire update limit reached");
            break;
        }
    }
}
