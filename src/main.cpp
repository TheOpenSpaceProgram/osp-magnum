#include <iostream>
#include <thread>
#include <random>


#include "OSPMagnum.h"
#include "osp/Universe.h"
#include "osp/Satellites/SatActiveArea.h"
#include "osp/Satellites/SatVehicle.h"
#include "osp/Resource/SturdyImporter.h"
#include "osp/Resource/Package.h"


void magnum_application();

int debug_cli_loop();
void debug_add_random_vehicle();
void debug_print_sats();

//bool g_partsLoaded = false;

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
    //static bool s_partsLoaded = false;
    if (!g_universe.debug_get_packges().size())
    {
        // Create a new package
        osp::Package lazyDebugPack("lzdb", "lazy-debug");
        //m_packages.push_back(std::move(p));

        // Create a sturdy
        osp::SturdyImporter importer;
        importer.open_filepath("OSPData/adera/spamcan.sturdy.gltf");

        // load the sturdy into the package
        importer.load_config(lazyDebugPack);

        // Add package to the univere
        g_universe.debug_get_packges().push_back(std::move(lazyDebugPack));

        // Add a vehicle so there's something to load
        debug_add_random_vehicle();

        //s_partsLoaded = true;
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
            debug_print_sats();
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
            // delete the universe
            g_universe.get_sats().clear();
            return 0;
        }
        else
        {
            std::cout << "that doesn't do anything ._.\n";
        }

    }
}

void debug_add_random_vehicle()
{
    osp::Satellite& sat = g_universe.create_sat();
    osp::SatVehicle& vehicle = sat.create_object<osp::SatVehicle>();

    // Part to add
    osp::Resource<osp::PartPrototype>* victim =
            g_universe.debug_get_packges()[0]
            .get_resource<osp::PartPrototype>(0);

    for (int i = 0; i < 20; i ++)
    {
        // Generate random vector
        Vector3 randomvec(std::rand() % 64 - 32,
                          std::rand() % 64 - 32,
                          std::rand() % 64 - 32);

        randomvec /= 32.0f;

        // Add a new [victim] part
        vehicle.get_blueprint().debug_add_part(*victim, randomvec,
                                               Quaternion(), Vector3(1, 1, 1));
        //std::cout << "random: " <<  << "\n";
    }


}

void debug_print_sats()
{
    const std::vector<osp::Satellite>& sats = g_universe.get_sats();

    // Loop through g_universe's satellites and print them.
    std::cout << "Universe:\n";
    for (const osp::Satellite& sat : sats)
    {
        std::cout << "* " << sat.get_name() << "\n";
    }

}
