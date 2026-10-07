/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    dc4_debug.cpp: DC4 coherent extension
*/

#include <component/gpu/juniper/dc4/dc4.hpp>

namespace Motion
{
    void CoherentExtensionDC4::AddUI()
    {
        if (ImGui::Begin("DC4 State", &enabled))
        {
            DC4* dc4 = (DC4*)component;

            bool singleMapMode = !(dc4->flags & 0x20);

            // iamadc4
            if (singleMapMode)
                ImGui::TextColored(CoherentUI::COLOUR_HEADER, "DC4: Single-map Mode. Flags:");
            else
                ImGui::TextColored(CoherentUI::COLOUR_HEADER, "DC4: Multi-map Mode, mapped colourmap %d. Flags:", (dc4->flags & 0x0F));

            ImGui::Text("Mapped colourmap %d", (dc4->flags & 0x0F));
            ImGui::Text("Bus addrmap: %d", (dc4->flags & DC4_FLAG_BUS_ADDRMAP));
            ImGui::Text("Single or multimode (reg addrmap): %s", (dc4->flags & DC4_FLAG_REG_ADDRMAP) ? "Multimap" : "Singlemap");
            ImGui::Text("RGB mode: %s", (dc4->flags & DC4_FLAG_RGB_MODE) ? "true" : "false");
            ImGui::Text("Use upper half: %s", (dc4->flags & DC4_FLAG_USE_UPPER_HALF) ? "true" : "false");
            ImGui::Text("OPTCLK enabled: %s", (dc4->flags & DC4_FLAG_OPTIONAL_CLOCK) ? "true" : "false");
            ImGui::Text("Pipeline Depth: %d", (dc4->flags & DC4_FLAG_PIPELINE_DEPTH_4) ? 4 : 2);
            ImGui::Text("Alternate video format: %s", (dc4->flags & DC4_FLAG_PROM) ? "true" : "false");
            ImGui::Text("Freerun mode: %s", (dc4->flags & DC4_FLAG_FREERUN) ? "true" : "false");

            float scrollX = ImGui::GetScrollX(), scrollY = ImGui::GetScrollY();
            int32_t initialOriginX = (ImGui::GetWindowPos().x - scrollX);
            int32_t initialOriginY = (ImGui::GetWindowPos().y - scrollY);
            int32_t initialX = (initialOriginX + ImGui::GetCursorPosX()); // make it easier to restart 
            int32_t x = initialX, y = initialOriginY + ImGui::GetCursorPosY();

            // parameters for drawing rects
            int32_t rectSizeX = 16;
            int32_t rectSizeY = 16;
            int32_t borderSize = 1;

            int32_t numMapped = 0;

            ImDrawList* windowDrawList = ImGui::GetWindowDrawList();

            // this is a lambda that draws rectangles for us
            auto DrawColour = [rectSizeX, rectSizeY, borderSize, &dc4, &windowDrawList, &numMapped, &initialX, &x, &y]
            (int32_t i)
            {
                // the colourmap entries are *16-bit* but only 0...255 are supported for each colour.
                // effectively ignore the lower 16 bits
                uint32_t colour = IM_COL32(
                    dc4->colourMap[i] & 0xFF,
                    dc4->colourMap[i + 256] & 0xFF,
                    dc4->colourMap[i + 512] & 0xFF,
                    0xFF
                );

                // draw colour with a white outline
                windowDrawList->AddRectFilled(ImVec2(x + 1, y + 1), ImVec2(x + (rectSizeX - borderSize), y + (rectSizeY - borderSize)), colour, 0.0f);
                windowDrawList->AddRect(ImVec2(x, y), ImVec2(x + rectSizeX, y + rectSizeY), 0xFFFFFFFF, 0.0f);
               
                numMapped++;

                if ((numMapped % 32 == 0) && (numMapped > 0))
                {
                    ImGui::Dummy(ImVec2(0, 16)); // required of imgui
                    y += rectSizeY;
                    x = initialX;
                }
                else
                    x += (rectSizeY);

            };

            // map 4096 colours
            if (singleMapMode)
            {
                ImGui::TextColored(CoherentUI::COLOUR_HEADER, "Colour map:");
                
                for (int32_t i = 0; i < 4096; i++)
                    DrawColour(i);
            }
            else
            {
                // linearly addressed
                for (int32_t i = 0; i < 16; i++)
                {
                    ImGui::TextColored(CoherentUI::COLOUR_HEADER, "Map %d:", i);

                    // restart drawing at the new cursor position
                    int32_t initialX = (initialOriginX + ImGui::GetCursorPosX()); // make it easier to restart 
                    y = initialOriginY + ImGui::GetCursorPosY();

                    // draw each colour
                    for (int32_t j = 0; j < 256; j++)
                        DrawColour((768 * i) + j); // 16 bits per colour, rgb888 format.
                }
            }
        }

        ImGui::End();
    }
}