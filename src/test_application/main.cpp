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

#include "OSPMagnum.h"
#include "flight.h"

#include "universes/simple.h"

#include <osp/Resource/AssetImporter.h>

#include <osp/Satellites/SatVehicle.h>

#include <planet-a/Satellites/SatPlanet.h>

#include <iostream>
#include <memory>
#include <thread>

using namespace osp;

void config_controls();

/**
 * As the name implies. This should only be called once for the entire lifetime
 * of the program.
 *
 * prefer not to use names like this anywhere else but main.cpp
 */
void load_a_bunch_of_stuff();

/**
 * Register satellite types into the universe to add support for them.
 */
void register_universe_types();

/**
 * Try to everything in the universe
 */
bool destroy_universe();

/**
 * The spaghetti command line interface that gets inputs from stdin. This
 * function will only return once the user exits.
 * @return An error code maybe
 */
int debug_cli_loop();

// called only from commands to display information
void debug_print_help();
void debug_print_sats();
void debug_print_hier();
void debug_print_update_order();

// Deals with the underlying OSP universe, with the satellites and stuff. A
// Magnum application or OpenGL context is not required for the universe to
// exist. This also stores loaded resources in packages.
osp::OSPApplication g_osp;

// Deals with the window, OpenGL context, and other game engine stuff that
// often have "Active" written all over them
std::unique_ptr<OSPMagnum> g_ospMagnum;
std::thread g_magnumThread;

// lazily save the arguments to pass to Magnum
int g_argc;
char** g_argv;

int main(int argc, char** argv)
{
    // just lazily save the arguments
    g_argc = argc;
    g_argv = argv;

    load_a_bunch_of_stuff();

    register_universe_types();

    create_simple_solar_system(g_osp);

    // start doing debug cli loop
    return debug_cli_loop();
}

int debug_cli_loop()
{

    debug_print_help();

    std::string command;

    while(true)
    {
        std::cout << "> ";
        std::cin >> command;

        if (command == "help")
        {
            debug_print_help();
        }
        else if (command == "simple")
        {
            if (destroy_universe())
            {
                create_simple_solar_system(g_osp);
            }
        }
        else if (command == "flight")
        {
            if (g_magnumThread.joinable())
            {
                g_magnumThread.join();
            }
            std::thread t(test_flight, std::ref(g_ospMagnum), std::ref(g_osp),
                          OSPMagnum::Arguments{g_argc, g_argv});
            g_magnumThread.swap(t);
        }
        else if (command == "list_uni")
        {
            debug_print_sats();
        }
        else if (command == "list_ent")
        {
            debug_print_hier();
        }
        else if (command == "list_upd")
        {
            debug_print_update_order();
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

    return 0;
}

bool destroy_universe()
{
    // Make sure no application is open
    if (g_ospMagnum.get() != nullptr)
    {
        std::cout << "Application must be closed to destroy universe.\n";
        return false;
    }

    // Destroy all satellites
    g_osp.get_universe().get_reg().clear();

    // Destroy blueprints as part of destroying all vehicles
    g_osp.debug_get_packges()[0].clear<BlueprintVehicle>();

    std::cout << "*explosion* Universe destroyed!\n";

    return true;
}

void load_a_bunch_of_stuff()
{
    // Create a new package
    osp::Package lazyDebugPack("lzdb", "lazy-debug");

    // Load sturdy glTF files
    osp::AssetImporter::load_sturdy_file("OSPData/adera/spamcan.sturdy.gltf", lazyDebugPack);
    osp::AssetImporter::load_sturdy_file("OSPData/adera/stomper.sturdy.gltf", lazyDebugPack);

    // Add package to the univere
    g_osp.debug_get_packges().push_back(std::move(lazyDebugPack));

    // Add 50 vehicles so there's something to load
    //g_osp.get_universe().get_sats().reserve(64);

    //s_partsLoaded = true;

    std::cout << "Resource loading complete\n\n";
}

void register_universe_types()
{
    osp::universe::Universe &uni = g_osp.get_universe();
    uni.type_register<osp::universe::SatActiveArea>(uni);
    uni.type_register<osp::universe::SatVehicle>(uni);
    uni.type_register<planeta::universe::SatPlanet>(uni);
}

void debug_print_help()
{
    std::cout
        << "OSP-Magnum Temporary Debug CLI\n"
        << "Choose a test universe:\n"
        << "* simple    - Simple test planet and vehicles (default)\n"
        << "\n"
        << "Start Application:\n"
        << "* flight    - Create an ActiveArea and start Magnum\n"
        << "\n"
        << "Other things to type:\n"
        << "* list_uni  - List Satellites in the universe\n"
        << "* list_ent  - List Entities in active scene\n"
        << "* list_upd  - List Update order from active scene\n"
        << "* help      - Show this again\n"
        << "* exit      - Deallocate everything and return memory to OS\n";
}

void debug_print_update_order()
{
    if (!g_ospMagnum)
    {
        std::cout << "Can't do that yet, start the magnum application first!\n";
        return;
    }

    osp::active::UpdateOrder &order = g_ospMagnum->get_scenes().begin()
                                            ->second.get_update_order();

    std::cout << "Update order:\n";
    for (auto call : order.get_call_list())
    {
        std::cout << "* " << call.m_name << "\n";
    }
}

void debug_print_hier()
{

    if (!g_ospMagnum)
    {
        std::cout << "Can't do that yet, start the magnum application first!\n";
        return;
    }

    std::cout << "ActiveScene Entity Hierarchy:\n";

    std::vector<active::ActiveEnt> parentNextSibling;
    active::ActiveScene &scene = g_ospMagnum->get_scenes().begin()->second;
    active::ActiveEnt currentEnt = scene.hier_get_root();

    parentNextSibling.reserve(16);

    while (true)
    {
        // print some info about the entity
        auto &hier = scene.reg_get<active::ACompHierarchy>(currentEnt);
        for (unsigned i = 0; i < hier.m_level; i ++)
        {
            // print arrows to indicate level
            std::cout << "  ->";
        }
        std::cout << "[" << int(currentEnt) << "]: " << hier.m_name << "\n";

        if (hier.m_childCount)
        {
            // entity has some children
            currentEnt = hier.m_childFirst;


            // save next sibling for later if it exists
            if (hier.m_siblingNext != entt::null)
            {
                parentNextSibling.push_back(hier.m_siblingNext);
            }
        }
        else if (hier.m_siblingNext != entt::null)
        {
            // no children, move to next sibling
            currentEnt = hier.m_siblingNext;
        }
        else if (parentNextSibling.size())
        {
            // last sibling, and not done yet
            // is last sibling, move to parent's (or ancestor's) next sibling
            currentEnt = parentNextSibling.back();
            parentNextSibling.pop_back();
        }
        else
        {
            break;
        }
    }
}

void debug_print_sats()
{

    universe::Universe &universe = g_osp.get_universe();

    auto view = universe.get_reg().view<universe::UCompTransformTraj,
                                        universe::UCompType>();

    for (universe::Satellite sat : view)
    {
        auto &posTraj = view.get<universe::UCompTransformTraj>(sat);
        auto &type = view.get<universe::UCompType>(sat);

        auto &pos = posTraj.m_position;

        std::cout << "SATELLITE: \"" << posTraj.m_name << "\" \n";
        if (type.m_type)
        {
            std::cout << " * Type: " << type.m_type->get_name() << "\n";
        }

        if (posTraj.m_trajectory)
        {
            std::cout << " * Trajectory: "
                      << posTraj.m_trajectory->get_type_name() << "\n";
        }

        std::cout << " * Position: ["
                  << pos.x() << ", " << pos.y() << ", " << pos.z() << "]\n";
    }


}
