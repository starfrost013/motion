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
#include <component/ip2/ip2_interrupt.hpp>
#include <component/ip2/ip2_mmu.hpp>
#include <component/ip2/ip2_rtc.hpp>
#include <component/ip2/ip2_duart.hpp>
#include <component/ip2/ip2_dip_switches.hpp>
#include <component/ip2/ip2_mouse.hpp>
#include <component/keyboard/keyboard_iris.hpp>
#include <component/multibus/multibus.hpp>
#include <component/gpu/juniper/dc4/dc4.hpp>
#include <component/gpu/juniper/gf2/gf2.hpp>
#include <component/gpu/juniper/uc4/uc4.hpp>
#include <component/gpu/juniper/bp3/bp3.hpp>
#include <component/storage/dsd5217.hpp>

namespace Motion
{
    // this is extremely temporary
    Cvar* forceEnterSerialMonitor;

    void IRIS3130::AddComponents()
    {
        AddComponent<Memory>();
        // PROM and PROM SRAM come before the CPU on purpose: both are early start, and the reset vector
        // and reset stack pointer live in them, so they have to be mapped before the CPU is reset.
        AddComponent<PROM>();
        AddComponent<PROM_SRAM>();
        AddComponent<MC68020>();
        AddComponent<Multibus>();
        AddComponent<BP3>();
        AddComponent<IP2Interrupt>();
        AddComponent<IP2MMU>();
        AddComponent<DUART68681>();
        AddComponent<IP2Switches>();
        AddComponent<IP2Clock>();
        AddComponent<IP2Mouse>();
        AddComponent<GF2>();
        AddComponent<DC4>();
        AddComponent<UC4>();
        AddComponent<DSD5217>();

        forceEnterSerialMonitor = Cvar::Get("forceEnterSerialMonitor", "0");

        // temporary debug solution utnil the graphics system works
        // unplugging the keyboard forces these machines into this mode
        if (!forceEnterSerialMonitor->GetValue())
            AddComponent<KeyboardIris>();
            
    }
}