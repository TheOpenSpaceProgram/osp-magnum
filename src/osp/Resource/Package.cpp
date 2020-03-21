#include "Package.h"



namespace osp
{

Package::Package(std::string const& prefix, std::string const& packageName) :
    // not sure about this but it compiles
    m_resources(),
    //m_prefix{prefix[0], prefix[1], prefix[2], prefix[3]},
    m_packageName(packageName)
{
}

}
