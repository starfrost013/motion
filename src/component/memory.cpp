/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    memory.cpp: RAM 
*/

#include <base/emulation.hpp>
#include <component/memory.hpp>

namespace Motion 
{
    #define MEM_SEG0_START          0x0
    #define MEM_SEG1_START          0x1000000
    #define MEM_SEG2_START          0x2000000
  
    void Memory::Start()
    {
        auto capacity = Emulation::GetMachine()->totalRamInstalled;
        ram = new uint8_t[capacity];
        Logger::Log(LOG_PREFIX_EMU_MACHINE, std::format("System RAM is {} bytes", capacity).c_str());
        
        AddrSpaceMapping mapping = AddrSpaceMapping();

        // gets mapped as text/data, stack and kernel

        // MMU segment 0 

        mapping.startAddr = 0x0;
        mapping.endAddr = mapping.startAddr + capacity - 1;   // GetMapping treats the end as inclusive
        mapping.component = this;
        AddrSpace::AddMapping(mapping);

        // This code is TEMPORARY for Version 0.1.x ONLY 
        ram[0] = 0x33; // initial sp=0x33000800
        ram[1] = 0x00;
        ram[2] = 0x08;
        ram[3] = 0x00;
        ram[4] = 0x30; // initial pc=0x30000400 (otherwise you start executing the vector base)
        ram[5] = 0x00;
        ram[6] = 0x04;
        ram[7] = 0x00;

        CoherentEditor::Settings settings;
        settings.buf = ram;
        settings.bufSize = capacity;
        settings.name = "Memory Editor";

        CoherentEditor* editor = new CoherentEditor(this, settings);
        Coherent::RegisterExtension(editor);
    }

    /*
        Reads and writes used to wrap with addr %= GetRamCapacity(), which also made every bounds
        check below it unreachable. RAM that is not fitted does not alias onto RAM that is - it reads
        as zero and swallows writes, which is both what MAME's IP2 RAM handler does and what the
        PROM's memory sizing loop depends on. The Multibus slave map can also aim a transfer at a
        frame past the end of RAM, and that has to read zero rather than corrupt a low page.
    */
    bool Memory::IsInRange(size_t addr, const char* what)
    {
        if (addr < GetRamCapacity())
            return true;

        if (outOfRangeLogged < MEMORY_MAX_OUT_OF_RANGE_LOGGED)
        {
            outOfRangeLogged++;

            Logger::Log(LOG_PREFIX_EMU_MACHINE, std::format("{} of 0x{:x} is past the {} bytes of RAM fitted{}",
                what, addr, GetRamCapacity(),
                (outOfRangeLogged == MEMORY_MAX_OUT_OF_RANGE_LOGGED) ? " - further ones will not be logged" : "").c_str(),
                LogChannels::Warning);
        }

        return false;
    }

    uint8_t Memory::Read8(size_t addr)
    {
        if (!IsInRange(addr, "Read8"))
            return 0;

        return ram[addr];
    }

    uint16_t Memory::Read16(size_t addr)
    {
        if (!IsInRange(addr, "Read16"))
            return 0;

        uint16_t* ram16 = (uint16_t*)ram;
        uint16_t value = ram16[addr >> 1];
        TOBE16(value);
        return value;
    }

    uint32_t Memory::Read32(size_t addr)
    {
        if (!IsInRange(addr, "Read32"))
            return 0;

        uint32_t* ram32 = (uint32_t*)ram;
        uint32_t value = ram32[addr >> 2];

        // IRIS is a big-endian system.
        TOBE32(value);
        return value;
    }

    void Memory::Write8(size_t addr, uint8_t value)
    {
        if (!IsInRange(addr, "Write8"))
            return;

        ram[addr] = value;
    }

    void Memory::Write16(size_t addr, uint16_t value)
    {
        if (!IsInRange(addr, "Write16"))
            return;
    
        uint16_t* ram16 = (uint16_t*)ram;       
        TOBE16(value);
        ram16[addr >> 1] = value;
    }

    void Memory::Write32(size_t addr, uint32_t value)
    {
        if (!IsInRange(addr, "Write32"))
            return;

        uint32_t* ram32 = (uint32_t*)ram;
        // IRIS is a big-endian system.
        TOBE32(value);
        ram32[addr >> 2] = value;
    }

    void Memory::Shutdown()
    {
        delete memoryEditor;
        delete[] ram;
    }
}