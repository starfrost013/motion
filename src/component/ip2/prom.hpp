#include <Motion.hpp>
#include <base/filesystem/filesystem.hpp>
#include <component/addrspace.hpp>

namespace Motion
{
    extern Cvar* promPath;
    extern Cvar* promSize;

    #define PROM_START_ADDRESS      0x30000000
    #define PROM_SRAM_START_ADDRESS 0x33000000
    #define LOG_PREFIX_PROM         "Emulation - PROM"

    // FOR COMPONENTS, WE DON'T NEED TO BOUNDS CHECK BECAUSE WE ALREADY MAPPED IT!

    class PROM : public Component
    {
    public: 
        // The CPU fetches its reset vector out of here, so this has to be mapped before the CPU starts.
        bool IsEarlyStart() override { return true; };

        void Start() override;
        void Shutdown() override;

        const char* GetName() override { return "IRIS 3130 System PROM"; };

        uint8_t Read8(size_t addr) override 
        { 
            addr %= (size_t)promSize->GetValue();
            return (rom[addr]); 
        };

        uint16_t Read16(size_t addr) override 
        { 
            addr %= (size_t)promSize->GetValue();
            uint16_t* rom16 = (uint16_t*)rom; 
            uint16_t value =  rom16[addr >> 1]; 
            TOBE16(value);
            return value;
        };

        uint32_t Read32(size_t addr) override 
        { 
            addr %= (size_t)promSize->GetValue();
            uint32_t* rom32 = (uint32_t*)rom; 
            uint32_t value = rom32[addr >> 2]; 
            TOBE32(value);
            return value;
        };

        void Write8(size_t addr, uint8_t value) override
        { 
            Logger::Log(LOG_PREFIX_PROM, std::format("Tried to write 8-bit {:x} to PROM mapped {:x}", value, addr).c_str(), LogChannels::Warning);
        };

        void Write16(size_t addr, uint16_t value) override
        { 
            Logger::Log(LOG_PREFIX_PROM, std::format("Tried to write 16-bit {:x} to PROM mapped {:x}", value, addr).c_str(), LogChannels::Warning);
        };

        void Write32(size_t addr, uint32_t value) override
        { 
            Logger::Log(LOG_PREFIX_PROM, std::format("Tried to write 32-bit {:x} to PROM mapped {:x}", value, addr).c_str(), LogChannels::Warning);
        };

    private: 
        uint8_t* rom;

    };
}