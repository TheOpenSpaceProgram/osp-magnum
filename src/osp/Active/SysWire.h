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
#pragma once

#include "activetypes.h"
#include "../types.h"
#include "../Resource/blueprints.h"

#include <vector>

namespace osp::active
{

namespace wiretype
{

    /**
     * A rotation in global space
     */
    struct Attitude
    {
        Quaternion m_global;
    };

    /**
     * A change in rotation. Separate pitch, yaw and roll
     */
    struct AttitudeControl
    {
        //  each (-1.0 .. 1.0)
        // pitch, yaw, roll
        Vector3 m_attitude;

        //Quaternion m_precise;
        //Quaternion m_rot;
        //Vector3 m_yawpitchroll
    };

    /**
     * A change in rotation in axis angle
     *
     */
    //struct AttitudeControlPrecise
    //{
    //    //Quaternion m_precise;
    //    Vector3 m_axis;
    //};

    /**
     * For something like throttle
     */
    struct Percent
    {
        float m_value;
    };

    enum class DeployOp : std::uint8_t { NONE, ON, OFF, TOGGLE };

    /**
     * Used to turn things on and off or ignite stuff
     */
    struct Deploy
    {
        //int m_stage;
        //bool m_on, m_off, m_toggle;
        DeployOp m_op;
    };

    /**
     * your typical logic gate boi
     */
    struct Logic
    {
        bool m_value;
    };

    /**
     * Pipe, generically exposes the output entity to the input one
     */
    struct Pipe
    {
        ActiveEnt m_source;
    };

} // namespace wiretype


//-----------------------------------------------------------------------------

/**
 * Stores the wire value, and connects to machine's ACompWirePanel
 */
template<typename VALUE_T>
struct WireNode
{
    struct ConnectToPanel
    {
        ActiveEnt m_entity; // ACompWirePanel<VALUE_T>
        uint32_t m_index;
        //typename VALUE_T::LinkType m_linktype;
    };

    std::vector<ConnectToPanel> m_connections;

    VALUE_T m_value;
};

/**
 * Connects machines to WireNodes
 */
template<typename VALUE_T>
struct ACompWirePanel
{
    struct ConnectToNode
    {
        uint32_t m_index;
        typename VALUE_T::LinkType m_linktype;
    };

    // Connect to nodes
    std::vector<ConnectToNode> m_connections;
};

class SysWire
{
public:


};

} // namespace osp::active
