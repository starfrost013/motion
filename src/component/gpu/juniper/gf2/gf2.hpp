/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    gf2.hpp: Silicon Graphics graphics interface / frame buffer controller version 2.

    GF2 is the board the CPU actually talks to. It carries the frame buffer controller the host
    drives through four Multibus I/O registers, and it fronts the geometry pipe, which lives in its
    own CPU segment rather than on the backplane.

    IRIX decides whether this machine has a graphics console by trying to reset this board inside a
    nofault region (con_init -> gr_init). Every access below therefore has to either answer or bus
    error deliberately - answering wrongly turns the clean fallback to the serial console into a
    hang, which is strictly worse.

    What is actually behind these four registers:

      - 14 custom geometry engine chips, each four 32-bit ALUs with a microcode store and a config
        register that selects which function the chip performs. They are wired as a pipeline: the
        first is a fifo converting IEEE 754 to 20.8 fixed point, the next four are a 4x4 matrix
        multiplier, six do clipping and z-buffering, two more scale two coordinates each, and the
        last converts back to IEEE 754. gefind() probes twelve of them by sending an illegal
        command and watching for a per-chip trap bit.
      - the pipeline output feeds four AMD Am2903 bitslice processors running in parallel for a
        16-bit datapath - this is the FBC, and it is what does flat and Gouraud shading. Those four
        slices are exactly why the microcode is declared "unsigned short ucode[][4]" and why
        Micro_Write walks wd = 0..3: one slice each.
      - the FBC reaches VRAM through the bitplane controller, BPC, which has its own command set.
        UC3 and DC3 went through the BPC too, but UC4 and DC4 do not, so their side of VRAM can be
        modelled as plain writes.

    On the IP2 the pipe is not on the backplane at all: it hangs off a private bus at segment 6,
    plus parts of Multibus I/O, and the address never moves.

    Note what GF2 is NOT, because the previous generation is a trap for anyone reading GL1 code or
    MAME's sgi_gl1_device as a reference. On GL1 the host reaches UC3 and DC3 *through* the FBC on
    the GF1, which is why that device is emulated as one lump of GF1+UC3+DC3+BP2 with a command
    fifo and a per-opcode parameter count table hanging off the FBC data port. On GL2 the update
    and display controllers have their own Multibus I/O addresses instead - UC4 at 0x50003000 and
    DC4 at 0x50004000, both already emulated separately - so nothing here should be routing drawing
    commands to them. What is left on this side of the FBC is the geometry pipe and the frame
    buffer controller proper.
*/

#pragma once
#include <component/addrspace.hpp>
#include <component/component.hpp>
#include <component/multibus/multibus.hpp>

namespace Motion
{
    /*
        The register block is selected by address bits A12-A19 ("board decode" in gfdev.h) and the
        register within it by A10-A11. GF1 decodes 1<<12 and GF2 decodes 2<<12, which is the only
        difference between the two maps - the offsets inside the block are identical:

            READPIXEL 0       FBCTL (1<<10)     FBDATA (2<<10)    GECTL (3<<10)
    */
    #define GF2_REG_START               0x50002000
    #define GF2_REG_END                 0x50002FFF

    #define GF2_REG_PIXEL               0x50002000  // read: pixel readback. write: clear FBC interrupt
    #define GF2_REG_FBCFLAGS            0x50002400
    #define GF2_REG_FBCDATA             0x50002800
    #define GF2_REG_GEFLAGS             0x50002C00

    #define GF2_MULTIBUS_SLOT           18

    /*
        The geometry pipe is not on the Multibus at all - it is segment 6, straight off the CPU, and
        the MMU passes it through untranslated. The textport driver writes 32-bit command words
        followed by 16-bit data to _GEPORT_.
    */
    #define GF2_GE_SEGMENT_START        0x60000000
    #define GF2_GE_SEGMENT_END          0x6FFFFFFF
    #define GF2_GE_TOKEN                0x60000000
    #define GF2_GE_PORT                 0x60001000

    /*
        GL2's constants are not GL1's - RUNMODE is 0x31 here and 1 there, and GEflags grew from 8
        bits to 16 to carry the microcode addressing. These come from gl2/gl2/include/gf2.h.
    */

    // FBCflags read bits. A trailing underscore is SGI's notation for active low.
    #define GF2_FBC_BPCACK_BIT          0x800   // BPC ACK FBC
    #define GF2_FBC_FBCACK_BIT          0x400   // FBC ACK GE
    #define GF2_FBC_GET_BIT             0x100   // FBC needs input
    #define GF2_FBC_NEWVERT_BIT_        0x80
    #define GF2_FBC_VERTINT_BIT         0x40
    #define GF2_FBC_TOKEN_BIT_          0x20    // GE port token flag; PIPEISBUSY tests this
    #define GF2_FBC_INTERRUPT_BIT_      0x10    // FBC programmed interrupt

    // FBCflags write values.
    #define GF2_FBC_RUNMODE             0x31    // normal operation
    #define GF2_FBC_READOUTRUN          0x32    // spy on the output register
    #define GF2_FBC_RUNDEBUG            0xF3
    #define GF2_FBC_STARTDEV            0xF0
    #define GF2_FBC_WRITEMICRO          0xFE
    #define GF2_FBC_READMICRO           0xFF

    /*
        FBCdata doubles as a command port: written under a debug mode it asks the controller a
        question, and reading it back under READOUTRUN returns the answer. fbc_reset() asks 8 and
        insists on 0xfff or 0x7ff, then asks 7 and insists the top byte of the reply is 0x02.
    */
    #define GF2_FBC_CMD_SCRATCH_SIZE    8
    #define GF2_FBC_CMD_MICRO_VERSION   7
    #define GF2_FBC_SCRATCH_SIZE        0xFFF   // 4K of scratch RAM, reported as size - 1
    #define GF2_FBC_MICRO_VERSION       0x0200  // major 2, minor 0

    // GEflags read bits.
    #define GF2_GE_LOWATER_BIT          0x0001
    #define GF2_GE_TRAPINT_BIT          0x0002
    #define GF2_GE_FIFOINT_BIT          0x0004
    #define GF2_GE_HIWATER_BIT          0x8000  // gewait() spins while this is set

    // GEflags write values and the fields the microcode addressing packs into it.
    #define GF2_GE_RESET1               0x843F
    #define GF2_GE_RESET3               0x8036
    #define GF2_GE_DEBUG                0x813E
    #define GF2_GE_RUNMODE              0x843E
    #define GF2_GE_MICROACCESS_BIT_     0x8000  // clear to enable microcode read/write
    #define GF2_GE_MICRO_MSB_MASK       0x1C00  // (state << 1) & 0x1c00, i.e. state bits 9-11
    #define GF2_GE_MICRO_SLICE_SHIFT    13      // slice 0-3 at bits 13-14

    // The microcode store: 4096 states of four 16-bit slices, the top slice only 8 bits wide.
    #define GF2_MICRO_STATES            4096
    #define GF2_MICRO_SLICES            4
    #define GF2_MICRO_SLICE3_MASK       0x00FF
    #define GF2_MICRO_WINDOW_MASK       0x3FE   // FBCucode offset, (state & 0x1ff) << 1

    #define LOG_PREFIX_GF2              "GF2"
    #define GF2_LOG_CHANNEL_NAME        "GF2"

    // A command stream we cannot execute yet would otherwise fill the log on its own.
    #define GF2_MAX_GE_LOGGED           64

    extern Cvar* logGF2;
    extern Cvar* enableGF2;

    class GF2 : public Component
    {
    public:
        void Start() override;
        void Shutdown() override;

        const char* GetName() override { return "GPU GF2 board (Graphics Interface v2)"; };

        uint8_t Read8(size_t addr) override;
        uint16_t Read16(size_t addr) override;
        uint32_t Read32(size_t addr) override;
        void Write8(size_t addr, uint8_t value) override;
        void Write16(size_t addr, uint16_t value) override;
        void Write32(size_t addr, uint32_t value) override;

    private:
        uint16_t ReadFBCData();
        void WriteGE(size_t addr, uint32_t value, int32_t width);

        // Unpack the microcode address the guest split across GEflags and the window offset.
        int32_t MicroState(size_t addr) { return (int32_t)(((geFlagsWritten & GF2_GE_MICRO_MSB_MASK) >> 1)
            | (((addr - GF2_REG_FBCDATA) & GF2_MICRO_WINDOW_MASK) >> 1)); };
        int32_t MicroSlice() { return (geFlagsWritten >> GF2_GE_MICRO_SLICE_SHIFT) & 0x3; };
        bool MicroAccessEnabled() { return (geFlagsWritten & GF2_GE_MICROACCESS_BIT_) == 0; };

        uint16_t fbcFlagsWritten = 0;
        uint16_t geFlagsWritten = 0;
        uint16_t fbcCommand = 0;
        bool fbcInterruptPending = false;

        /*
            Micro_Write writes all 4096 states four slices deep and then reads every one of them
            back and compares, returning the first address that differs. Storing the microcode is
            therefore not optional - a controller that accepts writes and reads back zero fails the
            verify pass and IRIX gives up with "micro write error".
        */
        uint16_t microcode[GF2_MICRO_STATES][GF2_MICRO_SLICES] = {};

        int32_t geWordsLogged = 0;
        LogChannel gf2Channel;
        Multibus* multibus = nullptr;
        bool logEnabled = false;
    };
};
