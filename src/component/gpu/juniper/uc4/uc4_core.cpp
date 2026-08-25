/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    uc4_core.cpp: UC4 core stuff
*/

#include <component/gpu/juniper/uc4/uc4.hpp>

namespace Motion
{
    Cvar* logUC4;

    void UC4::Start()
    {
        // multibus is early start, guaranteed
        multibus = Emulation::GetMachine()->FindComponentByType<Multibus>();
        vram = Emulation::GetMachine()->FindComponentByType<ComponentVRAM>();

        Multibus::SlotMapping slot = Multibus::SlotMapping(this);

        slot.ioStart = UC4_REG_START;
        slot.ioEnd = UC4_REG_END;
        slot.id = UC4_MULTIBUS_SLOT;

        multibus->AddSlotMapping(slot);

        extensionUC4 = new CoherentExtensionUC4(this);
        Coherent::RegisterExtension(extensionUC4);

        uc4Channel = LogChannel(UC4_LOG_CHANNEL_NAME, ConsoleColor::BrightCyan, ConsoleColor::White);
        Logger::AddChannel(uc4Channel);
        logUC4 = Cvar::Get("logUC4", "0");
        
        logEnabled = logUC4->GetValue();

        if (logEnabled)
            Logger::SetChannelEnabled(UC4_LOG_CHANNEL_NAME);
    }

    // The status bits in UCR are read only, so they are not part of what the guest wrote and have to be added on the way out.
    uint16_t UC4::ReadUCR()
    {
        return (uint16_t)(ucr | UC4_UCR_VERTICAL);
    }

    // this one is a weird one. only the ucr register is read as 8 bits.
    // usually we try to only override as few methods as possible so let's just do this
    uint8_t UC4::Read8(size_t addr)
    {
        uint8_t ret = 0xFF;

        switch (addr)
        {
            case UC4_REG_UCR:
                ret = (uint8_t)(ReadUCR() >> 8);
                break;
        }

        return ret; 
    }

    // I/O

    uint16_t UC4::Read16(size_t addr) 
    {
        uint16_t ret = 0xFF;

        switch (addr)
        {
            case UC4_REG_UCR:
                ret = ReadUCR();
                break;
            default: // ???
                ret = ReadBuffer(addr);
                break;
        }

        Logger::Log(LOG_PREFIX_UC4, std::format("UC4 Read16 0x{:x} from 0x{:x}", ret, addr).c_str(), UC4_LOG_CHANNEL_NAME);
        return ret;
    }

    // only DDA buffers can be read
    uint16_t UC4::ReadBuffer(size_t addr)
    {
        uint16_t bufferNumber = UC4_ADDR_TO_BUFFER(addr);
        uint16_t ret = 0xFF; 

        switch (bufferNumber)
        {
            case UC4_BUFFER_DDASAF:
                ret = ddasaf;
                break;
            case UC4_BUFFER_DDASAI:
                ret = ddasai;
                break;
            case UC4_BUFFER_DDASDF:
                ret = ddasdf;
                break;
            case UC4_BUFFER_DDASDI:
                ret = ddasaf;
                break;
            case UC4_BUFFER_DDAEDF:
                ret = ddaedf;
                break;
            case UC4_BUFFER_DDAEDI:
                ret = ddaedf;
                break;
            case UC4_BUFFER_DDAEAF:
                ret = ddaeaf;
                break;      
            case UC4_BUFFER_DDAEAI:
                ret = ddaeai;
                break;  
        }

        return ret; 
    }

    void UC4::Write16(size_t addr, uint16_t value)
    {
        switch (addr)
        {
            case UC4_REG_UCR:
                ucr = value;
                break; 
            case UC4_REG_COMMAND ... UC4_COMMAND_TO_ADDR(UC4_CMD_LAST):
                ParseCommand(addr, value);
                break; 
            default:
                WriteBuffer(addr, value);
                break;
        }

        Logger::Log(LOG_PREFIX_UC4, std::format("UC4 Write16 0x{:x} to 0x{:x}", value, addr).c_str(), UC4_LOG_CHANNEL_NAME);
    }

    void UC4::WriteBuffer(size_t addr, uint16_t value)
    {
        uint16_t bufferNumber = UC4_ADDR_TO_BUFFER(addr);

        switch (bufferNumber)
        {
            case UC4_BUFFER_EDB:
                edb = value;
                break;
            case UC4_BUFFER_ECB:
                ecb = value;
                break;
            case UC4_BUFFER_XSB:
                xsb = value;
                break;
            case UC4_BUFFER_XEB:
                xeb = value;
                break;
            case UC4_BUFFER_YSB:
                ysb = value;
                break;
            case UC4_BUFFER_YEB: 
                yeb = value;
                break;
            case UC4_BUFFER_FMAB:
                fmab = value;
                break;
            case UC4_BUFFER_DDASAF:
                ddasaf = value;
                break;
            case UC4_BUFFER_DDASAI:
                ddasai = value;
                break;
            case UC4_BUFFER_DDASDF:
                ddasdf = value;
                break;
            case UC4_BUFFER_DDASDI:
                ddasdi = value;
                break;
            case UC4_BUFFER_DDAEDF:
                ddaedf = value;
                break;
            case UC4_BUFFER_DDAEDI:
                ddaedi = value;
                break;
            case UC4_BUFFER_DDAEAF:
                ddaeaf = value;
                break;      
            case UC4_BUFFER_DDAEAI:
                ddaeai = value;
                break;            
            case UC4_BUFFER_CFB:
                config = value;
                break;
            case UC4_BUFFER_MDB:
                mode = value;
                break;
            case UC4_BUFFER_RPB:
                repeat = value;
                break;
        }
    }

    void UC4::Shutdown()
    {
        
    }
};