/* motion - The SGI Emulator. Copyright (c)2026 danifunker. gf2_commands.cpp: the segment 6 command stream. */

#include <component/gpu/juniper/gf2/gf2.hpp>

namespace Motion
{
    // How many words each FBC opcode takes after itself.
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

    // gl_getplaneinfo writes one pixel with the colour and the write mask both all ones and reads it back, so the honest answer is whatever the fitted.
    uint32_t GF2::GetInstalledPlaneMask()
    {
        BP3* bp3 = (BP3*)vram;

        return bp3 ? bp3->GetPlaneMask() : 0;
    }

    // Say that a result is waiting.
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

    // FBCclrint, and every read when AUTOCLEAR is set, takes one word off the front.
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

    // Start capturing.
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

    // GEstoremm in feedback mode.
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

    // The end markers have arrived, so fill in the word count and hand the buffer over.
    void GF2::EndFeedback()
    {
        feedbackMode = false;
        feedback[1] = (uint16_t)(feedbackLength - 2);

        if (logEnabled)
            Logger::Log(LOG_PREFIX_GF2, std::format("FBC feedback buffer complete: {} words",
                feedbackLength - 2).c_str(), LogChannels::Warning);

        RaiseProgrammedInterrupt(GF2_FBC_INT_FEEDBACK, feedback, feedbackLength);
    }

    // Execute one FBC command out of a passthru body.
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
                // The scratch RAM tables: the swizzle table at 0x700, the mask table just below it and the divide table at 0x800.
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
                // The font RAM: the default stipple pattern at 0, the cursor at 16 and the console font from 32 up.
                if (available < 2)
                    return 0;

                // Relative to the base, which is the whole point of there being one: mex loads thirteen cursor glyphs at 0, 16, 32 and so on up to 192, and then.
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
                // The write enable mask, one bit per bitplane.
                if (available > 1)
                    fbcWriteMask = words[1];

                return 1 + params;

            case GF2_FBC_OP_CHARPOSNABS:
                // Two forms.
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
                // A whole string, four words per character, as many as fit in one passthru - xcharstr sends thirty at a time and starts another command for the rest.
                int32_t count = (available - 1) / GF2_FBC_CHAR_WORDS;

                for (int32_t i = 0; i < count; i++)
                    DrawCharacter(&words[1 + i * GF2_FBC_CHAR_WORDS]);

                if (logEnabled)
                    Logger::Log(LOG_PREFIX_GF2, std::format("FBCdrawchars: {} characters from ({}, {})",
                        count, charPositionX, charPositionY).c_str(), LogChannels::Warning);

                return 1 + params;
            }

            case GF2_FBC_OP_FORCECOMPLETION:
                // Not a command the guest means anything by.
                return 1 + params;

            case GF2_FBC_OP_BASEADDRESS:
                // Where this client's slice of the font RAM starts.
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
                // Stop drawing and start capturing.
                BeginFeedback();

                if (logEnabled)
                    Logger::Log(LOG_PREFIX_GF2, "FBCfeedback: the pipe is now feeding back", LogChannels::Warning);

                return 1 + params;

            case GF2_FBC_OP_READCHARPOSN:
            {
                // Where the next character would go.
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
                // The end of a batch of work, and the handshake every GL program waits on.
                uint16_t answer[2] = { GF2_FBC_OP_EOF, 0 };

                RaiseProgrammedInterrupt(GF2_FBC_INT_EOF, answer, 2);

                return 1 + params;
            }

            case GF2_FBC_OP_READPIXELS:
            {
                // Read pixels back to the host.
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
                // Everything else is understood well enough to be stepped over but not yet to be carried out.
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

    // One word into the pipe.
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

        // Operands first.
        if (geOperandsLeft > 0)
        {
            if (geOperandCount < (int32_t)(sizeof(geOperands) / sizeof(geOperands[0])))
                geOperands[geOperandCount++] = word;

            geOperandsLeft--;

            if (geOperandsLeft == 0)
                ExecuteGECommand();

            return;
        }

        // A configuration byte per geometry chip, the chip number in the high byte counting down, ending at the first word whose high byte is 0xff - which.
        if (geReconfiguring)
        {
            if ((word & 0xFF00) == 0xFF00)
                geReconfiguring = false;

            return;
        }

        // The three end markers, before anything else looks at the word: FBCEOF1 is 0x0108, which is a well formed two word passthru header, so testing for a.
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
            // im_passthru(0) encodes as 0xFF08 because (0 - 1) << 8 wraps into the high byte.
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

    // How many operand words a geometry command takes.
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

    // A complete geometry command.
    void GF2::ExecuteGECommand()
    {
        if (logEnabled)
            Logger::Log(LOG_PREFIX_GF2, std::format("GE command 0x{:04x} ({})",
                geOpcode, FormatGEOperands()).c_str(), LogChannels::Warning);

        switch (geOpcode & 0xFF)
        {
            case GF2_GE_OP_LOADMM:
                // Sixteen floats, transposed.
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
                // Hand the top of the matrix stack back to the host.
                if (feedbackMode)
                    StoreMatrixToFeedback();
                break;

            case GF2_GE_OP_RECONFIGURE:
                // The configuration bytes follow; PushGEWord swallows them to the 0xff terminator.
                geReconfiguring = true;
                break;

            case GF2_GE_OP_CONCAT_FIRST ... GF2_GE_OP_CONCAT_LAST:
            {
                // A row of a matrix to concatenate onto the top of the stack.
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
                // Eight longs in 20.8 fixed point: centre x, centre y, half size x, half size y, then the near and far pairs this does not need until there is a z.
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
                // A transformed point.
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

    // Operands in the format the opcode asked for, so a trace reads as coordinates rather than as hex.
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

    // Everything the host draws arrives here.
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
