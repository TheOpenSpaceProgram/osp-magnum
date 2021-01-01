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
#include "OSPApplication.h"

#include "osp/Resource/blueprints.h"

using namespace osp;

OSPApplication::OSPApplication()
{

}

void OSPApplication::debug_add_package(Package&& p)
{
    auto const& [it, success] = m_packages.emplace(p.get_prefix(), std::move(p));
    assert(success);
}

Package& OSPApplication::debug_find_package(std::string_view prefix)
{
    if (auto it = m_packages.find(prefix); it != m_packages.end())
    {
        return it->second;
    }
    throw std::out_of_range("Package not found");
}

void OSPApplication::shutdown()
{
    m_glResources.clear_all();
    m_universe.destroy();

    /* TODO HACK:
       BlueprintVehicle resources themselves store DependRes<PrototypePart>,
       whose group is destructed before BlueprintVehicle. In order to prevent
       dereferencing of invalid pointers, the BlueprintVehicle group must be
       manually cleared before the packages are destroyed.
    */
    Package& pkg = debug_find_package("lzdb");
    pkg.clear<BlueprintVehicle>();

    m_packages.clear();
}
