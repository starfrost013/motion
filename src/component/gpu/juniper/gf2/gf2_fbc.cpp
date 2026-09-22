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
    void GF2::FBCStart()
    {
        CoherentEditor::Settings settings;
        settings.buf = (uint8_t*)ucode;
        settings.bufSize = GF2_FBC_UCODE_SLICES * GF2_FBC_UCODE_STATES;
        settings.name = "FBC Microcode Editor";

        fbcUcodeEditor = new CoherentEditor(this, settings);
        Coherent::RegisterExtension(fbcUcodeEditor);
    }

    uint16_t GF2::FBCRead16(size_t addr)
    {
        uint16_t value = 0x00;
        
        // TODO:
        if (addr >= GF2_FBC_DATA_START && addr <= GF2_FBC_DATA_END
        && fbcFlagsWritten == 0xFF)
        {
            uint16_t ucodeValue = ucode[GetCurrentUcodeState(addr)][GetCurrentUcodeSlice()];
            // the top slice of each state is 8b its wide
            value = (GetCurrentUcodeSlice() == 3) ? (ucodeValue & 0xFF) : ucodeValue;
        }    
        else
        {
            switch (addr)
            {
                case GF2_FBC_FLAGS:
                    // ensure that GL2 init knows we are alive
                    fbcFlagsRead |= GF2_FBC_FLAGS_READ_FBC_ACK;
                    fbcFlagsRead |= GF2_FBC_FLAGS_READ_BPC_ACK;
                    
                    value = fbcFlagsRead;
                    break;
            }
        }

        Logger::Log(std::format("FBC Read16 0x{:x} from 0x{:x}", value, addr).c_str(), LogChannels::Debug);
        return value; 
    }

    void GF2::FBCWrite16(size_t addr, uint16_t value)
    {
        // write to ucode
        if (addr >= GF2_FBC_DATA_START && addr <= GF2_FBC_DATA_END
        && fbcFlagsWritten == 0xFE)
        {
            uint16_t state = GetCurrentUcodeState(addr);
            uint16_t slice = GetCurrentUcodeSlice();
            ucode[state][slice] = value;
        }
        else
        {
            switch (addr)
            {
                case GF2_FBC_FLAGS:
                    fbcFlagsWritten = value;
                    break;
                // ON WRITE TO FBCDATA, INITIATE UCODE OPERATIONS!
                // THE UCODE MUST BE RUN ACCORDING TO THE COMMAND, WHICH WAS WRITTEN TO FBCDATA!

            }
        }
        
        Logger::Log(std::format("FBC Write16 0x{:x} to 0x{:x}", value, addr).c_str(), LogChannels::Debug);
    }

    void GF2::FBCExecuteCommand()
    {
        
    }
    
    void GF2::BPCExecuteCommand()
    {

    }

}; 