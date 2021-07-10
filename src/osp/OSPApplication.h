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

#include <map>
#include <spdlog/spdlog.h>

#include "Universe.h"
#include "Resource/Package.h"

namespace osp
{


class OSPApplication
{
public:

    // put more stuff into here eventually

    OSPApplication();

    /**
     * Add a resource package to the application
     *
     * The package should be populated externally, then passed via rvalue
     * reference so the contents can be moved into the application resources
     *
     * @param p [in] The package to add
     */
    void debug_add_package(Package&& p);

    /**
     * Get a resource package by prefix name
     *
     * @param prefix [in] The short prefix name of the package
     * @return The resource package
     */
    Package& debug_find_package(std::string_view prefix);

    size_t debug_num_packages() const { return m_packages.size(); }

    universe::Universe& get_universe() { return m_universe; }

    void set_universe_update(std::function<void(universe::Universe&)> func)
    {
        m_universeUpdate = func;
    }

    void update_universe() { m_universeUpdate(m_universe); };

    const std::shared_ptr<spdlog::logger>& get_logger() { return m_logger; };

private:
    std::map<ResPrefix_t, Package, std::less<>> m_packages;

    universe::Universe m_universe;
    std::function<void(universe::Universe&)> m_universeUpdate;

    const std::shared_ptr<spdlog::logger> m_logger;
};
}
