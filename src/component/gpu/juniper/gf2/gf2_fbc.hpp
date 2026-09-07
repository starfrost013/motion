/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    gf2_fbc.hpp: The Frame Buffer Controller, four AMD AM2903 chips running custom SGI Microcode.

    Also incorproates the Bitplane Controller, an interface to VRAM
*/

#pragma once
#include <component/component.hpp>
#include <component/gpu/juniper/gf2/am2903/am2903.hpp>

namespace Motion
{
    class GF2FBC
    {
    public: 
        void Start();
        void Tick();

        const char* GetName() { return "Framebuffer & Bitplane Controller (AMD Am2903)"; }; 
        
    private:
        AM2903 Am2903; 
    }; 
}; 