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
    bool Memory::IsInRange(size_t addr, size_t width, const char* what)
    {
        // The whole operand has to fit: a long read one byte short of the end used to run off the allocation.
        if (addr < GetRamCapacity()
        && width <= (GetRamCapacity() - addr))
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

    /*
        Every access below assembles the value a byte at a time, big endian, at the exact address it
        was given.

        It used to index a 16 or 32 bit view of the array with addr >> 1 or addr >> 2, which
        silently threw the bottom one or two address bits away. **The 68020 permits misaligned word
        and long operands** - unlike the 68000 and 68010, which take an address error - and splits
        them into as many bus cycles as it needs, so a compiler is free to emit `move.l (a0)+,(a1)+`
        over a buffer that is not four byte aligned, and IRIX's does. Rounding the address down meant
        every such access read or wrote up to three bytes early, which shifted the data by exactly
        (addr & 3) bytes and was invisible whenever the buffer happened to be aligned.

        What that looked like: a byte lost or gained at the front of a string. `tset -s -Q` printing
        `setenv TERM |wsiri ;` for `wsiris` (its name pointer landed one past a 4 byte boundary, so
        every long the copy loop read came from one byte lower, dragging the preceding `|` in and
        dropping the trailing `s`), `telinit` arriving as `elinit`, `PST8PDT` as `TPST8PDT`. It read
        as random because it depends purely on where a buffer happens to sit.

        gcc and clang both fold the byte assembly back into one unaligned load plus a bswap, so this
        is not slower than the version that was wrong.
    */

    uint8_t Memory::Read8(size_t addr)
    {
        if (!IsInRange(addr, sizeof(uint8_t), "Read8"))
            return 0;

        return ram[addr];
    }

    uint16_t Memory::Read16(size_t addr)
    {
        if (!IsInRange(addr, sizeof(uint16_t), "Read16"))
            return 0;

        return (uint16_t)(((uint16_t)ram[addr] << 8) | ram[addr + 1]);
    }

    uint32_t Memory::Read32(size_t addr)
    {
        if (!IsInRange(addr, sizeof(uint32_t), "Read32"))
            return 0;

        return ((uint32_t)ram[addr] << 24)
            | ((uint32_t)ram[addr + 1] << 16)
            | ((uint32_t)ram[addr + 2] << 8)
            | (uint32_t)ram[addr + 3];
    }

    void Memory::Write8(size_t addr, uint8_t value)
    {
        if (!IsInRange(addr, sizeof(uint8_t), "Write8"))
            return;

        ram[addr] = value;
    }

    void Memory::Write16(size_t addr, uint16_t value)
    {
        if (!IsInRange(addr, sizeof(uint16_t), "Write16"))
            return;

        ram[addr] = (uint8_t)(value >> 8);
        ram[addr + 1] = (uint8_t)value;
    }

    void Memory::Write32(size_t addr, uint32_t value)
    {
        if (!IsInRange(addr, sizeof(uint32_t), "Write32"))
            return;

        ram[addr] = (uint8_t)(value >> 24);
        ram[addr + 1] = (uint8_t)(value >> 16);
        ram[addr + 2] = (uint8_t)(value >> 8);
        ram[addr + 3] = (uint8_t)value;
    }

    void Memory::Shutdown()
    {
        delete memoryEditor;
        delete[] ram;
    }
}