/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    gf2_ge.cpp: 14 custom geometry engines.

    Basically, you have 14 custom geometry engine chips (which are four 32-bit alus and a microcode store with a config register loaded to switch function) 
    * first one is a fifo which converts from ieee 754 to 20.8 fixed point ("geometry accelerator")
    * next 4 basically make up a 4x4 matrix multiplier, 
    * the next 6 are clipping (and also z-buffering)
    * next 2 are scaling, they scale 2 coordinates at once each
    * last one converts back out to ieee 754 and fifo's
     
    Also Z Buffering is used

    Note that we don't model all these chips but actually model them as a single chip which performs functions of all of them (FBC)

    on the ip2 board it's not on multibus, it's a private bus, segment 6 and also parts of MBIO, but you can treat it as a static address that never changes
*/

#include <component/multibus/multibus.hpp>
#include <component/memory.hpp>
#include <component/gpu/juniper/gf2/gf2.hpp>

namespace Motion
{

    void GF2::GEStart()
    {
        geBusy = false; 
        geReset = false; 
    }

    uint16_t GF2::GERead16(size_t addr)
    {
        uint16_t value = 0x00; 

        switch (addr)
        {
            case GF2_GE_FLAGS:
                value = geFlagsRead;

                if (geReset)
                    value |= GF2_GE_FLAG_WRITE_RESET;
                else    
                    value &= ~(GF2_GE_FLAG_WRITE_RESET);
                break;
        }
        
        Logger::Log(GF2_GE_LOG_PREFIX, std::format("GE Read16 0x{:x} from 0x{:x}", value, addr).c_str(), LogChannels::Debug);
        return value; 
    }

    void GF2::GEWrite16(size_t addr, uint16_t value)
    {
        switch (addr)
        {
            case GF2_GE_FLAGS:
                geFlagsWritten = value; 

                // sets to gedebug BUT THEN EXPECTS IT TO BE ZERO?
                if (geFlagsWritten == 0x813e)
                    geFlagsWritten = 0;
                break; 
            case GF2_GE_DATA:
                GEPushCommandWord(value);
                break; 
        }

        
        Logger::Log(GF2_GE_LOG_PREFIX, std::format("GE Write16 0x{:x} to 0x{:x}", value, addr).c_str(), LogChannels::Debug);
    }

    //
    // Command parsing
    //
    
    /// @brief get the number of geometry engine parameters for a given command. MUST BE CONVERTED INTO TWO TABLES
    /// @param id the id to get 
    uint32_t GF2::GEGetCmdNrParameters(uint16_t word)
    {
        uint32_t numParameters = 0;

        switch (word)
        {
            default: // commands encode parameters for dimensions of operation and type 
                switch (word & GE_CMD_PARAM_DIMENSION_MASK)
                {
                    case GE_CMD_OPERATES_ON_2D:
                        numParameters = 2;
                        break;
                    case GE_CMD_OPERATES_ON_3D:
                        numParameters = 3;
                        break;
                    default:
                        numParameters = 4;
                        break;
                }

                if (!((word & GE_CMD_PARAM_TYPE_MASK) == GE_CMD_PARAM_IS_SHORT))
                    numParameters <<= 1; // bus is 16 bit
                break;
            case GE_CMD_POPMM: case GE_CMD_PUSHMM:
            case GE_CMD_NOOP:
            case GE_CMD_RECONFIGURE: 
            case GE_CMD_CLOSEPOLY:
                numParameters = 0;
                break;
            case GE_CMD_LOADMM:
                numParameters = 32;
                break;
            case GE_CMD_LOADVIEWPORT:
                numParameters = 16; // 8 dwords
                break; 
        }

        return numParameters;
    }

    void GF2::GEPushCommandWord(uint16_t word)
    {
        uint16_t cmd = (word & 0xFF);


        // Go into the fifo
        
        pipeData[pipeWritePtr] = word; 

        // commands passed through to fbc
        if (passthroughSize > 0 && passthroughWords < passthroughSize)
        {
            passthroughWords++;
            return;
        }

        // high byte all bits high = reconfigure all ges
        if (theGe.reconfiguring && (word & 0xFF00) == 0xFF00)
            theGe.reconfiguring = false; 

        // if 0, it's a new command...
        if (theGe.lastGeCommand == GE_NO_COMMAND)
        {
            theGe.lastGeCommand = cmd;
            theGe.lastGeCommandOffset = pipeWritePtr;
        }
        else
        {
            theGe.lastGeCommandParameters++;

            if (theGe.lastGeCommandParameters == GEGetCmdNrParameters(word))
            {
                Logger::Log(GF2_GE_LOG_PREFIX, std::format("About to execute GE command 0x{:x} with 0x{:x} parameters", cmd, theGe.lastGeCommandParameters).c_str(), LogChannels::Debug);
                GEExecuteCommand(word);

                theGe.lastGeCommand = GE_NO_COMMAND;
                theGe.lastGeCommandParameters = 0;
            }
        }

        if (cmd == GE_CMD_PASSTHRU)
        {
            if (passthroughWords == 0)
            {
                passthroughSize = ((word >> 8)) + 1;
                passthroughStart = pipeWritePtr;
            }


            if (passthroughWords == passthroughSize)
            {
                passthroughSize = passthroughStart = passthroughWords = 0;
                Logger::Log(GF2_GE_LOG_PREFIX, std::format("About to execute FBC command 0x{:x} with 0x{:x} parameters", cmd, passthroughWords).c_str(), LogChannels::Debug);
                FBCExecuteCommand();

            }
        }
            // Go to FBC Microcode Engine

        
        pipeWritePtr++;
        pipeWritePtr %= GF2_GE_MAX_PARAMETERS;

    }

    uint16_t GF2::GEPeekNextCommandWord()
    {
        pipePeekPtr++;
        pipePeekPtr %= GF2_GE_MAX_PARAMETERS;

        return pipeData[pipePeekPtr];
    }

    //
    // Command eexcution
    //

    void GF2::GEExecuteCommand(uint16_t commandId)
    {
        switch (commandId)
        {
            case GE_CMD_POPMM:
                break;
        }
    }


}