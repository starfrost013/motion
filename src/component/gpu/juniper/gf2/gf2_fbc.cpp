/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    gf2_fbc.cpp: Non-Am2903 parts of the F.BC
*/
#include <component/component.hpp>
#include <component/gpu/juniper/gf2/gf2.hpp>

namespace Motion
{
    uint16_t GF2::FBCRead16(size_t addr)
    {
        if (addr >= GF2_FBC_DATA_START && addr <= GF2_FBC_DATA_END)
            return GetRequestedFBCUcodeData(addr);
    }

    void GF2::FBCWrite16(size_t addr, uint16_t value)
    {
        
    }

}; 