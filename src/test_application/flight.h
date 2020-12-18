#include "OSPMagnum.h"

#include <thread>

/**
 * Start flight scene. This will create an ActiveArea in the universe and start
 * a Magnum application with an ActiveScene. All the necessary systems will be
 * registered to this scenes for in-universe flight, such as SysAreaAssociate.
 *
 * @param magnumApp [out] Magnum application created
 * @param OSPApp [in,out] OSP universe and resources to run the application on
 * @param args [in] Arguments to pass to Magnum
 */
void test_flight(std::unique_ptr<OSPMagnum>& magnumApp,
                 osp::OSPApplication& OSPApp, OSPMagnum::Arguments args);
