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

#include "Package.h"

#include <map>

namespace osp
{

// Prefix for resource paths to indicate which package they're from
// Ideally, this would be some kind of short string
using ResPrefix_t = std::string;

/**
 * @brief Stores Packages identifiable through a short prefix
 *
 * PackageRegistry is intended to store resources accessible throughout the
 * entire application, organized by Package.
 */
class PackageRegistry
{    
public:

    using Map_t = std::map<ResPrefix_t, Package, std::less<>>;

    /**
     * @brief Create a new resource package
     *
     * @param prefix [in] Prefix of package to add
     *
     * @return newly created Package
     */
    Package& create(ResPrefix_t const prefix);

    /**
     * @brief Get a resource package by prefix name
     *
     * @param prefix [in] The short prefix name of the package
     * @return The resource package
     */
    Package& find(std::string_view const prefix);

    /**
     * @return Read-only internal map
     */
    constexpr Map_t const& get_map() const noexcept { return m_packages; };

    /**
     * @return Number of registered packages
     */
    size_t count() const noexcept { return m_packages.size(); }

private:
    Map_t m_packages;

};
}
