/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    dc4_core.cpp: DC4 DAC & Palette RAM emulation
*/

#include <component/gpu/juniper/dc4/dc4.hpp>

namespace Motion
{
    Cvar* logDC4; 

    void DC4::Start()
    {
        // multibus is early start, guaranteed
        multibus = Emulation::GetMachine()->FindComponentByType<Multibus>();
        vram = Emulation::GetMachine()->FindComponentByType<ComponentVRAM>();

        Multibus::SlotMapping slot = Multibus::SlotMapping(this);

        slot.ioStart = DC4_REG_START;
        slot.ioEnd = DC4_REG_END;
        slot.id = DC4_MULTIBUS_SLOT;

        multibus->AddSlotMapping(slot);

        extensionDC4 = new CoherentExtensionDC4(this);
        Coherent::RegisterExtension(extensionDC4);

        dc4Channel = LogChannel(DC4_LOG_CHANNEL_NAME, ConsoleColor::BrightCyan, ConsoleColor::White);
        Logger::AddChannel(dc4Channel);
        logDC4 = Cvar::Get("logDC4", "0");
        
        logEnabled = logDC4->GetValue();

        if (logEnabled)
            Logger::SetChannelEnabled(DC4_LOG_CHANNEL_NAME);
    }

    uint16_t DC4::Read16(size_t addr)
    {
        uint16_t ret = 0x00;
        switch (addr)
        {
            case DC4_REG_FLAGS:
                ret = flags;
                break; 
            case DC4_REG_COLOURMAP_START ... DC4_REG_COLOURMAP_END:
                /*
                    The map reads back. getmcolor() is how the GL library finds out what a colour
                    index currently holds, and blink() uses it to remember the colour it is about to
                    replace so it can put it back - so a map that only accepts writes turns every
                    blinking colour black the first time it blinks.
                */
                ret = ReadColourmap(addr);
                break;
            default:
                Logger::Log(LOG_PREFIX_DC4, std::format("UNKNOWN DC4 Read16 0x{:x} from 0x{:x}", ret, addr).c_str(), LogChannels::Warning);
                break;
        }

        Logger::Log(LOG_PREFIX_DC4, std::format("DC4 Read16 0x{:x} from 0x{:x}", ret, addr).c_str(), DC4_LOG_CHANNEL_NAME);
        
        return ret;
    }    

    void DC4::Write16(size_t addr, uint16_t value)
    {
        uint16_t ret = 0x00;
        switch (addr)
        {
            case DC4_REG_FLAGS:
                flags = value;
                break; 
            case DC4_REG_COLOURMAP_START ... DC4_REG_COLOURMAP_END:
                UpdateColourmap(addr, value);
                break; 
            default:
                Logger::Log(LOG_PREFIX_DC4, std::format("UNKNOWN DC4 Write16 0x{:x} to 0x{:x}", value, addr).c_str(), LogChannels::Warning);
                break;
        }

        Logger::Log(LOG_PREFIX_DC4, std::format("DC4 Write16 0x{:x} to 0x{:x}", value, addr).c_str(), DC4_LOG_CHANNEL_NAME);
    }

    /*
        Which entry an address in the map window means. Reads and writes have to agree on this, so it
        is worked out in one place: the red, green and blue banks are three consecutive 256 entry
        blocks, and in multimap mode the flags register picks which of the sixteen maps they belong
        to. Returns -1 for an address outside the RAM rather than clamping, because a bad index is a
        decode that has gone wrong and silently writing entry 0 hides it.
    */
    int32_t DC4::ColourmapIndex(size_t addr)
    {
        /*
            Which bank a write lands in is always DCflags bits 0 to 3, in both modes: in multimap the
            driver has already selected the map it wants, and in single map it puts the colour's top
            four bits there itself before each write. Reading that from the flags either way is what
            keeps this in step with the lookup in Render, which is the whole point of the two of them
            agreeing on one calculation.
        */
        uint32_t index = GetMultimapAddress(addr);

        if (index >= (DC4_COLOUR_RAM_SIZE >> 1))
            return -1;

        return (int32_t)index;
    }

    uint16_t DC4::ReadColourmap(size_t addr)
    {
        int32_t index = ColourmapIndex(addr);

        if (index < 0)
        {
            Logger::Log(LOG_PREFIX_DC4, std::format("DC4: Invalid colourmap colour index for read from 0x{:x}", addr).c_str(), LogChannels::Warning);
            return 0;
        }

        return colourMap[index];
    }

    void DC4::UpdateColourmap(size_t addr, uint16_t value)
    {
        // 6 bytes per colour entrry (16 bit for ??????????)
        int32_t index = ColourmapIndex(addr);

        if (index < 0)
        {
            Logger::Log(LOG_PREFIX_DC4, std::format("DC4: Invalid colourmap colour index (byte 0x{:x})", addr).c_str(), LogChannels::Warning);
            return;
        }

        // *16 bit* map index
        colourMap[index] = value;
    }

    void DC4::Render(RenderTexture* screen)
    {
        // rgb mode
        if (flags & DC4_FLAG_RGB_MODE)
        {
            // if its rgb mode, just slam it in for now. we can deeal with the other 8 bits later.
            // NOTE: The screen is 1024*1024 but only the top 768 lines are shown nso we can do this
            memcpy(screen->GetPixels(), vram->GetPixels(), (DC4_SCREEN_SIZE_X * DC4_SCREEN_SIZE_Y) << 2); // always 4mb
        }
        else // slow path
        {
            bool isMultimap = (flags & DC4_FLAG_REG_ADDRMAP);
            uint32_t vramAddress = 0;

            /*
                VRAM address 0 is the bottom left of the screen - GL puts the origin at the bottom -
                so walking up through VRAM comes down the texture. The last row is SIZE_Y - 1:
                starting at SIZE_Y painted one row more than the screen has and pushed the whole
                picture down by one. The texture is 1024 tall and only 768 rows are shown, so the
                extra row landed inside the allocation rather than past it, which is why this was
                only ever a wrong image and not a crash - and SetPixel would not have caught it
                either, since its bounds check is a MOTION_ASSERT and that is compiled out of
                everything but Debug.
            */
            for (int y = DC4_SCREEN_SIZE_Y - 1; y >= 0; y--)
            {
                for (int x = 0; x < DC4_SCREEN_SIZE_X; x++)
                {
                    // get our palette address
                    uint32_t paletteValue = vram->Read32(vramAddress) & 0xFFF;

                    // Advance after the read: incrementing first skips pixel 0 and shifts every pixel one place.
                    vramAddress += 4;

                    /*
                        The RAM is sixteen banks of 256 entries, three components each, and the
                        index into a bank is only ever the bottom eight bits of the pixel - the
                        driver says so twice over, masking with DCMULTIMASK before it works out a
                        RAM address either way. What differs is where the bank comes from:

                          multimap   sixteen independent 256 entry maps, and the current one -
                                     DCflags bits 0 to 3 - picks which
                          single     one flat 4096 entry map living in those same sixteen banks, so
                                     the *pixel's* top four bits pick which, and gl_domapcolors
                                     writes it that way too: "DCflags = gl_dcr | DCBUSOP |
                                     DCIndexToReg(index); index &= DCMULTIMASK"

                        Indexing the RAM with the whole twelve bit pixel instead, which is what this
                        did, reaches entries nothing ever writes as soon as a colour goes past 255 -
                        so the picture was right only by virtue of nothing having used a high colour
                        yet. mex's cursor lives at 1024, in the overlay planes, and came out black.
                    */
                    uint32_t index = paletteValue & DC4_COLOURMAP_INDEX_MASK;
                    uint32_t bank = isMultimap ? (flags & DC4_COLOURMAP_BANK_MASK)
                                               : ((paletteValue >> 8) & DC4_COLOURMAP_BANK_MASK);
                    uint32_t base = bank * DC4_COLOURMAP_BANK_STRIDE;

                    // technically RGB161616 but treated as RGB 888. Huh.
                    uint32_t colour = ((colourMap[base + index] & 0xFF))
                        | ((colourMap[base + 256 + index] & 0xFF) << 8) & 0x0000FF00
                        | ((colourMap[base + 512 + index] & 0xFF) << 16) & 0x00FF0000
                        | (0xFF << 24) & 0xFF000000; // this is the alpha

                    screen->SetPixel(x, y, colour);
                }
            }

        }
    }
    
    void DC4::Shutdown()
    {
        delete extensionDC4;
    }
};