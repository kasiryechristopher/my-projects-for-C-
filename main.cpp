/*
 * =============================================================================
 *  NDEJJE FARMERS SACCO - LOAN MANAGEMENT INFORMATION SYSTEM
 *  Ndejje Town Council, Luweero District, Uganda
 *
 *  File    : main.cpp
 *  Purpose : Program entry point. main() is deliberately short: it creates
 *            the SaccoSystem object (which loads all saved data) and runs it.
 *            All the work is done inside well-named classes and functions.
 *
 *  Build (any C++11 compiler), from the folder containing the source files:
 *      g++ -std=c++11 -Wall -O2 -o NdejjeSACCO *.cpp
 *  Then run:
 *      ./NdejjeSACCO          (Linux / macOS)
 *      NdejjeSACCO.exe        (Windows)
 * =============================================================================
 */
#include <iostream>
#include <exception>
#include "SaccoSystem.h"

int main()
{
    try
    {
        SaccoSystem sacco;     // constructor loads members, loans, transactions, users, settings
        sacco.run();           // shows the welcome screen and menus until the user exits
    }
    catch (const std::exception& error)
    {
        std::cerr << "\n  [FATAL ERROR] " << error.what() << "\n"
                  << "  The program had to stop. Data saved before this point is safe.\n";
        return 1;
    }
    catch (...)
    {
        std::cerr << "\n  [FATAL ERROR] An unknown error occurred. The program had to stop.\n";
        return 1;
    }
    return 0;
}
