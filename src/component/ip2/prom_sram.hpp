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

    class PROMSRAM : public Component
    {
    public: 
        void Start() override
        {
            // map the private ram
            AddrSpaceMapping mapping = AddrSpaceMapping();

            mapping.startAddr = SRAM_START;
            mapping.endAddr = mapping.startAddr + SRAM_SIZE;
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

            CoherentEditor::Settings settings;
            settings.buf = sram;
            settings.bufSize = SRAM_SIZE;
            settings.name = "SRAM Viewer";

            editor = new CoherentEditor(this, settings);
            Coherent::RegisterExtension(editor);
        }

        const char* GetName() override { return "IP2 SRAM"; };

        uint8_t Read8(size_t addr) override 
        { 
            addr %= (size_t)SRAM_SIZE;
            return (sram[addr]); 
        }

        uint16_t Read16(size_t addr) override 
        { 
            addr %= (size_t)SRAM_SIZE;

            return READ16_BE(sram, addr);
        }

        uint32_t Read32(size_t addr) override 
        { 
            addr %= (size_t)SRAM_SIZE;
            return READ32_BE(sram, addr);
        }

        void Write8(size_t addr, uint8_t value) override
        { 
            addr %= (size_t)SRAM_SIZE;
            sram[addr] = value;
        }

        void Write16(size_t addr, uint16_t value) override
        { 
            addr %= (size_t)SRAM_SIZE;
            WRITE16_BE(sram, addr, value);
        }

        void Write32(size_t addr, uint32_t value) override
        { 
            addr %= (size_t)SRAM_SIZE;
            WRITE32_BE(sram, addr, value);
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

            delete editor; 
        }

    private: 
        uint8_t sram[SRAM_SIZE];

        FileStream* sramFile;
        CoherentEditor* editor;
    };
}