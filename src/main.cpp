#include <iostream>
#include <memory>
#include <thread>
#include <random>


#include "OSPMagnum.h"
#include "osp/Universe.h"
#include "osp/Satellites/SatActiveArea.h"
#include "osp/Satellites/SatVehicle.h"
#include "osp/Satellites/SatPlanet.h"
#include "osp/Resource/SturdyImporter.h"
#include "osp/Resource/Package.h"



void magnum_application();
void load_a_bunch_of_stuff();

int debug_cli_loop();
osp::Satellite& debug_add_random_vehicle();
void debug_print_sats();

//bool g_partsLoaded = false;

Magnum::Platform::Application::Arguments g_args();
std::thread g_magnumThread;

osp::Universe g_universe;
std::unique_ptr<osp::OSPMagnum> g_ospMagnum;

int g_argc;
char** g_argv;

int main(int argc, char** argv)
{   
    // eventually do more important things here.
    // just lazily save the arguments
    g_argc = argc;
    g_argv = argv;

    // start doing debug cli loop
    return debug_cli_loop();
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
            if (g_ospMagnum)
            {
                // request exit if application exists
                g_ospMagnum->exit();
            }
            break;
        }
        else
        {
            std::cout << "that doesn't do anything ._.\n";
        }

    }

    // wait for magnum thread to exit if it exists
    if (g_magnumThread.joinable())
    {
        g_magnumThread.join();
    }

    // destory the universe
    g_universe.get_sats().clear();
    return 0;
}

/**
 * Starts a magnum application, an active area, and links them together
 */
void magnum_application()
{
    // Create the application
    g_ospMagnum = std::make_unique<osp::OSPMagnum>(
                osp::OSPMagnum::Arguments{g_argc, g_argv});

    // Configure Controls

    using Key = osp::OSPMagnum::KeyEvent::Key;
    using TermOp = osp::ButtonTermConfig::TermOperator;
    using TermTrig = osp::ButtonTermConfig::TermTrigger;
    osp::UserInputHandler& userInput = g_ospMagnum->get_input_handler();

    // note: names like "game_thr_max" are arbitrary

    // Set throttle max to Z
    userInput.config_register_control("game_thr_max",
            {{0, (int) Key::Z, TermTrig::PRESSED, false, TermOp::OR}});
    // Set throttle min to X
    userInput.config_register_control("game_thr_min",
            {{0, (int) Key::X, TermTrig::PRESSED, false, TermOp::OR}});
    // Set self destruct to LeftCtrl+C or LeftShift+A
    userInput.config_register_control("game_self_destruct",
            {{0, (int) Key::LeftCtrl, TermTrig::HOLD, false, TermOp::AND},
             {0, (int) Key::C, TermTrig::PRESSED, false, TermOp::OR},
             {0, (int) Key::LeftShift, TermTrig::HOLD, false, TermOp::AND},
             {0, (int) Key::A, TermTrig::PRESSED, false, TermOp::OR}});

    // Set generic Up/down/left/right to WASD
    userInput.config_register_control("c_up",
            {{0, (int) Key::W, TermTrig::PRESSED, false, TermOp::OR}});
    userInput.config_register_control("c_dn",
            {{0, (int) Key::S, TermTrig::PRESSED, false, TermOp::OR}});
    userInput.config_register_control("c_lf",
            {{0, (int) Key::A, TermTrig::PRESSED, false, TermOp::OR}});
    userInput.config_register_control("c_rt",
            {{0, (int) Key::D, TermTrig::PRESSED, false, TermOp::OR}});

    // only call load once, since some stuff might already be loaded
    if (!g_universe.debug_get_packges().size())
    {
        load_a_bunch_of_stuff();
    }

    // create a satellite with an ActiveArea
    osp::Satellite& sat = g_universe.sat_create();
    osp::SatActiveArea& area = sat.create_object<osp::SatActiveArea>(
                                    g_ospMagnum->get_input_handler());
    sat.set_position({Vector3s(0, 0, 0), 10});

    // make the application switch to that area
    g_ospMagnum->set_active_area(area);

    // sat reference becomes invalid btw, since it refers to a vector elem.

    // this starts the game loop. non-asynchronous
    // OSPMagnum::drawEvent gets looped
    g_ospMagnum->exec();

    // Close button has been pressed

    std::cout << "Magnum Application closed\n";

    // Kill the active area
    g_universe.sat_remove(area.get_satellite());

    // workaround: wipe mesh resources because they're specific to the
    // opengl context
    static_cast<std::vector<osp::Resource<Magnum::GL::Mesh> >& >(
                g_universe.debug_get_packges()[0].debug_get_resource_table())
                    .clear();
    
    // destruct the application, this closes the window
    g_ospMagnum.reset();
}

/**
 * As the name implies.
 *
 * prefer not to use names like this anywhere else but main.cpp
 */
void load_a_bunch_of_stuff()
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

    // Add 5000 vehicles so there's something to load
    for (int i = 0; i < 5000; i ++)
    {
        // Creates a random mess of spamcans
        osp::Satellite& sat = debug_add_random_vehicle();

        // Put them in a long line along
        Vector3sp randomvec(Magnum::Math::Vector3<SpaceInt>(
                                (i / 2) * 1024 * 5, 0,  0), 10);

        sat.set_position(randomvec);

    }


    // Add a planet too
    osp::Satellite& planet = g_universe.sat_create();
    planet.create_object<osp::SatPlanet>();

    //s_partsLoaded = true;
}

osp::Satellite& debug_add_random_vehicle()
{
    // Get some needed variables
    osp::Satellite &sat = g_universe.sat_create();
    osp::SatVehicle &vehicle = sat.create_object<osp::SatVehicle>();
    osp::Resource<osp::BlueprintVehicle> blueprint;

    // Part to add, very likely a spamcan
    osp::Resource<osp::PrototypePart>* victim =
            g_universe.debug_get_packges()[0]
            .get_resource<osp::PrototypePart>(0);

    // Add 10 parts
    for (int i = 0; i < 10; i ++)
    {
        // Generate random vector
        Vector3 randomvec(std::rand() % 64 - 32,
                          std::rand() % 64 - 32,
                          std::rand() % 64 - 32);

        randomvec /= 32.0f;

        // Add a new [victim] part
        blueprint.m_data.add_part(*victim, randomvec,
                           Quaternion(), Vector3(1, 1, 1));
        //std::cout << "random: " <<  << "\n";
    }

    // Wire up throttle control
    // from (output): a MachineUserControl m_woThrottle
    // to    (input): a MachineRocket m_wiThrottle
    blueprint.m_data.add_wire(0, 0, 0,
                              0, 1, 1);

    // put blueprint in package
    auto blueprintRes = g_universe.debug_get_packges()[0]
            .debug_add_resource<osp::BlueprintVehicle>(std::move(blueprint));

    // set the SatVehicle's blueprint to the one just made
    vehicle.get_blueprint_depend().bind(*blueprintRes);

    return sat;

}

void debug_print_sats()
{
    std::vector<osp::Satellite>& sats = g_universe.get_sats();

    // Loop through g_universe's satellites and print them.
    std::cout << "Universe:\n";
    for (osp::Satellite& sat : sats)
    {
        Vector3sp pos = sat.get_position();
        std::cout << "* " << sat.get_name() << "["
                  << pos.x() << ", " << pos.y() << ", " << pos.z() << "] ("
                  << sat.get_object()->get_id().m_name << ")\n";
    }

}
