/* motion - The SGI Emulator. Copyright (c)2026 starfrost. gf2_geometry.cpp: the geometry pipe, and the drawing it feeds. */

#include <component/gpu/juniper/gf2/gf2.hpp>
#include <cmath>

namespace Motion
{
    // Pull the operands of the command being executed into a homogeneous vertex.
    void GF2::ReadVertex(float* out)
    {
        int32_t dimension;

        switch (geOpcode & GF2_GEPA_DIM_MASK)
        {
            case GF2_GEPA_2D: dimension = 2; break;
            case GF2_GEPA_3D: dimension = 3; break;
            default:          dimension = 4; break;
        }

        uint16_t type = geOpcode & GF2_GEPA_TYPE_MASK;
        int32_t step = (type == GF2_GEPA_SHORT) ? 1 : 2;

        out[0] = out[1] = out[2] = 0.0f;
        out[3] = 1.0f;

        for (int32_t i = 0; i < dimension; i++)
        {
            int32_t offset = i * step;

            if (offset + step > geOperandCount)
                break;

            if (type == GF2_GEPA_SHORT)
            {
                out[i] = (float)(int16_t)geOperands[offset];
                continue;
            }

            uint32_t bits = ((uint32_t)geOperands[offset] << 16) | geOperands[offset + 1];

            if (type == GF2_GEPA_FLOAT)
                memcpy(&out[i], &bits, sizeof(float));
            else
                out[i] = (float)(int32_t)bits;
        }
    }

    // One row of the matrix being concatenated, in the same operand format a vertex uses.
    void GF2::ReadMatrixRow(int32_t row)
    {
        int32_t dimension;

        switch (geOpcode & GF2_GEPA_DIM_MASK)
        {
            case GF2_GEPA_2D: dimension = 2; break;
            case GF2_GEPA_3D: dimension = 3; break;
            default:          dimension = 4; break;
        }

        uint16_t type = geOpcode & GF2_GEPA_TYPE_MASK;
        int32_t step = (type == GF2_GEPA_SHORT) ? 1 : 2;

        float values[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

        values[row] = 1.0f;

        for (int32_t i = 0; i < dimension; i++)
        {
            int32_t offset = i * step;

            if (offset + step > geOperandCount)
                break;

            if (type == GF2_GEPA_SHORT)
            {
                values[i] = (float)(int16_t)geOperands[offset];
                continue;
            }

            uint32_t bits = ((uint32_t)geOperands[offset] << 16) | geOperands[offset + 1];

            if (type == GF2_GEPA_FLOAT)
                memcpy(&values[i], &bits, sizeof(float));
            else
                values[i] = (float)(int32_t)bits;
        }

        for (int32_t i = 0; i < 4; i++)
            concatMatrix[i * 4 + row] = values[i];
    }

    void GF2::BeginConcatMatrix()
    {
        for (int32_t i = 0; i < 16; i++)
            concatMatrix[i] = ((i % 5) == 0) ? 1.0f : 0.0f;
    }

    // Multiply the collected matrix onto the top of the stack.
    void GF2::ConcatenateMatrix()
    {
        float* m = matrix[matrixTop];
        float out[16];

        for (int32_t row = 0; row < 4; row++)
        {
            for (int32_t column = 0; column < 4; column++)
            {
                float sum = 0.0f;

                for (int32_t k = 0; k < 4; k++)
                    sum += m[row * 4 + k] * concatMatrix[k * 4 + column];

                out[row * 4 + column] = sum;
            }
        }

        memcpy(m, out, sizeof(out));
    }

    // Matrix, divide, viewport.
    void GF2::TransformVertex(const float* in, float* screenX, float* screenY)
    {
        const float* m = matrix[matrixTop];

        float clip[4];

        for (int32_t i = 0; i < 4; i++)
        {
            clip[i] = m[i * 4 + 0] * in[0]
                    + m[i * 4 + 1] * in[1]
                    + m[i * 4 + 2] * in[2]
                    + m[i * 4 + 3] * in[3];
        }

        // A w of zero is a point on the horizon; there is nothing sensible to divide by.
        float w = (clip[3] != 0.0f) ? clip[3] : 1.0f;

        float deviceX = clip[0] / w;
        float deviceY = clip[1] / w;

        // Without a viewport there is nothing to scale by, and multiplying by zero would collapse every vertex onto one point.
        if (!viewportLoaded)
        {
            *screenX = (deviceX + 1.0f) * 0.5f * (float)(GF2_SCREEN_MAX_X + 1);
            *screenY = (deviceY + 1.0f) * 0.5f * (float)(GF2_SCREEN_MAX_Y + 1);

            return;
        }

        *screenX = deviceX * viewportScaleX + viewportCentreX;
        *screenY = deviceY * viewportScaleY + viewportCentreY;
    }

    // Transform the vertex the current command carries and add it to the polygon being built.
    void GF2::AddPolygonVertex()
    {
        float vertex[4];

        ReadVertex(vertex);

        if (polygonVertices >= GF2_GE_MAX_POLYGON_VERTICES)
            return;

        TransformVertex(vertex, &polygonX[polygonVertices], &polygonY[polygonVertices]);

        polygonVertices++;
    }

    // One pixel, through the write mask.
    void GF2::DrawPixel(int32_t x, int32_t y)
    {
        DrawPixel(x, y, fbcColour, fbcWriteMask);
    }

    void GF2::DrawPixel(int32_t x, int32_t y, uint16_t colour, uint16_t writeMask)
    {
        if (!vram)
            return;

        if (x < 0 || x > GF2_SCREEN_MAX_X || y < 0 || y > GF2_SCREEN_MAX_Y)
            return;

        size_t address = vram->GetVramAddress(x, y);

        uint32_t existing = vram->Read32(address);
        uint32_t written = (colour & writeMask) | (existing & ~(uint32_t)writeMask);

        vram->Write32(address, written);
    }

    // FBCselectcursor: a glyph address in the font RAM, then the mode, the config register, the colour and the write enable, each a long.
    void GF2::SelectCursor(const uint16_t* words, int32_t available)
    {
        if (available < 8)
            return;

        bool changed = (cursorGlyph != words[1] || cursorColour != words[5] || cursorWriteMask != words[7]);

        // Changing the cursor while it is up would strand the old glyph with no record of what was under it.
        bool wasDrawn = cursorDrawn;
        int32_t wasX = cursorX;
        int32_t wasY = cursorY;

        UndrawCursor();

        cursorGlyph = words[1];
        cursorColour = words[5];        // the low half of the colour long; twelve planes fit in it
        cursorWriteMask = words[7];     // likewise the write enable

        if (wasDrawn)
            DrawCursor(wasX, wasY);

        // The glyph goes in the trace because a cursor drawn in the wrong place and a cursor whose bitmap is wrong look identical on a screen and completely.
        if (logEnabled && changed)
        {
            std::string glyph;

            for (int32_t row = GF2_CURSOR_SIZE - 1; row >= 0; row--)
                glyph += std::format("{:04x} ", fontRam[(cursorGlyph + row) & (GF2_FBC_FONTRAM_WORDS - 1)]);

            Logger::Log(LOG_PREFIX_GF2, std::format("FBCselectcursor: glyph at font RAM {}, colour {} mask 0x{:x}, top row first: {}",
                cursorGlyph, cursorColour, cursorWriteMask, glyph).c_str(), LogChannels::Warning);
        }
    }

    // Put the cursor on the screen, remembering what it covers so it can be taken off again.
    void GF2::DrawCursor(int32_t x, int32_t y)
    {
        if (!vram)
            return;

        UndrawCursor();

        cursorX = x;
        cursorY = y;

        // Only the planes the cursor is allowed to write are saved, and only those are put back.
        cursorDrawnMask = cursorWriteMask;

        if (cursorTrace && cursorTraced < GF2_MAX_CURSOR_LOGGED)
        {
            cursorTraced++;
            std::string glyph;
            for (int32_t r = GF2_CURSOR_SIZE - 1; r >= 0; r--)
                glyph += std::format("{:04x} ", fontRam[(cursorGlyph + r) & (GF2_FBC_FONTRAM_WORDS - 1)]);
            Logger::Log(LOG_PREFIX_GF2, std::format("CURSOR draw at ({}, {}) glyph {} colour {} mask 0x{:x}: {}",
                x, y, cursorGlyph, cursorColour, cursorWriteMask, glyph).c_str(), LogChannels::Warning);
        }

        for (int32_t row = 0; row < GF2_CURSOR_SIZE; row++)
        {
            int32_t glyphRow = cursorGlyph + row;
            uint16_t bits = (glyphRow < GF2_FBC_FONTRAM_WORDS) ? fontRam[glyphRow] : 0;

            for (int32_t column = 0; column < GF2_CURSOR_SIZE; column++)
            {
                int32_t px = x + column;
                int32_t py = y + row;

                if (px < 0 || px > GF2_SCREEN_MAX_X || py < 0 || py > GF2_SCREEN_MAX_Y)
                    continue;

                cursorSaved[row][column] = vram->Read32(vram->GetVramAddress(px, py)) & cursorDrawnMask;

                if (bits & (0x8000 >> column))
                    DrawPixel(px, py, cursorColour, cursorWriteMask);
            }
        }

        cursorDrawn = true;
    }

    void GF2::UndrawCursor()
    {
        if (!cursorDrawn || !vram)
        {
            cursorDrawn = false;
            return;
        }

        if (cursorTrace && cursorTraced < GF2_MAX_CURSOR_LOGGED)
        {
            cursorTraced++;
            Logger::Log(LOG_PREFIX_GF2, std::format("CURSOR undraw at ({}, {}) mask 0x{:x}",
                cursorX, cursorY, cursorDrawnMask).c_str(), LogChannels::Warning);
        }

        for (int32_t row = 0; row < GF2_CURSOR_SIZE; row++)
        {
            for (int32_t column = 0; column < GF2_CURSOR_SIZE; column++)
            {
                int32_t px = cursorX + column;
                int32_t py = cursorY + row;

                if (px < 0 || px > GF2_SCREEN_MAX_X || py < 0 || py > GF2_SCREEN_MAX_Y)
                    continue;

                size_t address = vram->GetVramAddress(px, py);
                uint32_t existing = vram->Read32(address);

                vram->Write32(address, (existing & ~(uint32_t)cursorDrawnMask) | cursorSaved[row][column]);
            }
        }

        cursorDrawn = false;
    }

    // The retrace handler's route: it moves the cursor sixty times a second without putting a command through the pipe at all, so this is called from a.
    void GF2::MoveCursor(int32_t x, int32_t y)
    {
        if (cursorDrawn && x == cursorX && y == cursorY)
            return;

        bool wasDrawn = cursorDrawn;

        UndrawCursor();

        cursorX = x;
        cursorY = y;

        if (wasDrawn)
            DrawCursor(x, y);
    }

    // One character out of the font RAM. The descriptor says where the glyph is and how big it is; the glyph is one word per row with the leftmost pixel in.
    void GF2::DrawCharacter(const uint16_t* descriptor)
    {
        int32_t offset = descriptor[0];
        int32_t width = (descriptor[1] >> 8) & 0xFF;
        int32_t height = descriptor[1] & 0xFF;
        int32_t xoff = (int8_t)((descriptor[2] >> 8) & 0xFF);
        int32_t yoff = (int8_t)(descriptor[2] & 0xFF);
        int32_t advance = (int16_t)descriptor[3];

        int32_t originX = (int32_t)floorf(charPositionX) + xoff;
        int32_t originY = (int32_t)floorf(charPositionY) + yoff;

        for (int32_t row = 0; row < height; row++)
        {
            if (offset + row >= GF2_FBC_FONTRAM_WORDS)
                break;

            uint16_t bits = fontRam[offset + row];
            int32_t y = originY + row;

            for (int32_t column = 0; column < width && column < 16; column++)
            {
                if (bits & (0x8000 >> column))
                    DrawPixel(originX + column, y);
            }
        }

        // The position carries across a string: xcharstr sends a whole line as one command.
        charPositionX += (float)advance;
    }

    // Scan convert the polygon.
    void GF2::FillPolygon()
    {
        if (polygonVertices < 3)
        {
            polygonVertices = 0;
            return;
        }

        float lowest = polygonY[0];
        float highest = polygonY[0];

        for (int32_t i = 1; i < polygonVertices; i++)
        {
            if (polygonY[i] < lowest)  lowest = polygonY[i];
            if (polygonY[i] > highest) highest = polygonY[i];
        }

        if (logEnabled)
        {
            float leftmost = polygonX[0];
            float rightmost = polygonX[0];

            for (int32_t i = 1; i < polygonVertices; i++)
            {
                if (polygonX[i] < leftmost)  leftmost = polygonX[i];
                if (polygonX[i] > rightmost) rightmost = polygonX[i];
            }

            Logger::Log(LOG_PREFIX_GF2, std::format("fill {} vertices, ({}, {}) to ({}, {}), colour {} mask 0x{:x}",
                polygonVertices, leftmost, lowest, rightmost, highest, fbcColour, fbcWriteMask).c_str(),
                LogChannels::Warning);
        }

        // The pipe hands over vertices at pixel centres rather than at the corners of an area - which is why the screen clear arrives as 0.5 to 1023.5 and not.
        int32_t firstRow = (int32_t)ceilf(lowest - 0.5f);
        int32_t lastRow = (int32_t)floorf(highest - 0.5f);

        if (firstRow < 0)                  firstRow = 0;
        if (lastRow > GF2_SCREEN_MAX_Y)    lastRow = GF2_SCREEN_MAX_Y;

        for (int32_t y = firstRow; y <= lastRow; y++)
        {
            float sampleY = (float)y + 0.5f;

            // The top row's centre sits exactly on the top edge, and a scanline lying along a horizontal edge crosses nothing, so it would drop out.
            if (sampleY >= highest)
                sampleY = highest - GF2_FILL_EPSILON;
            float crossings[GF2_GE_MAX_POLYGON_VERTICES];
            int32_t found = 0;

            for (int32_t i = 0; i < polygonVertices; i++)
            {
                int32_t next = (i + 1) % polygonVertices;

                float y0 = polygonY[i];
                float y1 = polygonY[next];

                // A crossing counts for the edge whose lower end it is on, so a shared vertex is not counted twice.
                if ((y0 <= sampleY) == (y1 <= sampleY))
                    continue;

                float t = (sampleY - y0) / (y1 - y0);

                crossings[found++] = polygonX[i] + t * (polygonX[next] - polygonX[i]);
            }

            // Insertion sort: there are at most a handful of crossings on any scanline.
            for (int32_t i = 1; i < found; i++)
            {
                float value = crossings[i];
                int32_t j = i - 1;

                while (j >= 0 && crossings[j] > value)
                {
                    crossings[j + 1] = crossings[j];
                    j--;
                }

                crossings[j + 1] = value;
            }

            for (int32_t i = 0; i + 1 < found; i += 2)
            {
                // A pixel belongs to the span when its centre, at x + 0.5, falls inside it.
                int32_t from = (int32_t)ceilf(crossings[i] - 0.5f);
                int32_t to = (int32_t)floorf(crossings[i + 1] - 0.5f);

                for (int32_t x = from; x <= to; x++)
                    DrawPixel(x, y);
            }
        }

        polygonVertices = 0;
    }
};
