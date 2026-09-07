/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    gf2_fbc.hpp: The Frame Buffer Controller, four AMD AM2903 chips running custom SGI Microcode.
*/

#pragma once
#include <component/component.hpp>
#include <component/gpu/juniper/gf2/gf2_fbc.hpp>

namespace Motion
{
    #define GF2FAKE_START           0x50002000
    #define GF2FAKE_END             0x50002FFF

    #define GF2_PRIVATE_BUS_START   0x60000000
    #define GF2_PRIVATE_BUS_END     0x60001FFF

    #define GF2_MULTIBUS_SLOT       18

    #define GF2_GE_LOG_PREFIX       "GF2 - Geometry Engine"

    class GF2GE
    {
    public: 
        const char* GetName() { return "Geometry Engine Rev 2.0/2.5"; }; 

        void Start();
        void Tick();
    private:
        GF2FBC* fbc; // needed for passthrough

    }; 
}; 