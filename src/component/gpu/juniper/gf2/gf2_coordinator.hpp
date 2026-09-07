/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    gf2_coordinator.hpp: The mappings for GF2 are messy as shit. So we map them to different components in here...
*/

#pragma once
#include <component/component.hpp>
#include <component/gpu/juniper/gf2/gf2_fbc.hpp>
#include <component/gpu/juniper/gf2/gf2_ge.hpp>

namespace Motion
{    
    Cvar* disableGfx; 

    class GF2Coordinator : Component
    {
    public: 
        void Start() override; 
            
        uint8_t Read8(size_t addr) override;
        uint16_t Read16(size_t addr) override;
        uint32_t Read32(size_t addr) override;
        void Write8(size_t addr, uint8_t value) override;
        void Write16(size_t addr, uint16_t value) override;
        void Write32(size_t addr, uint32_t value) override; 

        void Tick() override;

        const char* GetName() { return "GF2 Board Coordinator (GE+FBC)"; }; 
    private: 
        Multibus* multibus;
        GF2GE ge;
        GF2FBC fbc;
    }; 
}; 