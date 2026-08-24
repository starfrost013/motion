/*
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    gf2_commands.cpp: the segment 6 command stream.

    Everything the host draws arrives as 16 bit words pushed at one address. The geometry pipe reads
    them as its own instruction set, except for the words wrapped in a GEpassthru, which it hands
    straight to the frame buffer controller. Splitting the two apart is the whole job of this file:
    the GE side needs a transform pipeline that does not exist yet, but the FBC side is where the
    drawing happens, and some of it - the scratch RAM tables, the pixel readback - has to answer
    correctly before IRIX will even finish bringing the board up.

    The framing rules are in gf2.hpp. The one that is easy to get wrong: a passthru body can hold
    more than one FBC command, so it is parsed with a parameter count per opcode rather than being
    treated as a single command with n-1 arguments.
*/

#include <component/gpu/juniper/gf2/gf2.hpp>

namespace Motion
{
    /*
        How many words each FBC opcode takes after itself. Derived from the im_passcmd(n, FBCxxx)
        call sites in the GL2 sources, where n counts the opcode, so the count here is n - 1. Where a
        macro appears with more than one n the smallest is the real one and the larger ones are
        groups holding several commands.

        GF2_FBC_PARAMS_UNKNOWN marks an opcode whose length this does not know. Hitting one means the
        rest of the body cannot be trusted, so the parser stops there rather than resynchronising
        onto data, which would turn a gap in this table into wrong drawing rather than a clear
        message.
    */
    int32_t GF2::FBCParamCount(uint16_t opcode)
    {
        switch (opcode & 0xFF)
        {
            case 0x00: return 0;    // FBCinitfbc
            case 0x02: return 6;    // FBCmasklist
            case 0x04: return 3;    // FBCrgbcolor
            case 0x05: return 3;    // FBCrgbwrten
            case 0x06: return 0;    // FBCsethitmode
            case 0x07: return 0;    // FBCclearhitmode
            case GF2_FBC_OP_FORCECOMPLETION: return 0;
            case 0x09: return 1;    // FBCbaseaddress
            case 0x0A: return 2;    // FBCcdcolorwe
            case GF2_FBC_OP_DRAWPIXELS: return GF2_FBC_PARAMS_REST;
            case 0x0E: return 1;    // FBCreadpixels
            case 0x12: return 2;    // FBCpoint
            case 0x13: return 4;    // FBCloadviewport
            case 0x14: return 1;    // FBCcolor
            case 0x15: return 1;    // FBCwrten
            case 0x16: return 2;    // FBCconfig
            case GF2_FBC_OP_LOADMASKS:  return GF2_FBC_PARAMS_REST;
            case GF2_FBC_OP_DRAWCHARS:  return GF2_FBC_PARAMS_REST;
            case 0x18: return 9;    // FBCselectrgbcursor
            case 0x19: return 1;    // FBClinewidth
            case GF2_FBC_OP_CHARPOSNABS: return 3;
            case 0x1D: return 7;    // FBCselectcursor
            case 0x1E: return 2;    // FBCdrawcursor
            case 0x1F: return 0;    // FBCundrawcursor
            case 0x20: return 2;    // FBClinestipple
            case 0x21: return 1;    // FBCpolystipple
            case 0x22: return 0;    // FBCsaveregs
            case 0x24: return 6;    // FBCdepthsetup
            case 0x25: return 0;    // FBCfeedback
            case GF2_FBC_OP_EOF: return 0;
            case 0x27: return 0;    // FBCreadcharposn
            case 0x28: return 3;    // FBCcopyfont
            case 0x29: return 1;    // FBCpushname
            case 0x2A: return 1;    // FBCloadname
            case 0x2B: return 0;    // FBCpopname
            case 0x2E: return 0;    // FBCinitnamestack
            case 0x2F: return 2;    // FBCpixelsetup
            case 0x34: return 2;    // FBCdrawmode
            case 0x35: return 1;    // FBCsetintensity
            case 0x36: return 1;    // FBCsetbackfacing
            case 0x39: return 4;    // FBCblockfill
            case 0x3A: return 1;    // FBCdumpram
            case 0x3D: return 5;    // FBCcopyscreen
            case 0x44: return 1;    // FBCbuffcopy

            // FBCloadram is variable length but carries its own count, so it is measured in place.
            case GF2_FBC_OP_LOADRAM: return 2;

            default: return GF2_FBC_PARAMS_UNKNOWN;
        }
    }

    /*
        gl_getplaneinfo writes one pixel with the colour and the write mask both all ones and reads
        it back, so the honest answer is whatever the fitted bitplanes can hold. Asking BP3 rather
        than returning a constant means +set numBitplanes actually changes what IRIX believes.
    */
    uint32_t GF2::GetInstalledPlaneMask()
    {
        BP3* bp3 = (BP3*)vram;

        return bp3 ? bp3->GetPlaneMask() : 0;
    }

    /*
        Say that a result is waiting. The host is either spinning on INTERRUPT_BIT_ or taking the
        interrupt; either way it then reads the code back under READOUTRUN to find out what kind of
        answer this is, and drains the data from the readback FIFO in RUNMODE.
    */
    void GF2::RaiseProgrammedInterrupt(uint16_t code, const uint16_t* data, int32_t count)
    {
        if (count > GF2_FBC_READBACK_MAX)
            count = GF2_FBC_READBACK_MAX;

        readbackHead = 0;
        readbackCount = count;

        for (int32_t i = 0; i < count; i++)
            readback[i] = data[i];

        fbcInterruptCode = code;
        fbcInterruptPending = true;

        if (logEnabled)
            Logger::Log(LOG_PREFIX_GF2, std::format("FBC programmed interrupt {}, {} words of readback",
                code, count).c_str(), LogChannels::Warning);

        UpdateInterrupt();
    }

    /*
        FBCclrint, and every read when AUTOCLEAR is set, takes one word off the front. The interrupt
        stays asserted until the host has drained everything the microcode left for it.
    */
    uint16_t GF2::PopReadback()
    {
        if (readbackCount <= 0)
        {
            fbcInterruptPending = false;

            // Whatever was waiting on the FIFO can have it now, and the line drops if nothing is.
            ServiceCursorRequest();
            return 0;
        }

        uint16_t word = readback[readbackHead];

        readbackHead++;
        readbackCount--;

        if (readbackCount == 0)
        {
            fbcInterruptPending = false;
            ServiceCursorRequest();
        }

        return word;
    }

    /*
        Start capturing. Two words are reserved at the front for the token the host throws away and
        the word count it checks, neither of which is known until the end markers arrive.
    */
    void GF2::BeginFeedback()
    {
        feedbackMode = true;
        feedbackLength = 2;
        feedback[0] = GF2_FBC_OP_FEEDBACK;
        feedback[1] = 0;
    }

    void GF2::AppendFeedback(uint16_t word)
    {
        if (feedbackLength >= GF2_FBC_FEEDBACK_MAX)
            return;

        feedback[feedbackLength++] = word;
    }

    /*
        GEstoremm in feedback mode. struct matdata is a short the driver comments as "GEstoremm
        command" and then a Matrix, so it is the opcode as a token followed by sixteen floats, high
        half of each first - the same order GEloadmm reads them back in, which is what makes
        restoreeverything's replay of the saved blob a round trip rather than a reinterpretation.
    */
    void GF2::StoreMatrixToFeedback()
    {
        AppendFeedback(geOpcode);

        for (int32_t i = 0; i < 16; i++)
        {
            uint32_t bits;

            memcpy(&bits, &matrix[matrixTop][i], sizeof(bits));

            AppendFeedback((uint16_t)(bits >> 16));
            AppendFeedback((uint16_t)bits);
        }
    }

    /*
        The end markers have arrived, so fill in the word count and hand the buffer over. The count
        covers everything after itself, the three markers included.
    */
    void GF2::EndFeedback()
    {
        feedbackMode = false;
        feedback[1] = (uint16_t)(feedbackLength - 2);

        if (logEnabled)
            Logger::Log(LOG_PREFIX_GF2, std::format("FBC feedback buffer complete: {} words",
                feedbackLength - 2).c_str(), LogChannels::Warning);

        RaiseProgrammedInterrupt(GF2_FBC_INT_FEEDBACK, feedback, feedbackLength);
    }

    /*
        Execute one FBC command out of a passthru body. Returns how many words it consumed, so the
        caller can carry on with whatever follows it in the same body, or 0 to say the body cannot be
        parsed any further.
    */
    int32_t GF2::ExecuteFBCCommand(const uint16_t* words, int32_t available)
    {
        uint16_t opcode = words[0] & 0xFF;
        int32_t params = FBCParamCount(opcode);

        if (params == GF2_FBC_PARAMS_UNKNOWN)
        {
            if (logEnabled)
                Logger::Log(LOG_PREFIX_GF2, std::format("Unknown FBC opcode 0x{:02x}, abandoning the rest of the body",
                    opcode).c_str(), LogChannels::Warning);

            return 0;
        }

        // A command with no length of its own owns everything left in the body.
        if (params == GF2_FBC_PARAMS_REST)
            params = available - 1;

        switch (opcode)
        {
            case GF2_FBC_OP_LOADRAM:
            {
                /*
                    The scratch RAM tables: the swizzle table at 0x700, the mask table just below it
                    and the divide table at 0x800. The microcode indexes these, so they have to be
                    kept even though nothing reads them back - write_scratch() sends them in blocks
                    of 120 words, and a block that lands in the wrong place would be silent.
                */
                if (available < 3)
                    return 0;

                int32_t addr = words[1];
                int32_t count = words[2];

                if (available < 3 + count)
                {
                    if (logEnabled)
                        Logger::Log(LOG_PREFIX_GF2, std::format("FBCloadram of {} words at 0x{:x} runs off the end of a {} word body",
                            count, addr, available).c_str(), LogChannels::Warning);

                    return 0;
                }

                for (int32_t i = 0; i < count; i++)
                {
                    if (addr + i < GF2_FBC_SCRATCH_WORDS)
                        scratchRam[addr + i] = words[3 + i];
                }

                if (logEnabled)
                    Logger::Log(LOG_PREFIX_GF2, std::format("FBCloadram: {} words into scratch RAM at 0x{:x}",
                        count, addr).c_str(), LogChannels::Warning);

                return 3 + count;
            }

            case GF2_FBC_OP_LOADMASKS:
            {
                /*
                    The font RAM: the default stipple pattern at 0, the cursor at 16 and the console
                    font from 32 up. There is no count word - the passthru length said how much, so
                    everything after the address belongs to this command. The textport cannot draw a
                    character that was never loaded, so this has to be kept even though the drawing
                    that reads it is not written yet.
                */
                if (available < 2)
                    return 0;

                /*
                    Relative to the base, which is the whole point of there being one: mex loads
                    thirteen cursor glyphs at 0, 16, 32 and so on up to 192, and then selects one by
                    its *absolute* address, 2224. That only lines up if the base was 2048 when the
                    glyphs went in - gl_fontslot() gave mex that slot, and the kernel adds the base
                    itself for the cursor because the microcode takes that one address absolute.

                    Ignoring the base put every one of mex's glyphs 2048 words too low, so selecting
                    2224 read font RAM nobody had written and selecting 16 - which is where the
                    kernel's arrow lives - got mex's second cursor instead. The pointer was drawn in
                    the right place, in the right colour, out of the wrong bitmap.
                */
                int32_t addr = fontRamBase + words[1];
                int32_t count = available - 2;

                for (int32_t i = 0; i < count; i++)
                {
                    if (addr + i < GF2_FBC_FONTRAM_WORDS)
                        fontRam[addr + i] = words[2 + i];
                }

                if (logEnabled)
                    Logger::Log(LOG_PREFIX_GF2, std::format("FBCloadmasks: {} words into font RAM at {} (base {} + {})",
                        count, addr, fontRamBase, words[1]).c_str(), LogChannels::Warning);

                return 2 + count;
            }

            case GF2_FBC_OP_COLOR:
                // The colour index everything is drawn in until it changes.
                if (available > 1)
                    fbcColour = words[1];

                return 1 + params;

            case GF2_FBC_OP_WRTEN:
                /*
                    The write enable mask, one bit per bitplane. IRIX works out what to put here from
                    the pixel readback, so with twelve usable planes out of thirty two fitted it
                    sends 0x0fff - which is a direct check that the readback answered correctly.
                */
                if (available > 1)
                    fbcWriteMask = words[1];

                return 1 + params;

            case GF2_FBC_OP_CHARPOSNABS:
                /*
                    Two forms. im_cmov2i sends this on its own and then pushes a GEpoint through the
                    pipe, so the position arrives transformed one command later - that is the form
                    the textport uses for every line it draws. gl_getplaneinfo instead passes the
                    point straight through as three more words, already in screen coordinates.
                */
                if (available >= 4)
                {
                    charPositionX = (float)(int16_t)words[2];
                    charPositionY = (float)(int16_t)words[3];
                    charPositionPending = false;
                }
                else
                {
                    charPositionPending = true;
                }

                return 1 + params;

            case GF2_FBC_OP_DRAWCHARS:
            {
                /*
                    A whole string, four words per character, as many as fit in one passthru -
                    xcharstr sends thirty at a time and starts another command for the rest.
                */
                int32_t count = (available - 1) / GF2_FBC_CHAR_WORDS;

                for (int32_t i = 0; i < count; i++)
                    DrawCharacter(&words[1 + i * GF2_FBC_CHAR_WORDS]);

                if (logEnabled)
                    Logger::Log(LOG_PREFIX_GF2, std::format("FBCdrawchars: {} characters from ({}, {})",
                        count, charPositionX, charPositionY).c_str(), LogChannels::Warning);

                return 1 + params;
            }

            case GF2_FBC_OP_FORCECOMPLETION:
                /*
                    Not a command the guest means anything by. The kernel's gl_WaitForEOF has no EOF
                    interrupt to wait on - unlike the user space one it never sends FBCeof - so it
                    waits by filling the pipe instead, pushing seventy six copies of the long
                    0x00080008 and reasoning that "pipe only holds about 140 goobies, when all these
                    passthrus have been stuffed down the pipe, eof must have been reached". Each of
                    those longs is a one word passthru whose body is GEpassthru itself.

                    Named here so it does not come out of the log as four thousand unimplemented
                    commands a boot, which is what sent this session looking for a parser bug.
                */
                return 1 + params;

            case GF2_FBC_OP_BASEADDRESS:
                /*
                    Where this client's slice of the font RAM starts. gl_fontslot() hands every GL
                    process a 256 word aligned slot and the FBC adds the base to what it is given, so
                    two programs can each load a font at "0" without treading on each other.
                */
                if (available > 1)
                    fontRamBase = words[1];

                if (logEnabled)
                    Logger::Log(LOG_PREFIX_GF2, std::format("FBCbaseaddress: font RAM base is now {}",
                        fontRamBase).c_str(), LogChannels::Warning);

                return 1 + params;

            case GF2_FBC_OP_SELECTCURSOR:
                SelectCursor(words, available);
                return 1 + params;

            case GF2_FBC_OP_DRAWCURSOR:
                if (available >= 3)
                    DrawCursor((int16_t)words[1], (int16_t)words[2]);

                return 1 + params;

            case GF2_FBC_OP_UNDRAWCURSOR:
                UndrawCursor();
                return 1 + params;

            case GF2_FBC_OP_FEEDBACK:
                /*
                    Stop drawing and start capturing. Everything the geometry side produces from here
                    goes into the feedback buffer until the three end markers arrive.
                */
                BeginFeedback();

                if (logEnabled)
                    Logger::Log(LOG_PREFIX_GF2, "FBCfeedback: the pipe is now feeding back", LogChannels::Warning);

                return 1 + params;

            case GF2_FBC_OP_READCHARPOSN:
            {
                /*
                    Where the next character would go. restoreeverything() reads this out so it can
                    put it back, and the third word is the valid flag: -1 means "good character
                    position" and anything else makes the restore send a bare FBCcharposnabs and
                    leave the position alone. There is no word count in front of this one - the
                    driver knows there are three words and reads exactly three.
                */
                uint16_t answer[4] =
                {
                    GF2_FBC_OP_READCHARPOSN,
                    (uint16_t)(int16_t)charPositionX,
                    (uint16_t)(int16_t)charPositionY,
                    0xFFFF,
                };

                RaiseProgrammedInterrupt(GF2_FBC_INT_CHPOSN, answer, 4);

                return 1 + params;
            }

            case GF2_FBC_OP_EOF:
            {
                /*
                    The end of a batch of work, and the handshake every GL program waits on. The host
                    locks the pipe, increments EOFpending in its shared memory, and sends this; the
                    microcode raises _INTEOF when it comes out the far end, and the kernel's
                    fbc_progintr() pops two words and decrements the count, which is what releases

                        while (sh->EOFpending & EOFPENDINGBITS) ;

                    in gl_WaitForEOF. Without the interrupt that loop never ends: mex spins there
                    forever at 100% CPU having drawn nothing, which looks like a hang in the geometry
                    engine and is really just an unanswered acknowledgement.
                */
                uint16_t answer[2] = { GF2_FBC_OP_EOF, 0 };

                RaiseProgrammedInterrupt(GF2_FBC_INT_EOF, answer, 2);

                return 1 + params;
            }

            case GF2_FBC_OP_READPIXELS:
            {
                /*
                    Read pixels back to the host. The only caller that matters during bring-up is
                    gl_getplaneinfo, which writes a single all-ones pixel and reads it back to find
                    out which bitplanes exist.

                    What it expects to find, in order: the command, the word count, then the pixel as
                    two 16 bit halves, C/D planes first and A/B second - it reassembles them as
                    (cd << 16) | ab. The first two are popped and thrown away, so only their presence
                    matters, but the interrupt code does not: reading anything other than _INTPIXEL32
                    there is a panic.
                */
                uint32_t planes = GetInstalledPlaneMask();

                uint16_t answer[4] =
                {
                    GF2_FBC_OP_READPIXELS,
                    (uint16_t)(available > 1 ? words[1] : 1),
                    (uint16_t)(planes >> 16),
                    (uint16_t)(planes & 0xFFFF),
                };

                RaiseProgrammedInterrupt(GF2_FBC_INT_PIXEL32, answer, 4);

                return 1 + params;
            }

            default:
                /*
                    Everything else is understood well enough to be stepped over but not yet to be
                    carried out. Stepping over it correctly is what keeps the commands that do work
                    aligned, so this is not the same as ignoring the stream.
                */
                if (logEnabled)
                {
                    std::string args;

                    for (int32_t i = 1; i <= params && i < available; i++)
                        args += std::format("{}0x{:04x}", args.empty() ? "" : " ", words[i]);

                    Logger::Log(LOG_PREFIX_GF2, std::format("FBC command 0x{:02x} ({}) is not implemented",
                        opcode, args).c_str(), LogChannels::Warning);
                }

                return 1 + params;
        }
    }

    // Walk a completed passthru body, which may hold more than one command.
    void GF2::ExecuteFBCBody()
    {
        int32_t offset = 0;

        while (offset < geBodyLength)
        {
            int32_t used = ExecuteFBCCommand(&geBody[offset], geBodyLength - offset);

            // A command that could not be parsed takes the rest of the body with it.
            if (used <= 0)
                break;

            offset += used;
        }

        geBodyLength = 0;
    }

    /*
        One word into the pipe. The address decides whether this is the last word of a command (the
        GE port token) but not what the word means - that is entirely positional, which is why the
        passthru counter has to be kept across writes rather than worked out per access.
    */
    void GF2::PushGEWord(uint16_t word, bool last)
    {
        if (gePassthruLeft > 0)
        {
            if (geBodyLength < (int32_t)(sizeof(geBody) / sizeof(geBody[0])))
                geBody[geBodyLength++] = word;

            gePassthruLeft--;

            if (gePassthruLeft == 0)
                ExecuteFBCBody();

            return;
        }

        /*
            Operands first. A word only means an opcode when nothing is waiting for it, and a
            coordinate is free to look like anything: the textport draws a rectangle at x = 8, whose
            low byte is GEpassthru, so testing for a passthru header before this drops the rest of
            that rectangle and resynchronises the parser onto the middle of a command.
        */
        if (geOperandsLeft > 0)
        {
            if (geOperandCount < (int32_t)(sizeof(geOperands) / sizeof(geOperands[0])))
                geOperands[geOperandCount++] = word;

            geOperandsLeft--;

            if (geOperandsLeft == 0)
                ExecuteGECommand();

            return;
        }

        /*
            A configuration byte per geometry chip, the chip number in the high byte counting down,
            ending at the first word whose high byte is 0xff - which gl_justconfigure writes as
            im_passthru(0) and the inline versions in gr.c write as a last config value.
        */
        if (geReconfiguring)
        {
            if ((word & 0xFF00) == 0xFF00)
                geReconfiguring = false;

            return;
        }

        /*
            The three end markers, before anything else looks at the word: FBCEOF1 is 0x0108, which
            is a well formed two word passthru header, so testing for a passthru first would swallow
            the other two markers as an FBC command body and leave the feedback buffer unfinished -
            and the host spinning on an interrupt that never comes.
        */
        if (feedbackMode)
        {
            if (geEofWordsLeft > 0)
            {
                AppendFeedback(word);

                if (--geEofWordsLeft == 0)
                    EndFeedback();

                return;
            }

            if (word == GF2_FBC_EOF1)
            {
                AppendFeedback(word);
                geEofWordsLeft = 2;
                return;
            }
        }

        if ((word & 0xFF) == GF2_GE_OP_PASSTHRU)
        {
            /*
                im_passthru(0) encodes as 0xFF08 because (0 - 1) << 8 wraps into the high byte. It
                locks the pipe when written to GEPORT and frees it when written to LASTGE, and either
                way it opens no body - reading it as a 256 word one swallows everything after it.
            */
            if (word == GF2_GE_PASSTHRU_FREE)
                return;

            gePassthruLeft = ((word >> 8) & 0xFF) + 1;
            geBodyLength = 0;

            return;
        }

        geOpcode = word;
        geOperandCount = 0;
        geOperandsLeft = GEParamCount(word);

        if (geOperandsLeft == 0)
            ExecuteGECommand();
    }

    /*
        How many operand words a geometry command takes. The opcode is the low byte and the operand
        format is packed into bits 8-11: a dimension of 2, 3 or 4, and a type of short, long integer
        or float. Only the drawing and matrix commands carry operands - the rest are bare opcodes.

        This is what makes the stream self describing, and it is why the GE side can be framed
        correctly long before anything is able to carry the commands out.
    */
    int32_t GF2::GEParamCount(uint16_t word)
    {
        switch (word & 0xFF)
        {
            // Coordinates: move, draw, point, curve, their relative forms, polygons, transform.
            case 0x10: case 0x11: case 0x12: case 0x13:
            case 0x14: case 0x15: case 0x16:
            case 0x30: case 0x31: case 0x34: case 0x35: case 0x37: case 0x38:
            // Matrix concatenations take a row of the matrix in the same formats.
            case 0x20: case 0x21: case 0x22: case 0x23:
            case 0x24: case 0x25: case 0x26: case 0x27:
            case 0x28: case 0x29: case 0x2A: case 0x2B:
            case 0x2C: case 0x2D: case 0x2E: case 0x2F:
            {
                int32_t dimension;

                switch (word & GF2_GEPA_DIM_MASK)
                {
                    case GF2_GEPA_2D: dimension = 2; break;
                    case GF2_GEPA_3D: dimension = 3; break;
                    default:          dimension = 4; break;
                }

                // Shorts are one word each; integers and floats are both 32 bits, so two.
                return ((word & GF2_GEPA_TYPE_MASK) == GF2_GEPA_SHORT) ? dimension : dimension * 2;
            }

            case 0x01: return 32;   // GEloadmm, a 4x4 matrix of floats
            case 0x05: return 16;   // GEloadviewport, eight longs

            default:   return 0;    // pop, push, store, reconfigure, noop, closepoly
        }
    }

    /*
        A complete geometry command. Carrying these out means the transform pipeline the fourteen GE
        chips implement - the matrix stack, the viewport, clipping - feeding transformed vertices to
        the frame buffer controller, and none of that exists yet.

        The textport is the reason to build it: it draws its background and its cursor as filled
        polygons and positions every character with a transformed point, so text on the screen goes
        through here rather than through the passthru side.
    */
    void GF2::ExecuteGECommand()
    {
        if (logEnabled)
            Logger::Log(LOG_PREFIX_GF2, std::format("GE command 0x{:04x} ({})",
                geOpcode, FormatGEOperands()).c_str(), LogChannels::Warning);

        switch (geOpcode & 0xFF)
        {
            case GF2_GE_OP_LOADMM:
                /*
                    Sixteen floats, transposed. They replace the top of the stack rather than being
                    concatenated onto it, which is what makes this loadmatrix and not multmatrix.
                */
                for (int32_t i = 0; i < 16 && (i * 2 + 1) < geOperandCount; i++)
                {
                    uint32_t bits = ((uint32_t)geOperands[i * 2] << 16) | geOperands[i * 2 + 1];

                    memcpy(&matrix[matrixTop][i], &bits, sizeof(float));
                }
                break;

            case GF2_GE_OP_PUSHMM:
                if (matrixTop + 1 < GF2_GE_MATRIX_STACK_DEPTH)
                {
                    memcpy(matrix[matrixTop + 1], matrix[matrixTop], sizeof(matrix[0]));
                    matrixTop++;
                }
                break;

            case GF2_GE_OP_POPMM:
                if (matrixTop > 0)
                    matrixTop--;
                break;

            case GF2_GE_OP_STOREMM:
                /*
                    Hand the top of the matrix stack back to the host. Outside feedback mode there is
                    nowhere for it to go - the controller is drawing, not capturing - so this is the
                    one command whose meaning depends on what the FBC was told beforehand.
                */
                if (feedbackMode)
                    StoreMatrixToFeedback();
                break;

            case GF2_GE_OP_RECONFIGURE:
                // The configuration bytes follow; PushGEWord swallows them to the 0xff terminator.
                geReconfiguring = true;
                break;

            case GF2_GE_OP_CONCAT_FIRST ... GF2_GE_OP_CONCAT_LAST:
            {
                /*
                    A row of a matrix to concatenate onto the top of the stack. Which row is the low
                    two bits; whether this row opens the matrix, closes it, or does both is the two
                    above them. Everything that moves the world other than loadmatrix arrives here,
                    including the single GEcompletemm3 mex sends to put a window's textport where the
                    window is instead of at the screen origin.
                */
                int32_t row = (geOpcode & GF2_GE_CONCAT_ROW_MASK);
                int32_t kind = ((geOpcode & 0xFF) >> GF2_GE_CONCAT_KIND_SHIFT) & 0x03;

                if (kind == GF2_GE_CONCAT_FIRST_ROW || kind == GF2_GE_CONCAT_COMPLETE)
                    BeginConcatMatrix();

                ReadMatrixRow(row);

                if (kind == GF2_GE_CONCAT_LAST_ROW || kind == GF2_GE_CONCAT_COMPLETE)
                    ConcatenateMatrix();

                break;
            }

            case GF2_GE_OP_LOADVIEWPORT:
            {
                /*
                    Eight longs in 20.8 fixed point: centre x, centre y, half size x, half size y,
                    then the near and far pairs this does not need until there is a z buffer.
                */
                if (geOperandCount < 8)
                    break;

                auto asLong = [this](int32_t index)
                {
                    return (float)(int32_t)(((uint32_t)geOperands[index * 2] << 16) | geOperands[index * 2 + 1]);
                };

                viewportCentreX = asLong(0) / GF2_GE_VIEWPORT_FRACTION;
                viewportCentreY = asLong(1) / GF2_GE_VIEWPORT_FRACTION;
                viewportScaleX = asLong(2) / GF2_GE_VIEWPORT_FRACTION;
                viewportScaleY = asLong(3) / GF2_GE_VIEWPORT_FRACTION;
                viewportLoaded = true;

                if (logEnabled)
                    Logger::Log(LOG_PREFIX_GF2, std::format("GE viewport: centre ({}, {}) half size ({}, {})",
                        viewportCentreX, viewportCentreY, viewportScaleX, viewportScaleY).c_str(),
                        LogChannels::Warning);
                break;
            }

            case GF2_GE_OP_POINT:
            {
                /*
                    A transformed point. The textport only sends one to say where the next string
                    goes, which is why this feeds the character position rather than drawing.
                */
                float vertex[4];

                ReadVertex(vertex);
                TransformVertex(vertex, &charPositionX, &charPositionY);

                charPositionPending = false;
                break;
            }

            case GF2_GE_OP_MOVEPOLY:
                // The first vertex of a filled polygon, so anything half built is abandoned.
                polygonVertices = 0;
                AddPolygonVertex();
                break;

            case GF2_GE_OP_DRAWPOLY:
                AddPolygonVertex();
                break;

            case GF2_GE_OP_CLOSEPOLY:
                FillPolygon();
                break;

            default:
                break;
        }

        geOperandCount = 0;
    }

    /*
        Operands in the format the opcode asked for, so a trace reads as coordinates rather than as
        hex. The three types are not interchangeable: a short is one word, an integer is two, and a
        float is two words of IEEE 754 that the first GE chip converts to 20.8 fixed point.
    */
    std::string GF2::FormatGEOperands()
    {
        std::string out;

        bool isShort = (geOpcode & GF2_GEPA_TYPE_MASK) == GF2_GEPA_SHORT;
        bool isFloat = (geOpcode & GF2_GEPA_TYPE_MASK) == GF2_GEPA_FLOAT;
        int32_t step = isShort ? 1 : 2;

        for (int32_t i = 0; i + step <= geOperandCount; i += step)
        {
            if (!out.empty())
                out += ", ";

            if (isShort)
            {
                out += std::format("{}", (int16_t)geOperands[i]);
                continue;
            }

            uint32_t bits = ((uint32_t)geOperands[i] << 16) | geOperands[i + 1];

            if (isFloat)
            {
                float value;

                memcpy(&value, &bits, sizeof(value));
                out += std::format("{:g}", value);
            }
            else
            {
                out += std::format("{}", (int32_t)bits);
            }
        }

        return out;
    }

    // The log is a separate step so that turning it off costs one branch rather than reformatting.
    void GF2::LogGEWord(size_t addr, uint32_t value, int32_t width)
    {
        if (!logEnabled || geWordsLogged >= GF2_MAX_GE_LOGGED)
            return;

        geWordsLogged++;

        const char* port = ((addr & 0x1000) == 0) ? "LASTGE" : "GEPORT";

        Logger::Log(LOG_PREFIX_GF2, std::format("GE {}-bit write of 0x{:x} to {} (0x{:x})",
            width, value, port, addr).c_str(), LogChannels::Warning);

        if (geWordsLogged == GF2_MAX_GE_LOGGED)
            Logger::Log(LOG_PREFIX_GF2, "further geometry pipe writes will not be logged", LogChannels::Debug);
    }

    /*
        Everything the host draws arrives here. A long write is two words, high half first, because
        the pipe is 16 bits wide and the FIFO order is what the parser depends on.
    */
    void GF2::WriteGE(size_t addr, uint32_t value, int32_t width)
    {
        LogGEWord(addr, value, width);

        // A12 selects LASTGE, which sets the GE port token flag as the word goes in.
        bool last = ((addr & 0x1000) == 0);

        if (width == 32)
        {
            PushGEWord((uint16_t)(value >> 16), false);
            PushGEWord((uint16_t)value, last);

            return;
        }

        PushGEWord((uint16_t)value, last);
    }
};
