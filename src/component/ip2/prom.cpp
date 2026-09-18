#include <component/ip2/prom.hpp>

namespace Motion
{
    Cvar* promPath;
    Cvar* promSize;
    
    void PROM::Start()
    {
        promPath = Cvar::Get("promPath", "./roms/iris3130/ip2/ip2_prom_3.0.10.bin");
        promSize = Cvar::Get("promSize", "98304");

        Logger::Log(LOG_PREFIX_PROM, std::format("Loading {} from {}, size is {} bytes", GetName(), promPath->GetString(), promSize->GetString()).c_str(), 
        LogChannels::Message);
        
        rom = new uint8_t[(size_t)promSize->GetValue()];

        // read in the rom, then close
        FileStream* prom = Filesystem::Open(promPath->GetString(), FileFlags::Binary);

        if (!prom) // noreturn
            Logger::Log("Failed to open PROM", LogChannels::FatalError);
  
        prom->stream.read((char*)rom, promSize->GetValue());
        Filesystem::Close(prom);
    
        Logger::Log(LOG_PREFIX_PROM, "Loaded PROM successfully. Mapping it...", LogChannels::Debug);

        AddrSpaceMapping mapping = AddrSpaceMapping();

        mapping.startAddr = PROM_START_ADDRESS;
        mapping.endAddr = mapping.startAddr + promSize->GetValue();

        mapping.component = this;
        AddrSpace::AddMapping(mapping);
    }

    uint8_t PROM::Read8(size_t addr)  
    { 
        addr %= (size_t)promSize->GetValue();
        return (rom[addr]); 
    };

    uint16_t PROM::Read16(size_t addr)  
    { 
        addr %= (size_t)promSize->GetValue();
        uint16_t* rom16 = (uint16_t*)rom; 
        uint16_t value =  rom16[addr >> 1]; 
        TOBE16(value);
        return value;
    };

    uint32_t PROM::Read32(size_t addr)
    { 
        addr %= (size_t)promSize->GetValue();
        uint32_t* rom32 = (uint32_t*)rom; 
        uint32_t value = rom32[addr >> 2]; 
        TOBE32(value);
        return value;
    };

    void PROM::Write8(size_t addr, uint8_t value)
    { 
        Logger::Log(LOG_PREFIX_PROM, std::format("Tried to write 8-bit {:x} to PROM mapped {:x}", value, addr).c_str(), LogChannels::Warning);
    };

    void PROM::Write16(size_t addr, uint16_t value)
    { 
        Logger::Log(LOG_PREFIX_PROM, std::format("Tried to write 16-bit {:x} to PROM mapped {:x}", value, addr).c_str(), LogChannels::Warning);
    };

    void PROM::Write32(size_t addr, uint32_t value)
    { 
        Logger::Log(LOG_PREFIX_PROM, std::format("Tried to write 32-bit {:x} to PROM mapped {:x}", value, addr).c_str(), LogChannels::Warning);
    };

    void PROM::Shutdown()
    {
        delete[] rom;
    }

}