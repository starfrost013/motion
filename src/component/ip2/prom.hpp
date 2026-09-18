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
        void Start() override;
        void Shutdown() override;

        const char* GetName() override { return "IRIS 3130 System PROM"; };

        uint8_t Read8(size_t addr) override;
        void Write8(size_t addr, uint8_t value) override;
        uint16_t Read16(size_t addr) override;
        void Write16(size_t addr, uint16_t value) override;
        uint32_t Read32(size_t addr) override;
        void Write32(size_t addr, uint32_t value) override;

        bool IsEarlyStart() override { return true; }; 

    private: 
        uint8_t* rom;

    };
}