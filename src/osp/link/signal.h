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

#include "machines.h"

#include <Corrade/Containers/ArrayView.h>

namespace osp::link
{

constexpr JuncCustom gc_sigIn  = 0;
constexpr JuncCustom gc_sigOut = 1;

template <typename VALUE_T>
using SignalValues_t = std::vector<VALUE_T>;

template <typename VALUE_T>
struct UpdateNodes
{
    BitVector_t                 m_nodeDirty;
    SignalValues_t<VALUE_T>     m_nodeNewValues;

    void assign(NodeId node, VALUE_T&& value)
    {
        m_nodeDirty.set(node);
        m_nodeNewValues[node] = std::forward<VALUE_T>(value);
    }
};

template <typename VALUE_T, typename RANGE_T>
bool update_signal_nodes(
        RANGE_T const&                                  toUpdate,
        Nodes::NodeToMach_t const&                      nodeToMach,
        Machines const&                                 machines,
        Corrade::Containers::ArrayView<VALUE_T const>   newValues,
        Corrade::Containers::ArrayView<VALUE_T>         currentValues,
        UpdMachPerType_t&                               rUpdMach)
{
    bool somethingNotified = false;

    for (uint32_t const node : toUpdate)
    {
        // Apply node value changes
        currentValues[node] = newValues[node];

        // Notify connected inputs
        for (Junction junc : nodeToMach[node])
        {
            if (junc.m_custom == gc_sigIn)
            {
                somethingNotified = true;
                rUpdMach[junc.m_type].m_localDirty.set(junc.m_local);
            }
        }
    }

    return somethingNotified;
}

} // namespace osp::wire
