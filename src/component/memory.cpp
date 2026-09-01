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
        mapping.endAddr = mapping.startAddr + capacity;
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
        settings.name = "Memory Viewer";

        CoherentEditor* editor = new CoherentEditor(this, settings);
        Coherent::RegisterExtension(editor);
    }

    uint8_t Memory::Read8(size_t addr)
    {
        addr %= GetRamCapacity();

        if (addr >= GetRamCapacity())
        {
            Logger::Log("Memory::Read8 - Tried to read from invalid RAM address!");
            return 0;
        }   

        return ram[addr];
    }

    uint16_t Memory::Read16(size_t addr)
    {
        addr %= GetRamCapacity();

        if (addr >= GetRamCapacity())
        {
            Logger::Log("Memory::Read16 - Tried to read from invalid RAM address!");
            return 0;
        } 

        return READ16_BE(ram, addr);
    }

    uint32_t Memory::Read32(size_t addr)
    {
        addr %= GetRamCapacity();

        if (addr >= GetRamCapacity())
        {
            Logger::Log("Memory::Read32 - Tried to read from invalid RAM address!");
            return 0;
        } 

        return READ32_BE(ram, addr);
    }

    void Memory::Write8(size_t addr, uint8_t value)
    {
        addr %= GetRamCapacity();

        if (addr >= GetRamCapacity())
        {
            Logger::Log("Memory::Write8 - Tried to write to invalid RAM address!");
            return;
        } 

        ram[addr] = value;
    }

    void Memory::Write16(size_t addr, uint16_t value)
    {
        addr %= GetRamCapacity();

        if (addr >= GetRamCapacity())
        {
            Logger::Log("Memory::Write16 - Tried to write to invalid RAM address!");
            return;
        } 

        WRITE16_BE(ram, addr, value);
    }

    void Memory::Write32(size_t addr, uint32_t value)
    {
        addr %= GetRamCapacity();
        
        if (addr >= GetRamCapacity())
        {
            Logger::Log("Memory::Write32 - Tried to write to invalid RAM address!");
            return;
        } 

        WRITE32_BE(ram, addr, value);
    }

    void Memory::Shutdown()
    {
        delete memoryEditor;
        delete[] ram;
    }
}