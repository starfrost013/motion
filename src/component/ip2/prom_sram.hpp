/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    prom_sram.hpp: Private sram for the PROM to use and store settings. (and do stuff - 2 KB)
*/

#pragma once 
#include <Motion.hpp>
#include <base/filesystem/filesystem.hpp>
#include <base/profile/profile.hpp>
#include <component/addrspace.hpp>

namespace Motion
{
    extern Cvar* promPath;
    extern Cvar* promSize;

    #define SRAM_START          0x33000000
    #define SRAM_SIZE           2048

    // don't se ewhy this should be configurable
    #define SRAM_PATH           "ip2_sram.bin"      // in log folder
    #define LOG_PREFIX_SRAM     "Emulation - IP2 SRAM"

    // FOR COMPONENTS, WE DON'T NEED TO BOUNDS CHECK BECAUSE WE ALREADY MAPPED IT!

    class PROM_SRAM : public Component
    {
    public: 
        // The reset stack pointer points at the top of this, so it must be mapped before the CPU starts.
        bool IsEarlyStart() override { return true; };

        void Start() override
        {
            // map the private ram
            AddrSpaceMapping mapping = AddrSpaceMapping();

            mapping.startAddr = SRAM_START;
            mapping.endAddr = mapping.startAddr + SRAM_SIZE - 1;   // GetMapping's end is inclusive
            mapping.component = this;
            AddrSpace::AddMapping(mapping);

            // try and open a folder in the profile
            
            sramFile = Profile::Open(SRAM_PATH, FileFlags::Binary);
            
            if (!sramFile)
            {
                sramFile = Profile::Open(SRAM_PATH, (FileFlags)(FileFlags::Binary | FileFlags::Create));

                if (!sramFile)
                    Logger::Log(LOG_PREFIX_SRAM, "Failed to open SRAM file. Settings won't be preserved!", LogChannels::Error);   
            }

            // read in the sram if its open
            if (sramFile)
            {
                sramFile->stream.read((char*)sram, SRAM_SIZE);
                Profile::Close(sramFile);
                sramFile = nullptr;
            }
        }

        const char* GetName() override { return "IRIS 3130 System PROM Private SRAM [MCM2016HN16 - IP2/U95]"; };

        uint8_t Read8(size_t addr) override 
        { 
            addr %= (size_t)SRAM_SIZE;
            return (sram[addr]); 
        }

        /*
            Byte at a time, big endian, at the address given - see the note in memory.cpp. Read32 had
            a second bug on top of that one: it swapped into a uint16_t, truncating the result, and
            then returned the *unswapped* word anyway, so every 32 bit read of the SRAM came back
            little endian.
        */
        uint16_t Read16(size_t addr) override 
        { 
            addr %= (size_t)SRAM_SIZE;
            return (uint16_t)(((uint16_t)sram[addr] << 8) | sram[(addr + 1) % (size_t)SRAM_SIZE]);
        }

        uint32_t Read32(size_t addr) override 
        { 
            return ((uint32_t)Read16(addr) << 16) | Read16(addr + 2);
        }

        void Write8(size_t addr, uint8_t value) override
        { 
            addr %= (size_t)SRAM_SIZE;
            sram[addr] = value; 
        }

        void Write16(size_t addr, uint16_t value) override
        { 
            addr %= (size_t)SRAM_SIZE;
            sram[addr] = (uint8_t)(value >> 8);
            sram[(addr + 1) % (size_t)SRAM_SIZE] = (uint8_t)value;
        }

        void Write32(size_t addr, uint32_t value) override
        { 
            Write16(addr, (uint16_t)(value >> 16));
            Write16(addr + 2, (uint16_t)value);
        }

        void Shutdown() override
        {
            // just in case somehow we managed to call reset mid write, we will do this instead
            if (sramFile)
                Profile::Close(sramFile);

            sramFile = Profile::Open(SRAM_PATH);
            Filesystem::Seek(sramFile, 0);
            sramFile->stream.write((char*)sram, SRAM_SIZE);
            Profile::Close(sramFile);
        }

    private: 
        uint8_t sram[SRAM_SIZE];

        FileStream* sramFile;
    };
}