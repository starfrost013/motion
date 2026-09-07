/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    machine_iris3130.cpp: Implements the SGI IRIS 3130 machine.
*/

#include <base/machine/machine.hpp>
#include <base/machine/machines.hpp>
#include <component/memory.hpp>
#include <component/cpu/mc68020.hpp>
#include <component/ip2/prom.hpp>
#include <component/ip2/prom_sram.hpp>
#include <component/ip2/ip2_mmu.hpp>
#include <component/ip2/ip2_rtc.hpp>
#include <component/ip2/ip2_duart.hpp>
#include <component/ip2/ip2_dip_switches.hpp>
#include <component/ip2/ip2_mouse.hpp>
#include <component/keyboard/keyboard_iris.hpp>
#include <component/multibus/multibus.hpp>
#include <component/gpu/juniper/dc4/dc4.hpp>
#include <component/gpu/juniper/uc4/uc4.hpp>
#include <component/gpu/juniper/bp3/bp3.hpp>
#include <component/storage/dsd5217.hpp>
#include <component/gpu/juniper/gf2/gf2_coordinator.hpp>

namespace Motion
{
    // this is extremely temporary
    Cvar* forceEnterSerialMonitor;

    void IRIS3130::AddComponents()
    {
        AddComponent<Memory>();
        AddComponent<PROM>();
        AddComponent<MC68020>();
        AddComponent<PROMSRAM>();
        AddComponent<Multibus>();
        AddComponent<BP3>();
        AddComponent<IP2Interrupt>();
        AddComponent<IP2MMU>();
        AddComponent<DUART68681>();
        AddComponent<IP2Switches>();
        AddComponent<IP2Clock>();
        AddComponent<DC4>();
        AddComponent<UC4>();
        AddComponent<DSD5217>();
        
        // add in reverse order
        AddComponent<GF2Coordinator>();
        AddComponent<IP2Mouse>();

        forceEnterSerialMonitor = Cvar::Get("forceEnterSerialMonitor", "0");

        // temporary debug solution utnil the graphics system works
        // unplugging the keyboard forces these machines into this mode
        if (!forceEnterSerialMonitor->GetValue())
            AddComponent<KeyboardIris>();
            
    }

    void IRIS3130::Reset()
    {
        Memory* memory = Emulation::GetMachine()->FindComponentByType<Memory>();

        // i have no idea how the machine gets these so just write them in before reset 
        
        memory->Write32(0x0, 0x33000800);        // initial sp
        memory->Write32(0x4, 0x30000400);        // initial pc
        Machine::Reset();
    }
}