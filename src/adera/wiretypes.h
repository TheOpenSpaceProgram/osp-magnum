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

#include <osp/Active/SysSignal.h>

namespace adera::wire
{
    /**
     * Rotation command with pitch, yaw and roll components
     */
    struct AttitudeControl : public osp::active::Signal<AttitudeControl>
    {
        static inline std::string smc_wire_name = "AttitudeControl";

        AttitudeControl() = default;
        constexpr AttitudeControl(osp::Vector3 value) noexcept : m_attitude(value) { }

        //  each (-1.0 .. 1.0)
        // pitch, yaw, roll
        osp::Vector3 m_attitude;

        bool operator==(AttitudeControl const& rhs) const
        {
            return m_attitude == rhs.m_attitude;
        }

        bool operator!=(AttitudeControl const& rhs) const
        {
            return m_attitude != rhs.m_attitude;
        }
    };

    /**
     * A percentage for something like throttle
     */
    struct Percent : public osp::active::Signal<Percent>
    {
        static inline std::string smc_wire_name = "Percent";

        Percent() = default;
        constexpr Percent(float value) noexcept : m_percent(value) { }

        float m_percent;

        constexpr bool operator==(Percent const& rhs) const
        {
            return m_percent == rhs.m_percent;
        }

        constexpr bool operator!=(Percent const& rhs) const
        {
            return m_percent != rhs.m_percent;
        }
    };

    /**
     * Boolean signal for logic gates
     */
    struct Logic : public osp::active::Signal<Logic>
    {
        bool m_value;
    };

} // namespace wiretype


