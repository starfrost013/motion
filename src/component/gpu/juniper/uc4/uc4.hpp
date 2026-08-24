/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    uc4.hpp: Silicon Graphics update controller version 4.

    This part of the graphics system receives input from the GF2 (Graphics & Framebuffer 2) board and performs various 2D graphics actions such as stipple alpha,
    DDA-type lines and text (from a FONT rom). It uses a similar command interface to the FBC and GE on the GF2 board. It's required to perform all graphics actions.
    
*/
#pragma once

#include <component/addrspace.hpp>
#include <component/component.hpp>
#include <component/multibus/multibus.hpp>
#include <component/gpu/vram.hpp>

namespace Motion
{
    // Registers

    #define UC4_REG_START           0x50003000

    // (buffer_id<<1)
    #define UC4_REG_BUFFER_IO       0x50003080
    #define UC4_REG_UCR             0x50003180

    #define UC4_UCR_BOARDENAB	    (1 << 8)	    // read/write
    #define UC4_UCR_MBENAB	        (1 << 9)	    // read/write
    #define UC4_UCR_INTRENAB	    (1 << 10)	    // read/write
    #define UC4_UCR_DMAENAB	        (1 << 11)	    // read/write **** NOT USED ****
    #define UC4_UCR_ZERO	        (1 << 12)	    // read only 
    #define UC4_UCR_VERTICAL	    (1 << 13)	    // read only 
    #define UC4_UCR_VERTINTR	    (1 << 14)	    // read only **** vblank INTERUPT ****
    #define UC4_UCR_BUSY	        (1 << 15)	    // read only

    #define UC4_REG_COMMAND         0x50003200

    #define UC4_REG_END             0x50003fff
    #define UC4_MULTIBUS_SLOT       18              // shown as 19

    // All "drawline" commands are Bresenham segment type acceleration
    // The basic calculations are done by the GL2 and the UC does the task of actually plotting it.
    #define UC4_CMD_READFONT	    0x00
    #define UC4_CMD_WRITEFONT	    0x01
    #define UC4_CMD_READREPEAT	    0x02
    #define UC4_CMD_SETADDRS	    0x03
    #define UC4_CMD_SAVEWORD	    0x04
    #define UC4_CMD_DRAWWORD	    0x05
    #define UC4_CMD_READLSTIP	    0x06
    #define UC4_CMD_NOOP		    0x07
    #define UC4_CMD_DRAWCHAR	    0x09
    #define UC4_CMD_FILLRECT	    0x0A
    #define UC4_CMD_FILLTRAP	    0x0B
    #define UC4_CMD_DRAWLINE1	    0x0C
    #define UC4_CMD_DRAWLINE2	    0x0D
    #define UC4_CMD_DRAWLINE4	    0x0E
    #define UC4_CMD_DRAWLINE5	    0x0F
    #define UC4_CMD_SETSCRMASKX	    0x10
    #define UC4_CMD_SETSCRMASKY	    0x11
    #define UC4_CMD_SETCOLORCD	    0x14
    #define UC4_CMD_SETCOLORAB	    0x15
    #define UC4_CMD_SETWECD	        0x16
    #define UC4_CMD_SETWEAB	        0x17
    #define UC4_CMD_READPIXELCD	    0x18
    #define UC4_CMD_READPIXELAB	    0x19
    #define UC4_CMD_DRAWPIXELCD	    0x1A
    #define UC4_CMD_DRAWPIXELAB	    0x1B
    #define UC4_CMD_DRAWLINE11	    0x1C
    #define UC4_CMD_DRAWLINE10	    0x1D
    #define UC4_CMD_DRAWLINE8	    0x1E
    #define UC4_CMD_DRAWLINE7	    0x1F

    #define UC4_CMD_LAST            UC4_CMD_DRAWLINE7

    // *** Buffers *** (use UC4_CMD_SAVEWORD and UC4_CMD_LOADWORD to access)
    // All buffers except DDA* ar e write only
    #define UC4_BUFFER_EDB          0x01
    #define UC4_BUFFER_ECB          0x02
    #define UC4_BUFFER_XSB          0x03    // start of current x coord [xy logic]
    #define UC4_BUFFER_XEB          0x04    // end of current x coord [xy logic]
    #define UC4_BUFFER_YSB          0x05    // start of current y coord [xy logic]
    #define UC4_BUFFER_YEB          0x06    // end of current y coord [xy logic]
    #define UC4_BUFFER_FMAB         0x07
    #define UC4_BUFFER_DDASAF       0x08
    #define UC4_BUFFER_DDASAI       0x09
    #define UC4_BUFFER_DDAEAF       0x0A
    #define UC4_BUFFER_DDAEAI       0x0B
    #define UC4_BUFFER_DDASDF       0x0C
    #define UC4_BUFFER_DDASDI       0x0D
    #define UC4_BUFFER_DDAEDF       0x0E
    #define UC4_BUFFER_DDAEDI       0x0F
    #define UC4_BUFFER_MDB          0x10    // mode register
    #define UC4_BUFFER_RPB          0x11    // repeat register
    #define UC4_BUFFER_CFB          0x12    // config

    #define UC4_MODE_SWIZZLE        (1 << 0)
    #define UC4_MODE_DOUBLE_BUFFER  (1 << 1)
    #define UC4_MODE_DEPTH_CUE      (1 << 2)
    #define UC4_MODE_MB_SETADDR     (1 << 5)

    #define UC4_CFG_DISP_A              (1 << 0)
    #define UC4_CFG_DISP_B              (1 << 1)
    #define UC4_CFG_UPDATE_A            (1 << 2)
    #define UC4_CFG_UPDATE_B            (1 << 3)
    #define UC4_CFG_SCREEN_MASK         (1 << 4)
    #define UC4_CFG_INVERT              (1 << 5)
    #define UC4_CFG_FINISH_LINE         (1 << 6)
    #define UC4_CFG_LOAD_LINE_STIPPLE   (1 << 7)
    #define UC4_CFG_PFICD               (1 << 8)
    #define UC4_CFG_PFIREAD             (1 << 9)
    #define UC4_CFG_PFICOLUMN           (1 << 10)
    #define UC4_CFG_PFIXDOWN            (1 << 11)
    #define UC4_CFG_PFIYDOWN            (1 << 12)
    #define UC4_CFG_ALLPATTERN          (1 << 13)
    #define UC4_CFG_PATTERN32           (1 << 14)
    #define UC4_CFG_PATTERN64           (1 << 15)

    // buffersa re in the 50003080...500030ff space 
    #define UC4_BUFFER_TO_ADDR(x)   (UC4_REG_BUFFER_IO | (x<<1))
    #define UC4_ADDR_TO_BUFFER(x)   (addr - UC4_REG_BUFFER_IO) >> 1

    #define UC4_COMMAND_TO_ADDR(x)  (UC4_REG_COMMAND | (x<<1))
    #define UC4_ADDR_TO_COMMAND(x)  (addr - UC4_REG_COMMAND) >> 1

    #define UC4_NUM_FRAMEBUFFERS    2

    // fun fact there is a dma functionality but it is not used by anything

    #define UC4_FONT_ROM_SIZE       0x10000

    extern Cvar* logUC4;
    #define LOG_PREFIX_UC4          "UC4"
    #define UC4_LOG_CHANNEL_NAME    LOG_PREFIX_UC4

    // As it turns out this is a very common operation because of how the write enable code works.
    #define APPLY_WE_CODE(newV, orig, mask) (orig & ~mask) | (newV & mask)

    // the coherent extension
    class CoherentExtensionUC4 : public CoherentExtension
    {
    public: 
        CoherentExtensionUC4(Component* component) : CoherentExtension(component) { };

        void AddUI() override;
    }; 

    class UC4 : public Component
    {
        friend class CoherentExtensionUC4;

    public: 
        void Start() override;
        void Shutdown() override;
        
        // Register I/O
        // bus is 16 bit hopefully
        uint8_t Read8(size_t addr) override;
        uint16_t Read16(size_t addr) override;
        void Write16(size_t addr, uint16_t value) override;

        const char* GetName() override { return "GPU UC4 board (Update Controller v4)"; }; 

    private: 
        // Fields

        Multibus* multibus;
        ComponentVRAM* vram; 

        uint8_t fontRom[UC4_FONT_ROM_SIZE];

        // registers
        uint16_t scrMaskX = 0, scrMaskY = 0;
        uint16_t ucr; // update controller reset register?

        /// @brief UCR with its read-only status bits filled in.
        uint16_t ReadUCR();

        CoherentExtensionUC4* extensionUC4;

        // the buffers

        uint16_t edb = 0;
        uint16_t ecb = 0;
        uint16_t xsb = 0;       // current x position
        uint16_t xeb = 0;
        uint16_t ysb = 0;       // current y position
        uint16_t yeb = 0;
        uint16_t fmab = 0;

        // digital differential analyser stuff
        uint16_t ddasaf = 0, ddasai = 0;
        uint16_t ddaeaf = 0, ddaeai = 0;
        uint16_t ddasdf = 0, ddasdi = 0;
        uint16_t ddaedf = 0, ddaedi = 0;

        uint16_t mode = 0;
        uint16_t repeat = 0;
        uint16_t config = 0;

        // command stuff

        // there are two buffers
        struct Buffer
        {
            // figure out what is actually oging here
            uint8_t dummy;
        };

        uint32_t colorcode;         // The current colour to render with.
        uint32_t wecode;            // a mask (basically how many bp's there are?)

        Buffer buffers[UC4_NUM_FRAMEBUFFERS] = {0};

        // Methods
        uint16_t ReadBuffer(size_t addr);
        void WriteBuffer(size_t addr, uint16_t value);
        void ParseCommand(size_t addr, uint16_t value);
        
        LogChannel uc4Channel;
        bool logEnabled;
    }; 
}; 