/* motion - The SGI Emulator. Copyright (c)2026 danifunker. gf2.hpp: Silicon Graphics graphics interface / frame buffer controller version 2. */

#pragma once
#include <component/addrspace.hpp>
#include <component/component.hpp>
#include <component/multibus/multibus.hpp>
#include <component/gpu/juniper/bp3/bp3.hpp>

namespace Motion
{
    // The register block is selected by address bits A12-A19 ("board decode" in gfdev.h) and the register within it by A10-A11.
    #define GF2_REG_START               0x50002000
    #define GF2_REG_END                 0x50002FFF

    #define GF2_REG_PIXEL               0x50002000  // read: pixel readback. write: clear FBC interrupt
    #define GF2_REG_FBCFLAGS            0x50002400
    #define GF2_REG_FBCDATA             0x50002800
    #define GF2_REG_GEFLAGS             0x50002C00

    #define GF2_MULTIBUS_SLOT           18

    // The board's interrupt line.
    #define GF2_MULTIBUS_IRQ            3

    // The display is 60 fields a second, interlaced.
    #define GF2_RETRACE_HZ_DEFAULT      "60"

    // The geometry pipe is not on the Multibus at all - it is segment 6, straight off the CPU, and the MMU passes it through untranslated.
    #define GF2_GE_SEGMENT_START        0x60000000
    #define GF2_GE_SEGMENT_END          0x6FFFFFFF
    #define GF2_GE_TOKEN                0x60000000
    #define GF2_GE_PORT                 0x60001000

    // GL2's constants are not GL1's - RUNMODE is 0x31 here and 1 there, and GEflags grew from 8 bits to 16 to carry the microcode addressing.

    // FBCflags read bits. A trailing underscore is SGI's notation for active low.
    #define GF2_FBC_BPCACK_BIT          0x800   // BPC ACK FBC
    #define GF2_FBC_FBCACK_BIT          0x400   // FBC ACK GE
    #define GF2_FBC_GET_BIT             0x100   // FBC needs input
    #define GF2_FBC_NEWVERT_BIT_        0x80
    #define GF2_FBC_VERTINT_BIT         0x40
    #define GF2_FBC_TOKEN_BIT_          0x20    // GE port token flag; PIPEISBUSY tests this
    #define GF2_FBC_INTERRUPT_BIT_      0x10    // FBC programmed interrupt

    // FBCflags write bits, and the combinations of them the driver has names for.
    #define GF2_FBC_HOSTFLAG            0x04

    #define GF2_FBC_RUNMODE             0x31    // normal operation
    #define GF2_FBC_READOUTRUN          0x32    // spy on the output register
    #define GF2_FBC_RUNDEBUG            0xF3
    #define GF2_FBC_STARTDEV            0xF0
    #define GF2_FBC_WRITEMICRO          0xFE
    #define GF2_FBC_READMICRO           0xFF

    // FBCdata doubles as a command port: written under a debug mode it asks the controller a question, and reading it back under READOUTRUN returns the.
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

    // The command stream.
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

    // Three FBC commands have no fixed length.
    #define GF2_FBC_PARAMS_REST         (-2)
    #define GF2_FBC_PARAMS_UNKNOWN      (-1)

    // The font RAM, which lives on the bitplane controller.
    #define GF2_FBC_FONTRAM_WORDS       16384

    // Programmed interrupt codes the microcode raises to say what kind of answer is waiting in the readback FIFO. gl2cmds.h calls these _INT*.
    #define GF2_FBC_INT_EOF             9       // _INTEOF, the end of a command batch
    #define GF2_FBC_INT_PIXEL32         10      // _INTPIXEL32, a 32 bit pixel readback
    #define GF2_FBC_INT_CURSOR          19      // _INTCURSOR, "cursor signal received; want Y"
    #define GF2_FBC_INT_CHPOSN          7       // _INTCHPOSN, the current character position
    #define GF2_FBC_INT_FEEDBACK        20      // _INTFEEDBACK, a feedback buffer full of results

    // The readback FIFO and the pipe's reverse channel: a result the microcode leaves behind when it raises the programmed interrupt.
    #define GF2_FBC_EOF1                0x0108
    #define GF2_FBC_EOF2                0x0084
    #define GF2_FBC_EOF3                0x0042

    // One GEstoremm is a struct matdata: the opcode as a token, then a 4x4 matrix of floats.
    #define GF2_GE_STOREMM_WORDS        33
    #define GF2_FBC_FEEDBACK_MAX        (GF2_GE_MATRIX_STACK_DEPTH * GF2_GE_STOREMM_WORDS + 8)

    #define GF2_FBC_READBACK_MAX        GF2_FBC_FEEDBACK_MAX
    #define GF2_GE_AUTOCLEAR_BIT        0x800   // "enable FBC int clr after rd"

    // FBC scratch RAM: 4K words, holding the swizzle, mask and divide tables the microcode indexes.
    #define GF2_FBC_SCRATCH_WORDS       4096

    // The transform.
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

    // A character in the stream is four words - a struct fontchar, which the driver hands over whole because struct ufontchar is declared to overlap it as.
    #define GF2_FBC_CHAR_WORDS          4

    // The cursor.
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

    // Matrix concatenation: sixteen opcodes, 0x20 to 0x2f, that between them build a 4x4 matrix a row at a time and multiply it onto the top of the stack.
    #define GF2_GE_OP_CONCAT_FIRST      0x20
    #define GF2_GE_OP_CONCAT_LAST       0x2F
    #define GF2_GE_CONCAT_ROW_MASK      0x03
    #define GF2_GE_CONCAT_KIND_SHIFT    2
    #define GF2_GE_CONCAT_MID           0
    #define GF2_GE_CONCAT_FIRST_ROW     1
    #define GF2_GE_CONCAT_LAST_ROW      2
    #define GF2_GE_CONCAT_COMPLETE      3

    // A hair less than the 20.8 fixed point the pipe works in, so nudging a sample by it cannot move it onto a coordinate the geometry engine could have.
    #define GF2_FILL_EPSILON            (1.0f / 256.0f)
    #define GF2_GE_OP_MOVEPOLY          0x30
    #define GF2_GE_OP_DRAWPOLY          0x31
    #define GF2_GE_OP_CLOSEPOLY         0x33

    #define LOG_PREFIX_GF2              "GF2"
    #define GF2_LOG_CHANNEL_NAME        "GF2"

    // A command stream we cannot execute yet would otherwise fill the log on its own.
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

        // The vertical retrace.
        uint64_t lastRetraceNs = 0;
        uint32_t retraceHz = 0;
        bool verticalPending = false;

        // A cursor interrupt the host has asked for with HOSTFLAG but that has not been raised yet because another programmed interrupt still owns the readback.
        bool cursorRequested = false;

        // HOSTFLAG as the host last set it, ignoring the spy mode writes that pass through it.
        bool hostFlagSet = false;

        // What the Multibus line is currently being driven to, so it is only changed on an edge.
        bool irqAsserted = false;

        // Micro_Write writes all 4096 states four slices deep and then reads every one of them back and compares, returning the first address that differs.
        uint16_t microcode[GF2_MICRO_STATES][GF2_MICRO_SLICES] = {};

        // Command stream state.
        int32_t gePassthruLeft = 0;
        int32_t geBodyLength = 0;
        uint16_t geBody[256] = {};

        // Transform state.
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

        // The character position.
        float charPositionX = 0.0f, charPositionY = 0.0f;
        bool charPositionPending = false;

        // The cursor, and the pixels it is currently sitting on top of.
        int32_t cursorGlyph = 0;
        uint16_t cursorColour = 0;
        uint16_t cursorWriteMask = 0;
        int32_t cursorX = 0, cursorY = 0;
        int32_t cursorPendingX = 0;
        bool cursorDrawn = false;

        // The mask in force when it was drawn, so it is put back exactly as far as it reached.
        uint16_t cursorDrawnMask = 0;

        // The cursor half of +set logMouse.
        bool cursorTrace = false;
        int32_t cursorTraced = 0;
        uint32_t cursorSaved[GF2_CURSOR_SIZE][GF2_CURSOR_SIZE] = {};

        // The offset the FBC adds to a character's font RAM address. Not the cursor's - that is absolute.
        uint16_t fontRamBase = 0;

        // Feedback in progress, and the buffer it is filling.
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
