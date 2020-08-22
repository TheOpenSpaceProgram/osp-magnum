#pragma once

#include "Universe.h"
#include "Resource/Package.h"

namespace osp
{


class OSPApplication
{
public:

    // put more stuff into here eventually

    OSPApplication();

    std::vector<Package>& debug_get_packges() { return m_packages; };

    universe::Universe& get_universe() { return m_universe; }

private:
    std::vector<Package> m_packages;
    universe::Universe m_universe;
};

}
