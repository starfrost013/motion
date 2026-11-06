/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    gf2_fbc.cpp: FBC Orchestration

    Controls AM2903 (Data) & AM2910 (Control)
    Loads microcode into both

    SGI wrote the microcode in highly macro'd C which was compiled into their "mas" microcode assembler (which has been mostly ported to Linux by me). 
    The MAS Binary then converted the microcode into a binary file which was then uploaded. There's 4096 16-bit (4x"slices" for each am2903), controlled by an AM2910.

    Each state was started by _NS and ended by _ES
    
    MICROCODE FORMAT:
    /*================================================================
    /*   outputs
    /*================================================================

    Output NextAddress ={"di",		0,	15,	0};
    // 2910 Inputs 
    Output i0 = 		{"i0",		16,	16,	0};
    Output i1 =		    {"i1",		17,	20,	6};
    Output i2 =		    {"i2",		21,	24,	4};  deflt: WRE 
    
    Output cin =		{"cin",		25,	25,	0};
    Output earbar =		{"earbar",	26,	26,	0};
    Output ealbar =		{"ealbar",	27,	27,	0};
    Output addra =		{"addra",	28,	31,	0};
    Output addrb =		{"addrb",	32,	35,	0};

    // START PARTS THAT WE CAN HOPEFULLY IGNORE
    Output clklong =	{"clklong",	36,	36,	0};     
    Output get = 		{"get",		37,	37,	0};
    Output put =		{"put",		38,	38,	0};
    Output loadout = 	{"loadout",	39,	39,	0};    
    Output enram = 		{"enram",	40,	40,	0};
    Output rdram =		{"rdram",	41,	41,	0};
    Output highbyte =	{"highbyte",	42,	42,	0};
    Output rightjust =	{"rightjust",	43,	43,	0};
    // STOP PARTS THAT WE CAN HOPEFULLY IGNORE

    Output seqop =		{"seqop",	44,	47,	14};
    Output ccsel =		{"ccsel",	48,	50,	0};
    Output fbccode =	{"fbccode",	51,	54,	0};
    Output reverse =	{"reverse",	55,	55,	0};

    Output DIsrc = 		{"DIsrc",	0,	7,	0};
            pseudo-field for recording intended use of DI bus	
    Output seqtype = 	{"seqtype",	0,	7,	0};
            pseudo-field for categorizing 2910 opcode used	
    Output microconst = 	{"microconst",	-1,	-1,	0};
            pseudo-field for recording whether MICROCONST invoked 


*/
#include <component/component.hpp>
#include <component/gpu/juniper/gf2/gf2.hpp>

namespace Motion
{
    void GF2::FBCStart()
    {
        CoherentEditor::Settings settings;
        settings.buf = (uint8_t*)ucode.data;
        settings.bufSize = (GF2_FBC_UCODE_SLICES * GF2_FBC_UCODE_STATES) << 1; // 32 kb
        settings.name = "FBC Microcode Editor";
        
        fbcUcodeEditor = new CoherentEditor(this, settings);
        Coherent::RegisterExtension(fbcUcodeEditor);

        am2903.Start();
    }

    uint16_t GF2::FBCRead16(size_t addr)
    {
        uint16_t value = 0x00;
        
        // TODO:
        if (addr >= GF2_FBC_DATA_START && addr <= GF2_FBC_DATA_END
        && fbcFlagsWritten == 0xFF
        && UcodeAccessIsEnabled())
        {
            uint16_t ucodeValue = ucode.data[GetCurrentUcodeState(addr)][GetCurrentUcodeSlice()];
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
                case GF2_FBC_DATA_START:
                    value = FBCExecuteAlternativeCommand(lastFbcAltCommand);
                    break; 
            }
        }

        Logger::Log(GF2_FBC_LOG_PREFIX, std::format("FBC Read16 0x{:x} from 0x{:x}", value, addr).c_str(), LogChannels::Debug);
        return value; 
    }

    // the "real " fbc command set is passed through from the GEs

    uint16_t GF2::FBCExecuteAlternativeCommand(uint16_t id)
    {
        uint16_t ret = 0x00;

        switch (id)
        {
            case GF2_FBC_ALTCMD_GET_MICRO_VERSION:
                ret = GF2_FBC_MICRO_VERSION;
                break; 
            case GF2_FBC_ALTCMD_GET_SCRATCH_SIZE:
                ret = GF2_FBC_SCRATCH_SIZE;
                break;
        }

        Logger::Log(GF2_FBC_LOG_PREFIX, std::format("FBC Execute Alternative Command #{:x} returning 0x{:x}", id, ret).c_str(), LogChannels::Debug);
        return ret; 
    }

    void GF2::FBCWrite16(size_t addr, uint16_t value)
    {
        // write to ucode
        if (addr >= GF2_FBC_DATA_START && addr <= GF2_FBC_DATA_END
        && fbcFlagsWritten == 0xFE
        && UcodeAccessIsEnabled())
        {
            uint16_t state = GetCurrentUcodeState(addr);
            uint16_t slice = GetCurrentUcodeSlice();
            ucode.data[state][slice] = value;
        }
        else
        {
            switch (addr)
            {
                case GF2_FBC_FLAGS:
                    fbcFlagsWritten = value;
                    break;
                // "Alternative" Codes
                case GF2_FBC_DATA_START:  // not in condition to write uCode
                    lastFbcAltCommand = value;
                    break; 
                // ONLY ON REAL FBC COMMAND MUST UCODE OPERATIONS BE INITIALISED

            }
        }
        
        
        Logger::Log(GF2_FBC_LOG_PREFIX, std::format("FBC Write16 0x{:x} to 0x{:x}", value, addr).c_str(), LogChannels::Debug);
    }

    void GF2::FBCTick()
    {
        
    }

    void GF2::FBCExecuteCommand(uint16_t word) // size from GE Passthrough
    {
        // ok
        uint16_t passthroughPtr = passthroughStart;

        pipePeekPtr = passthroughStart;

        uint16_t cmd = GEPeekNextCommandWord();

        // Temporary buffer for storing the parameters

        uint16_t wordBuffer[GF2_GE_MAX_PARAMETERS] = {0};

        for (int32_t i = 0; i < passthroughWords - 1; i++)
        {
            uint16_t paramWord = GEPeekNextCommandWord();
            wordBuffer[i + 1] = paramWord; 
        }
    
        // ok we are done. get the microcode state start location
        uint16_t ucodeStart = cmd << 1; 

        // initiate execution of current ucode
        am2910.Start(ucodeStart, wordBuffer);
    }
    
    void GF2::BPCExecuteCommand(uint16_t word)
    {

    }

}; 