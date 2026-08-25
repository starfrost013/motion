#pragma once

#include <Motion.hpp>
#include <component/gpu/vram.hpp>

namespace Motion
{
    extern Cvar* numBitplanes;
    
    #define LOG_PREFIX_BP3              "VRAM - BP3"

    class BP3 : public ComponentVRAM
    {
    public:
        void Start() override;
        void Shutdown() override;

        // these get the internal fb size
        
        int32_t GetInternalFbSizeX() override { return 1024; };
        int32_t GetInternalFbSizeY() override { return 1024; };

        // We model our VRAM as a 1024*1024*32 bits and mask all writes to the appropriate number of bitplanes.
        virtual int32_t GetBytesPerPixel() override { return 4; }; 

        size_t GetVramAddress(int32_t x, int32_t y) override;

        // Register I/O
        uint8_t Read8(size_t addr) override;
        uint16_t Read16(size_t addr) override;
        uint32_t Read32(size_t addr) override;
        void Write8(size_t addr, uint8_t value) override;
        void Write16(size_t addr, uint16_t value) override;
        void Write32(size_t addr, uint32_t value) override; 
        
        const char* GetName() override { return "Video RAM (BP3 Bitplane boards)"; };

        // One bit per fitted bitplane.
        uint32_t GetPlaneMask() { return writeMask; };

    private:
        // Guard for every VRAM access; see the note in bp3_core.cpp.
        bool InRange(size_t addr, size_t width);

        // implements our BPs
        uint32_t realBitplanes;
        uint32_t writeMask;
    };
};