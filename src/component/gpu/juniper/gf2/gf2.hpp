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
#include <component/gpu/juniper/bp3/bp3.hpp>

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
        The board's interrupt line. con_init() patches ivectors[3] with fbc_intr and ivectors[4] with
        ge_intr the moment gr_init() returns - sys/ipII/console.c, and the shipped kernel agrees:
        _initvectors writes _fbc_intr into _ivectors+0xc. ivectors[n] is what Xmbintr<n> dispatches
        to, so both halves of the frame buffer controller's interrupt, the vertical retrace and the
        programmed interrupt, arrive on Multibus line 3.

        Patching them there rather than during autoconfiguration is what makes this safe to assert at
        all: ivectors[3] starts out as default_intr, which panics, and the vertical interrupt is
        enabled by the fbcreset() a few lines earlier. The vectors are in place before main() reaches
        its spl0(), so the first one the CPU is ever allowed to see already has a handler.

        Line 4 is not driven. ge_intr services the GE FIFO high water mark and the instruction trap
        bits, neither of which the emulated pipe reaches - commands here complete as they arrive.
    */
    #define GF2_MULTIBUS_IRQ            3

    /*
        The display is 60 fields a second, interlaced. retrace.c: "Although the FBC gets these
        interrupts 60 times a second, we are only interested in every other one since the display
        refresh is interlaced" - the halving is the driver's business, not the board's.

        A cvar because the retrace is the emulator's only free running interrupt and being able to
        turn it off is the quickest way to tell a fault in the retrace path from one behind it.
    */
    #define GF2_RETRACE_HZ_DEFAULT      "60"

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

    /*
        FBCflags write bits, and the combinations of them the driver has names for. HOSTFLAG is the
        host asking the controller to raise a programmed interrupt when it next reaches its command
        dispatch state, which is how the cursor gets moved without stopping the pipe.
    */
    #define GF2_FBC_HOSTFLAG            0x04

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
    #define GF2_GE_ENABVERTINT_BIT_     0x0020  // clear to let the vertical retrace interrupt out
    #define GF2_GE_ENABFBCINT_BIT_      0x0400  // clear to let the programmed interrupt out
    #define GF2_GE_MICROACCESS_BIT_     0x8000  // clear to enable microcode read/write
    #define GF2_GE_MICRO_MSB_MASK       0x1C00  // (state << 1) & 0x1c00, i.e. state bits 9-11
    #define GF2_GE_MICRO_SLICE_SHIFT    13      // slice 0-3 at bits 13-14

    // The microcode store: 4096 states of four 16-bit slices, the top slice only 8 bits wide.
    #define GF2_MICRO_STATES            4096
    #define GF2_MICRO_SLICES            4
    #define GF2_MICRO_SLICE3_MASK       0x00FF
    #define GF2_MICRO_WINDOW_MASK       0x3FE   // FBCucode offset, (state & 0x1ff) << 1

    /*
        The command stream. Words pushed at segment 6 go to the geometry pipe, which executes the GE
        opcodes itself and hands anything wrapped in a GEpassthru straight to the frame buffer
        controller. The passthru header carries the length, so the two instruction sets can share one
        FIFO without the host having to say which is which:

            im_passcmd(n, cmd)  ->  long  ((GEpassthru | ((n - 1) << 8)) << 16) | cmd

        n counts the FBC opcode itself, so a bare command is n = 1. n = 0 encodes as 0xFF08 because
        (0 - 1) << 8 wraps, and it means lock the pipe (written to GEPORT) or free it (to LASTGE)
        rather than starting a body.

        One body can hold several FBC commands - gl_getplaneinfo sends pixelsetup, readpixels and
        pixelsetup as a single group of eight - so the body is parsed with a per-opcode parameter
        count rather than being treated as one command.
    */
    #define GF2_GE_OP_PASSTHRU          0x08
    #define GF2_GE_PASSTHRU_FREE        0xFF08  // im_passthru(0) / im_freepipe

    // GE opcodes that carry coordinates, and the parameter format packed into bits 8-11.
    #define GF2_GEPA_DIM_MASK           0x0300
    #define GF2_GEPA_4D                 0x0000
    #define GF2_GEPA_2D                 0x0100
    #define GF2_GEPA_3D                 0x0200
    #define GF2_GEPA_TYPE_MASK          0x0C00
    #define GF2_GEPA_FLOAT              0x0000
    #define GF2_GEPA_INT                0x0400
    #define GF2_GEPA_SHORT              0x0800

    // FBC opcodes. Only the ones with behaviour behind them are named; gl2cmds.h has the rest.
    #define GF2_FBC_OP_FORCECOMPLETION  0x08
    #define GF2_FBC_OP_BASEADDRESS      0x09
    #define GF2_FBC_OP_SELECTCURSOR     0x1D
    #define GF2_FBC_OP_DRAWCURSOR       0x1E
    #define GF2_FBC_OP_UNDRAWCURSOR     0x1F
    #define GF2_FBC_OP_DRAWPIXELS       0x0D
    #define GF2_FBC_OP_READPIXELS       0x0E
    #define GF2_FBC_OP_LOADMASKS        0x17
    #define GF2_FBC_OP_FEEDBACK         0x25
    #define GF2_FBC_OP_READCHARPOSN     0x27
    #define GF2_FBC_OP_PIXELSETUP       0x2F
    #define GF2_FBC_OP_LOADRAM          0x32

    /*
        Three FBC commands have no fixed length. FBCloadram carries its own word count; FBCloadmasks
        and FBCdrawpixels do not, and take everything left in the passthru body - the host sizes the
        body to suit and sends another one if it has more to say. FBCParamCount reports them as
        GF2_FBC_PARAMS_REST so the parser knows not to look for a command after them.
    */
    #define GF2_FBC_PARAMS_REST         (-2)
    #define GF2_FBC_PARAMS_UNKNOWN      (-1)

    /*
        The font RAM, which lives on the bitplane controller. gl_loadmasks writes the stipple
        pattern, the cursor and the character bitmaps into it in blocks of up to 120 words, and the
        textport draws every character out of it. Addresses are 16 bit word addresses.
    */
    #define GF2_FBC_FONTRAM_WORDS       16384

    /*
        Programmed interrupt codes the microcode raises to say what kind of answer is waiting in the
        readback FIFO. gl2cmds.h calls these _INT*.
    */
    #define GF2_FBC_INT_EOF             9       // _INTEOF, the end of a command batch
    #define GF2_FBC_INT_PIXEL32         10      // _INTPIXEL32, a 32 bit pixel readback
    #define GF2_FBC_INT_CURSOR          19      // _INTCURSOR, "cursor signal received; want Y"
    #define GF2_FBC_INT_CHPOSN          7       // _INTCHPOSN, the current character position
    #define GF2_FBC_INT_FEEDBACK        20      // _INTFEEDBACK, a feedback buffer full of results

    /*
        The readback FIFO. When the microcode finishes something the host asked for a result from, it
        raises the programmed interrupt and leaves the answer here. The host reads the interrupt code
        by spying on the output register under READOUTRUN, then switches back to RUNMODE where
        FBCdata reads the head of this FIFO, and pops with FBCclrint (or automatically on every read
        if it set AUTOCLEAR in GEflags). The interrupt drops when the FIFO drains.
    */
    /*
        Feedback: the pipe's reverse channel, and the other half of the readback FIFO's job. FBCfeedback
        switches the controller from drawing to capturing, after which the results of the geometry
        commands that follow are appended to a buffer rather than reaching the frame buffer. Three
        magic words end it, at which point _INTFEEDBACK is raised with the whole buffer waiting.

        The three words are one long each in the driver and look like nothing in particular, except
        that FBCEOF1 is also a well formed two word passthru header - which is why the parser has to
        recognise the sequence before it tests for a passthru.

        What the host finds, in order: a token it throws away, a word count, then the buffer with the
        three end markers still on the end of it. The count includes them, which is what fbc_feedint
        means by "intct = FBCdata - 3", the three it deliberately does not count.

        This is how the kernel takes the graphics hardware away from one process and gives it to
        another: saveeverything() reads the matrix stack and the graphics position out of the pipe
        this way, and restoreeverything() pushes the next process's back in. So a window manager
        starting up cannot get as far as its first drawing command without it.
    */
    #define GF2_FBC_EOF1                0x0108
    #define GF2_FBC_EOF2                0x0084
    #define GF2_FBC_EOF3                0x0042

    /*
        One GEstoremm is a struct matdata: the opcode as a token, then a 4x4 matrix of floats. 33
        words, which is exactly the "i*33" saveeverything works its word count out from. The stack is
        MATRIXSTACKDEPTH deep, so a save can be that many matrices plus the end markers and the two
        word header, and the readback FIFO has to be able to hold all of it in one go.
    */
    #define GF2_GE_STOREMM_WORDS        33
    #define GF2_FBC_FEEDBACK_MAX        (GF2_GE_MATRIX_STACK_DEPTH * GF2_GE_STOREMM_WORDS + 8)

    #define GF2_FBC_READBACK_MAX        GF2_FBC_FEEDBACK_MAX
    #define GF2_GE_AUTOCLEAR_BIT        0x800   // "enable FBC int clr after rd"

    // FBC scratch RAM: 4K words, holding the swizzle, mask and divide tables the microcode indexes.
    #define GF2_FBC_SCRATCH_WORDS       4096

    /*
        The transform. A vertex goes through the 4x4 matrix, the perspective divide and then the
        viewport, which is the job the fourteen GE chips are pipelined to do.

        The matrix arrives transposed - the macro that loads it is called im_do_loadmatrixtrans and
        the array orthomattrans - so the translation sits in the last column and a vertex is a column
        vector: out[i] = m[i][0]*x + m[i][1]*y + m[i][2]*z + m[i][3]*w.

        The viewport is eight longs in 20.8 fixed point, a centre and a half size per axis, built as
        ((right + left) + 1) << 7 and ((right - left) + 1) << 7. Those shifts are 8 for the fixed
        point less 1 for the halving, so a 1024 wide screen gives centre and half size both 512.0,
        and normalised device coordinates land back on the pixel grid they started from.
    */
    #define GF2_GE_MATRIX_STACK_DEPTH   32
    #define GF2_GE_VIEWPORT_FRACTION    256.0f  // 20.8 fixed point
    #define GF2_GE_MAX_POLYGON_VERTICES 64

    // Screen geometry, as IRIX configures it: XMAXSCREEN 1023, YMAXSCREEN 767.
    #define GF2_SCREEN_MAX_X            1023
    #define GF2_SCREEN_MAX_Y            767

    #define GF2_FBC_OP_COLOR            0x14
    #define GF2_FBC_OP_WRTEN            0x15
    #define GF2_FBC_OP_EOF              0x26
    #define GF2_FBC_OP_CHARPOSNABS      0x1A
    #define GF2_FBC_OP_DRAWCHARS        0x1C

    /*
        A character in the stream is four words - a struct fontchar, which the driver hands over
        whole because struct ufontchar is declared to overlap it as two longs:

            offset  the glyph's address in the font RAM, already absolute; shiftfontbase() adds
                    FR_DEFFONT to every descriptor once, at startup, "for speed"
            w, h    size in pixels, one byte each
            xoff    where the glyph sits relative to the character position, one byte each
            yoff
            width   how far to advance afterwards

        The glyph itself is h consecutive words in the font RAM, one per row, the leftmost pixel in
        the top bit. The console font is 9 wide, and the low byte of every row carries a marker bit
        the font source explains as "01 added to right 8 pixels so font handling errors are
        apparent" - so w really is the width, and reading past it draws that marker.
    */
    #define GF2_FBC_CHAR_WORDS          4

    /*
        The cursor. Sixteen words in the font RAM, one per row, the leftmost pixel in the top bit and
        row 0 at the bottom - the same way round as a character, because it is loaded into the same
        RAM by the same command. The default is an arrow whose point is its top left corner, which is
        why ginit sets the hot spot to (0, 16): GL's y grows upwards, so the point sits at the top of
        the glyph and the driver subtracts the origin before handing the position over.

        The microcode saves the screen underneath before drawing and puts it back when undrawing -
        "saves 2 adjacent 16x16 pixel screen areas" into off screen VRAM. Doing that in host memory
        instead is invisible to the guest, which never reads those rows, and means the cursor cannot
        be left behind in a VRAM dump.

        The glyph address is absolute: the microcode comments it "NOTE --- cursor font ram adr is
        absolute!", so unlike a character it does not go through FBCbaseaddress.
    */
    #define GF2_CURSOR_SIZE             16

    // Enough of the cursor's comings and goings to see a gesture; it moves sixty times a second.
    #define GF2_MAX_CURSOR_LOGGED       200

    // GE opcodes with behaviour behind them; gl2cmds.h has the whole set.
    #define GF2_GE_OP_POPMM             0x00
    #define GF2_GE_OP_LOADMM            0x01
    #define GF2_GE_OP_STOREMM           0x03
    #define GF2_GE_OP_PUSHMM            0x04
    #define GF2_GE_OP_RECONFIGURE       0x0C
    #define GF2_GE_OP_LOADVIEWPORT      0x05
    #define GF2_GE_OP_POINT             0x12

    /*
        Matrix concatenation: sixteen opcodes, 0x20 to 0x2f, that between them build a 4x4 matrix a
        row at a time and multiply it onto the top of the stack. The low two bits are the row, and
        the next two say where the row sits in the sequence - mid, first, last or complete, where
        complete is a matrix of a single row. A row that is never sent keeps its identity value, and
        so does a component past the end of a short operand form: im_do_scale2 sends row 0 as two
        floats and means (x, 0, 0, 0), the last two coming from the identity.

        This is how everything except loadmatrix moves the world: translate is one GEcompletemm3
        carrying (x, y, z, 1), scale is three rows, rotate is two. mex positions its windows with it,
        which is why the textport inside a mex window draws at the screen origin without it.
    */
    #define GF2_GE_OP_CONCAT_FIRST      0x20
    #define GF2_GE_OP_CONCAT_LAST       0x2F
    #define GF2_GE_CONCAT_ROW_MASK      0x03
    #define GF2_GE_CONCAT_KIND_SHIFT    2
    #define GF2_GE_CONCAT_MID           0
    #define GF2_GE_CONCAT_FIRST_ROW     1
    #define GF2_GE_CONCAT_LAST_ROW      2
    #define GF2_GE_CONCAT_COMPLETE      3

    /*
        A hair less than the 20.8 fixed point the pipe works in, so nudging a sample by it cannot
        move it onto a coordinate the geometry engine could have produced.
    */
    #define GF2_FILL_EPSILON            (1.0f / 256.0f)
    #define GF2_GE_OP_MOVEPOLY          0x30
    #define GF2_GE_OP_DRAWPOLY          0x31
    #define GF2_GE_OP_CLOSEPOLY         0x33

    #define LOG_PREFIX_GF2              "GF2"
    #define GF2_LOG_CHANNEL_NAME        "GF2"

    /*
        A command stream we cannot execute yet would otherwise fill the log on its own. Big enough to
        reach the window manager, though: at 4096 the word log ran out during the font load, which is
        eight seconds before anything interesting happens, and a stream question that needed the raw
        words cost a rebuild to answer.
    */
    #define GF2_MAX_GE_LOGGED           100000

    extern Cvar* logGF2;
    extern Cvar* enableGF2;

    class GF2 : public Component
    {
    public:
        void Start() override;
        void Shutdown() override;
        void Tick() override;

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

        // gf2_core.cpp: the interrupt line and the one handshake that rides on top of it.
        void UpdateInterrupt();
        void ServiceCursorRequest();

        // gf2_commands.cpp: the segment 6 command stream.
        void PushGEWord(uint16_t word, bool last);
        int32_t GEParamCount(uint16_t word);
        void ExecuteGECommand();
        std::string FormatGEOperands();

        // The reverse channel: results going back to the host rather than to the frame buffer.
        void BeginFeedback();
        void AppendFeedback(uint16_t word);
        void StoreMatrixToFeedback();
        void EndFeedback();

        // gf2_geometry.cpp: the transform and the drawing it feeds.
        void ReadVertex(float* out);
        void ReadMatrixRow(int32_t row);
        void BeginConcatMatrix();
        void ConcatenateMatrix();
        void TransformVertex(const float* in, float* screenX, float* screenY);
        void AddPolygonVertex();
        void FillPolygon();
        void DrawPixel(int32_t x, int32_t y);
        void DrawPixel(int32_t x, int32_t y, uint16_t colour, uint16_t writeMask);
        void DrawCharacter(const uint16_t* descriptor);

        // gf2_geometry.cpp: the hardware cursor, which is drawn over the screen and taken back off.
        void SelectCursor(const uint16_t* words, int32_t available);
        void DrawCursor(int32_t x, int32_t y);
        void UndrawCursor();
        void MoveCursor(int32_t x, int32_t y);
        void ExecuteFBCBody();
        int32_t ExecuteFBCCommand(const uint16_t* words, int32_t available);
        int32_t FBCParamCount(uint16_t opcode);
        void RaiseProgrammedInterrupt(uint16_t code, const uint16_t* data, int32_t count);
        uint16_t PopReadback();
        uint32_t GetInstalledPlaneMask();
        void LogGEWord(size_t addr, uint32_t value, int32_t width);

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
            The vertical retrace. NEWVERT_BIT_ is a latch rather than a level: it reads clear once a
            field has ended and stays clear until the host resets it, which it does by writing
            GEflags with ENABVERTINT_BIT_ set - the Disabvert/Enabvert pair at the top of fbc_intr,
            whose comment calls it exactly that. Reporting it permanently clear, which is what a
            board with no retrace at all does, would send every interrupt down the retrace path.
        */
        uint64_t lastRetraceNs = 0;
        uint32_t retraceHz = 0;
        bool verticalPending = false;

        /*
            A cursor interrupt the host has asked for with HOSTFLAG but that has not been raised yet
            because another programmed interrupt still owns the readback FIFO. fbc_intr's comment
            allows for this - "there may be a programmed interrupt in progress, so we must service
            until we find the cursor interrupt".
        */
        bool cursorRequested = false;

        // HOSTFLAG as the host last set it, ignoring the spy mode writes that pass through it.
        bool hostFlagSet = false;

        // What the Multibus line is currently being driven to, so it is only changed on an edge.
        bool irqAsserted = false;

        /*
            Micro_Write writes all 4096 states four slices deep and then reads every one of them
            back and compares, returning the first address that differs. Storing the microcode is
            therefore not optional - a controller that accepts writes and reads back zero fails the
            verify pass and IRIX gives up with "micro write error".
        */
        uint16_t microcode[GF2_MICRO_STATES][GF2_MICRO_SLICES] = {};

        /*
            Command stream state. A passthru body is collected whole before it is executed, because a
            body can hold several FBC commands and the last one has to be able to see how many words
            are left.
        */
        int32_t gePassthruLeft = 0;
        int32_t geBodyLength = 0;
        uint16_t geBody[256] = {};

        /*
            Transform state. The matrix stack and the viewport are what the geometry commands set up,
            and the polygon is what they draw with; colour and write mask come from the FBC side, so
            a filled rectangle needs both halves of the stream to have been understood.
        */
        float matrix[GF2_GE_MATRIX_STACK_DEPTH][16] = {};
        int32_t matrixTop = 0;

        // The matrix being built a row at a time by the concatenation opcodes, before it is applied.
        float concatMatrix[16] = {};
        float viewportCentreX = 0.0f, viewportCentreY = 0.0f;
        float viewportScaleX = 0.0f, viewportScaleY = 0.0f;
        bool viewportLoaded = false;

        float polygonX[GF2_GE_MAX_POLYGON_VERTICES] = {};
        float polygonY[GF2_GE_MAX_POLYGON_VERTICES] = {};
        int32_t polygonVertices = 0;

        uint16_t fbcColour = 0;
        uint16_t fbcWriteMask = 0;

        /*
            The character position. im_cmov2i sends FBCcharposnabs on its own and then pushes a
            GEpoint through the pipe, so the position arrives transformed, from the geometry side,
            one command later - hence the pending flag rather than a parameter.
        */
        float charPositionX = 0.0f, charPositionY = 0.0f;
        bool charPositionPending = false;

        /*
            The cursor, and the pixels it is currently sitting on top of. Its position arrives by two
            routes: FBCdrawcursor carries it outright, and the retrace handler moves it without
            stopping the pipe by writing x to FBCdata, raising HOSTFLAG and handing over y when the
            cursor interrupt comes back - so the second half of that handshake is a plain register
            write that only means "cursor y" because of what is outstanding at the time.
        */
        int32_t cursorGlyph = 0;
        uint16_t cursorColour = 0;
        uint16_t cursorWriteMask = 0;
        int32_t cursorX = 0, cursorY = 0;
        int32_t cursorPendingX = 0;
        bool cursorDrawn = false;

        // The mask in force when it was drawn, so it is put back exactly as far as it reached.
        uint16_t cursorDrawnMask = 0;

        /*
            The cursor half of +set logMouse. It rides on the mouse switch rather than logGF2 because
            logGF2 traces every word in the pipe and slows the machine enough that mex has not
            finished starting by the time a test run ends - which reads exactly like a hang, and cost
            this session two runs before that was clear.
        */
        bool cursorTrace = false;
        int32_t cursorTraced = 0;
        uint32_t cursorSaved[GF2_CURSOR_SIZE][GF2_CURSOR_SIZE] = {};

        // The offset the FBC adds to a character's font RAM address. Not the cursor's - that is absolute.
        uint16_t fontRamBase = 0;

        /*
            Feedback in progress, and the buffer it is filling. The first two words are left empty for
            the token and the word count, which is only known once the end markers arrive.

            GEreconfigure is here too because it is the one geometry command whose length is decided
            by its own data: it carries a configuration byte per chip, high byte the chip number
            counting down, and ends at the first word whose high byte is 0xff. Getting that wrong is
            not a missed command but a lost parser - one of the words it carries is 0x0021, which
            reads as GEmidmm1 and would eat the eight that follow it as a matrix row.
        */
        bool feedbackMode = false;
        bool geReconfiguring = false;
        int32_t geEofWordsLeft = 0;
        int32_t feedbackLength = 0;
        uint16_t feedback[GF2_FBC_FEEDBACK_MAX] = {};

        // A geometry command being collected: the opcode word and however many operands it takes.
        uint16_t geOpcode = 0;
        int32_t geOperandsLeft = 0;
        int32_t geOperandCount = 0;
        uint16_t geOperands[64] = {};

        // The FBC's scratch RAM, written by FBCloadram and indexed by the microcode.
        uint16_t scratchRam[GF2_FBC_SCRATCH_WORDS] = {};

        // The bitplane controller's font RAM: stipple patterns, cursors and character bitmaps.
        uint16_t fontRam[GF2_FBC_FONTRAM_WORDS] = {};

        // The readback FIFO and the interrupt code that says what is in it.
        uint16_t readback[GF2_FBC_READBACK_MAX] = {};
        int32_t readbackHead = 0;
        int32_t readbackCount = 0;
        uint16_t fbcInterruptCode = 0;

        int32_t geWordsLogged = 0;
        LogChannel gf2Channel;
        Multibus* multibus = nullptr;
        ComponentVRAM* vram = nullptr;
        bool logEnabled = false;
    };
};
