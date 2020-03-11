#include <iostream>
#include <thread>

#include "OSPMagnum.h"
#include "osp/Universe.h"
#include "osp/Satellites/SatActiveArea.h"
#include "osp/Resource/SturdyImporter.h"

void magnum_application();

int debug_cli_loop();
void cli_print_sats();

Magnum::Platform::Application::Arguments g_args();
std::thread g_magnumThread;

osp::Universe g_universe;
osp::OSPMagnum *g_ospMagnum;

int g_argc;
char** g_argv;

int main(int argc, char** argv)
{   
    // eventually do more important things here
    g_argc = argc;
    g_argv = argv;

    return debug_cli_loop();
}

/**
 * Starts a magnum application, an active area, and links them together
 */
void magnum_application()
{

    // only call load once
    static bool s_partsLoaded = false;
    if (!s_partsLoaded)
    {
        // Create a new package
        osp::Package lazyDebugPack({'l','z','d','b'}, "lazy-debug");
        //m_packages.push_back(std::move(p));

        // Create a sturdy
        osp::SturdyImporter importer;
        importer.open_filepath("OSPData/adera/spamcan.sturdy.gltf");

        importer.load_config(lazyDebugPack);

        g_universe.debug_get_packges().push_back(std::move(lazyDebugPack));

        //importer.load_config();
        //std::vector<osp::PartPrototype> prototypesToLoadVerySoonIThink;
        //importer.obtain_parts(prototypesToLoadVerySoonIThink);

        //g_universe.add_parts(prototypesToLoadVerySoonIThink);

        s_partsLoaded = true;
    }

    osp::Satellite& sat = g_universe.create_sat();
    osp::SatActiveArea& area = sat.create_object<osp::SatActiveArea>();

    osp::OSPMagnum app({g_argc, g_argv});
    app.set_active_area(area);

    app.exec();

    std::cout << "Application closed\n";
    
    // workaround: wipe mesh resources because they're specific to the
    // opengl context
    static_cast<std::vector<osp::Resource<Magnum::GL::Mesh> >& >(
                g_universe.debug_get_packges()[0].debug_get_resource_table())
                    .clear();
    
}


/**
 * A basic spaghetti command line interface that gets inputs from stdin to
 * mess with g_universe and g_ospMagnum. Replace this entire file eventually.
 * @return An error code maybe
 */
int debug_cli_loop()
{

    std::cout
        << "OSP-Magnum Temporary Debug CLI\n"
        << "Things to type:\n"
        << "* ulist     - List Satellites in the universe\n"
        << "* start     - Create an ActiveArea and start Magnum\n"
        << "* exit      - Deallocate everything and return memory to OS\n";

    std::string command;

    while(true)
    {
        std::cout << "> ";
        std::cin >> command;

        if (command == "ulist")
        {
            cli_print_sats();
        }
        else if (command == "start")
        {
            if (g_magnumThread.joinable())
            {
                g_magnumThread.join();
            }
            std::thread t(magnum_application);
            g_magnumThread.swap(t);
        }
        else if (command == "exit")
        {
            return 0;
        }
        else
        {
            std::cout << "that doesn't do anything ._.\n";
        }

    }
}


void cli_print_sats()
{
    const std::vector<osp::Satellite>& sats = g_universe.get_sats();

    // Loop through g_universe's satellites and print them.
    std::cout << "Universe:\n";
    for (const osp::Satellite& sat : sats)
    {
        std::cout << "* " << sat.get_name() << "\n";
    }

}
