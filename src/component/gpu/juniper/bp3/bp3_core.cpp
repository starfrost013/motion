#include <component/gpu/juniper/bp3/bp3.hpp>

namespace Motion
{
    Cvar* numBitplanes;

    void BP3::Start()
    {
        bool invalid = false; 
        const char* invalidMessage = "";
        // Install the number of bitplanes that are required
        numBitplanes = Cvar::Get("numBitplanes", "32");
        realBitplanes = (int32_t)numBitplanes->GetValue();
        
        // BP3 boards each hold 1 bitplane. 
        if (realBitplanes % 4 != 0)
        {
            realBitplanes = ((int32_t)realBitplanes + 3) & ~3;
            invalidMessage = "Number of bitplanes must be a multiple of 4 (one BP3 board is 4 bitplanes)";
            invalid = true; 
        }

        if (realBitplanes > 32)
        {
            invalid = true;  
            invalidMessage = "Only up to 32 bitplanes can be installed.";
            realBitplanes = 32;
        }
        
        if (realBitplanes < 4)
        {
            invalid = true;
            invalidMessage = "At least 4 bitplanes have to be installed.";
            realBitplanes = 4;
        }

        // funny
        if (realBitplanes == 420)
        {
            invalid = true;
            invalidMessage = "Ur vram is a g ThAnG bRO...";
            realBitplanes = 32; 
        }

        if (invalid)
        {
            Logger::Log(LOG_PREFIX_BP3, std::format("Your bitplane setting was rejected because: {}."
                "The system will come up with {} bitplanes.", invalidMessage, realBitplanes).c_str(),
                LogChannels::Warning);
        }

        Logger::Log(LOG_PREFIX_BP3, std::format("There are {} bitplanes.", realBitplanes).c_str());

        // bad code
        if (realBitplanes == 32)
            writeMask = 0xFFFFFFFF;
        else
            writeMask = (1 << (realBitplanes)) - 1; 

        // only NOW call vram start once the real number of bitplanes was determined.
        ComponentVRAM::Start();

        // don't need to update to match.
    }

    // These are the same as memory read but due to the bitplane organisation we need to apply the write mask

    /*
        Every access is bounds checked, and it has to be. The drawing engines walk rectangles out of
        registers the guest wrote - UC4's fill and character draw run x and y straight from xsb/xeb
        and ysb/yeb - so a guest that asks to draw off the end of the screen would otherwise run off
        the end of this allocation and into the host heap. Real bitplanes cannot be addressed past
        their own ends; dropping the access is the closest thing to what the hardware does, and it
        turns a corrupted heap into a pixel that is not drawn.

        This is not a hypothetical: it fires as soon as the GL2 stack comes up far enough to program
        UC4, and it lands as a crash somewhere else entirely, whenever the heap is next touched.
    */
    bool BP3::InRange(size_t addr, size_t width)
    {
        return vram && (addr + width) <= GetCapacity();
    }

    uint8_t BP3::Read8(size_t addr)
    {
        if (!InRange(addr, sizeof(uint8_t)))
            return 0;

        return vram[addr];
    }

    /*
        VRAM is host order 32 bit pixels rather than a big endian byte array - the dumps and DC4 both
        read it that way - so unlike memory.cpp these stay in host order. What does change is the
        indexing: addr >> 1 / addr >> 2 threw the low address bits away, so a misaligned access, which
        a 68020 is allowed to make, silently aliased onto a lower pixel. memcpy reads the bytes that
        were actually asked for and is identical to the old code whenever the address was aligned.
    */
    uint16_t BP3::Read16(size_t addr) 
    {
        if (!InRange(addr, sizeof(uint16_t)))
            return 0;

        uint16_t value = 0;
        memcpy(&value, vram + addr, sizeof(value));
        return value;
    }

    uint32_t BP3::Read32(size_t addr) 
    {
        if (!InRange(addr, sizeof(uint32_t)))
            return 0;

        uint32_t value = 0;
        memcpy(&value, vram + addr, sizeof(value));
        return value;
    }

    void BP3::Write8(size_t addr, uint8_t value)
    {
        if (!InRange(addr, sizeof(uint8_t)))
            return;

        vram[addr] = value & writeMask;
    }

    void BP3::Write16(size_t addr, uint16_t value)
    {
        if (!InRange(addr, sizeof(uint16_t)))
            return;

        // A word index is the byte address halved, not quartered - >> 2 put every 16 bit write at
        // half the offset it was meant for, which is the same address two different pixels share.
        // It is a memcpy now so that a misaligned address lands where it was asked to; see Read16.
        uint16_t masked = (uint16_t)(value & writeMask);
        memcpy(vram + addr, &masked, sizeof(masked));
    }
    
    void BP3::Write32(size_t addr, uint32_t value)
    {
        if (!InRange(addr, sizeof(uint32_t)))
            return;

        uint32_t masked = value & writeMask;
        memcpy(vram + addr, &masked, sizeof(masked));
    }

    /// @brief get a vram address
    /// @param x the x coordinate to get
    /// @param y the y coordinate to get
    /// @return the vram address of (x,y)
    size_t BP3::GetVramAddress(int32_t x, int32_t y)
    {
        // size is always 1024x1024
        return (y * 4096) + (x << 2);
    }
    
    void BP3::Shutdown()
    {
        ComponentVRAM::Shutdown();
    }
};