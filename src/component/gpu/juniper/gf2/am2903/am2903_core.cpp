#include <component/gpu/juniper/gf2/am2903/am2903.hpp>

namespace Motion
{
    void AM2903::Start()
    {

    }

    void AM2903::ExecuteUcode(uint16_t addr)
    {
        Logger::Log(AM2903_LOG_PREFIX, std::format("The Am2910 told us to execute ucode address 0x{:x}", addr).c_str(), LogChannels::Debug);
    
        uint16_t word1 = ucode->data[addr][1];
    }
    
    void AM2903::Tick()
    {
        
    }
}; 