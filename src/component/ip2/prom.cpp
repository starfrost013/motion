#include <component/ip2/prom.hpp>

namespace Motion
{
    Cvar* promPath;
    Cvar* promSize;
    
    void PROM::Start()
    {
        promPath = Cvar::Set("promPath", "./roms/iris3130/ip2/ip2_prom_3.0.10.bin");
        promSize = Cvar::Set("promSize", "98304");

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

    void PROM::Shutdown()
    {
        delete[] rom;
    }

}