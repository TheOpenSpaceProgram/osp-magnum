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

using namespace osp;
using namespace osp::active;


const std::string SysWire::smc_name = "Wire";

WireData* WireInput::connected_value()
{
    WireOutput* woConnected = list();
    if (woConnected)
    {
        return &(woConnected->value());
    }
    else
    {
        return nullptr;
    }
}

void WireInput::doErase()
{
     list()->cut(this);
};

SysWire::SysWire(ActiveScene &scene)
 : m_updateWire{scene.get_update_order().add("wire", "", "",
                [this] (ActiveScene& rScene) { this->update_propagate(rScene); })}
{

}

void SysWire::update_propagate(ActiveScene& rScene)
{
    for (DependentOutput &output : m_dependentOutputs)
    {
        output.m_element->propagate_output(output.m_output);
    }
}

void SysWire::connect(WireOutput &wireFrom, WireInput &wireTo)
{
    std::cout << "Connected " << wireFrom.get_name() << " to "
              << wireTo.get_name() << "\n";
    wireFrom.insert(&wireTo);
    // TODO: check for dependent outputs and add to list and sort
}
