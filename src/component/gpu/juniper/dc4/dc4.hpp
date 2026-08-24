/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    dc4.hpp: Silicon Graphics display controller version 4.

    This part of the graphics system receives input from the update controller UC4. It handles the actual presentation of the display
    as well as non-paletted graphics modes.
*/

#pragma once
#include <component/addrspace.hpp>
#include <component/component.hpp>
#include <component/multibus/multibus.hpp>
#include <component/gpu/vram.hpp>

namespace Motion
{
    // Registers
    #define DC4_REG_START               0x50004000
    #define DC4_REG_END                 0x500047FF
    #define DC4_REG_FLAGS               0x50004000
    #define DC4_MULTIBUS_SLOT           17

    #define LOG_PREFIX_DC4              "DC4"

    #define DC4_FLAG_REG_ADDR0          (1 << 0)  // mapping
    #define DC4_FLAG_REG_ADDR1          (1 << 1)
    #define DC4_FLAG_REG_ADDR2          (1 << 2)
    #define DC4_FLAG_REG_ADDR3          (1 << 3)
    #define DC4_FLAG_BUS_ADDRMAP        (1 << 4)
    #define DC4_FLAG_REG_ADDRMAP        (1 << 5)
    #define DC4_FLAG_RGB_MODE           (1 << 6)  // bypass colour ram, use direct rgb colour
    #define DC4_FLAG_USE_UPPER_HALF     (1 << 7)  // dc4 use upper half of doubled double-width colour map 
    #define DC4_FLAG_OPTIONAL_CLOCK     (1 << 11)
    #define DC4_FLAG_PIPELINE_DEPTH_4   (1 << 12)
    #define DC4_FLAG_PROM               (1 << 13) // different prom
    #define DC4_FLAG_FREERUN            (1 << 14) // probably diag only

    #define DC4_REG_COLOURMAP_START     0x50004200
    #define DC4_REG_COLOURMAP_END       0x500047FF

    #define DC4_REG_COLOURMAP_RED       0x50004200
    #define DC4_REG_COLOURMAP_RED_END   0x500043FF
    #define DC4_REG_COLOURMAP_GREEN     0x50004400
    #define DC4_REG_COLOURMAP_GREEN_END 0x500045FF
    #define DC4_REG_COLOURMAP_BLUE      0x50004600
    #define DC4_REG_COLOURMAP_BLUE_END  0x500047FF

    #define DC4_COLOUR_RAM_SIZE         49152

    /*
        The colour RAM is sixteen banks, each 256 entries of red, green and blue - so 768 words a
        bank. DCMULTIMASK is the driver's name for the index mask and DCIndexToReg for the shift that
        gets the bank out of a colour, which is how gl_domapcolors addresses it.
    */
    #define DC4_COLOURMAP_INDEX_MASK    0xFF
    #define DC4_COLOURMAP_BANK_MASK     0x0F
    #define DC4_COLOURMAP_BANK_STRIDE   768

    // Single map stuff
    #define DC4_SINGLEMAP_MASK          0xFFF

    // Multimap stuff
    #define DC4_MULTIMAP_NUM_MAPS       64
    #define DC4_MULTIMAP_MASK           0xFF

    #define DC4_SCREEN_SIZE_X           1024
    #define DC4_SCREEN_SIZE_Y           768

    // logging
    extern Cvar* logDC4; 
    #define DC4_LOG_CHANNEL_NAME        "DC4"

    class CoherentExtensionDC4 : public CoherentExtension
    {
    public:
        CoherentExtensionDC4(Component* owner) : CoherentExtension(owner) {}

        void AddUI() override;
    };

    class DC4 : public Component
    {
        friend class CoherentExtensionDC4;

    public: 
        void Start() override;
        void Shutdown() override;
        
        const char* GetName() override { return "GPU DC4 board (Display Controller v4)"; }; 

        // Register I/O
        // bus is 16 bit 
        uint16_t Read16(size_t addr) override;
        void Write16(size_t addr, uint16_t value) override;

        // This may be the worst interface ever invented.
        // It's only used by other Enhanced IRIS / GF2 / Juniper stuff though.

        // This is the DAC
        // TODO: Limit the refresh rate by running this on its own thread like iris.
        void Render(RenderTexture* render) override; 

    private: 
        void UpdateColourmap(size_t addr, uint16_t value);
        uint16_t ReadColourmap(size_t addr);

        /// @brief Which colour map entry an address in the map window means, or -1 if it is outside it.
        int32_t ColourmapIndex(size_t addr);

        uint16_t GetMultimapAddress(uint32_t offset) { return (((3 * (flags & 0x0F) << 8)) + ((offset - DC4_REG_COLOURMAP_START) >> 1));};

        // this is 16 bit so we index it with logical map indices not bytes as it is more logical
        uint16_t colourMap[DC4_COLOUR_RAM_SIZE >> 1]; // safe to put in BSS ????
        uint16_t flags; 
        CoherentExtensionDC4* extensionDC4; 
        ComponentVRAM* vram;
        LogChannel dc4Channel;
        Multibus* multibus;

        bool logEnabled = false;
    }; 
}; 