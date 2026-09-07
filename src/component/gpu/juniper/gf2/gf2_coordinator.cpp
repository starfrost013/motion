/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    gf2_coordinator.cpp: The code for the gf2 coordinator
*/

#include <component/component.hpp>
#include <component/multibus/multibus.hpp>
#include <component/gpu/juniper/gf2/gf2_coordinator.hpp>

namespace Motion
{
    void GF2Coordinator::Start()
    {
        disableGfx = Cvar::Get("disableGfx", "0");

        if (disableGfx->GetValue())
            return;

        Logger::Log(GF2_GE_LOG_PREFIX, "Initialising GF2...");

        ge.Start();
        fbc.Start();

        // map gf2
        Multibus::SlotMapping mapping = Multibus::SlotMapping(this);
        mapping.ioStart = GF2_MULTIBUS_START;
        mapping.ioEnd = GF2_MULTIBUS_END;
        mapping.id = GF2_MULTIBUS_SLOT;

        multibus->AddSlotMapping(mapping);
        
        AddrSpaceMapping privMapping = AddrSpaceMapping();
        privMapping.startAddr = GF2_PRIVATE_BUS_START;
        privMapping.endAddr = GF2_PRIVATE_BUS_END;
        privMapping.component = this;

        AddrSpace::AddMapping(privMapping);
    } 
    
    // BIG ENDIAN! WE DO NOT USE MULTIBUS MEMORY SO WE DON'T NEED TO FLIP

    uint8_t GF2Coordinator::Read8(size_t addr) 
    {
        if (addr & 1)
            return (uint8_t)(Read16(addr & ~1) & 0x0000FFFF);
        else
            return (Read16(addr) & 0xFFFF0000) >> 8;
    }

    uint16_t GF2Coordinator::Read16(size_t addr) 
    {

    }

    uint32_t GF2Coordinator::Read32(size_t addr) 
    {
        return ((uint32_t)Read16(addr) << 16) + (Read16(addr + 2));
    }

    void GF2Coordinator::Write8(size_t addr, uint8_t value) 
    {
        uint16_t val = Read16(addr);

        // big endian
        if (addr & 1)
            val = (val & 0xFF00) | value;
        else
            val = (val & 0x00FF) | (value << 8);

        Write16(addr, val);
    }

    void GF2Coordinator::Write16(size_t addr, uint16_t value) 
    {

    }
    
    void GF2Coordinator::Write32(size_t addr, uint32_t value)  
    {
        Write16(addr, (value & 0xFFFF0000) >> 16);
        Write16(addr + 2, (value & 0x0000FFFF));
    }

    void GF2Coordinator::Tick() 
    {
        ge.Tick();
        fbc.Tick();
    }
};