/*
    C    O    H    E    R    E    N    T
    Extensible Emulator Debugging Tools!

    Coherent is an extensible debugger for emulators that is intended to allow the debugging of multiple types of CPU cores in an easy way.

    coherent_editorcpp: Coherent Memory Editor base code.
*/

#include <coherent/coherent_editor.hpp>

namespace Motion
{
    void CoherentEditor::AddUI()
    {
        ImGui::SetNextWindowSize(ImVec2(500, 350));

        if (!settings.buf)
        {
            if (!shutupFatalError) // otherwise it will log every frame
            {
                Logger::Log(COHERENT_LOG_PREFIX, "CoherentEditor will not display because nothing was provided for it to edit (settings.buf == nullptr)", LogChannels::Error);
                shutupFatalError = true;
            }

            return;
        }
        else if (!settings.bufSize)
        {
            if (!shutupFatalError)
            {
                Logger::Log(COHERENT_LOG_PREFIX, "CoherentEditor will not display because the buffer size is 0 bytes (settings.bufSize == 0)", LogChannels::Error);
                shutupFatalError = true;   
            }
        }   

        const char* name = settings.name;

        if (!name)
            name = "Name this editor please";

        if (ImGui::Begin(name, &enabled))
        {
            // temporary thing for v0.1.1 since we already wrote some code to do this

            bool updateMemoryView = false; 

            if (ImGui::InputTextWithHint("##InputStartRendering", "View memory starting at...", startAddressInputAtBuf, STRING_MAX_SHORT,
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsHexadecimal))
            {
                updateMemoryView = true;
                ImGui::SetKeyboardFocusHere(-1);
            }

            ImGui::SameLine();

            if (ImGui::Button("Go"))
                updateMemoryView = true; 

            if (updateMemoryView)
            {
                auto addr = (size_t)strtol(startAddressInputAtBuf, NULL, 16);

                addressIsValid = (addr >= 0
                && addr < settings.bufSize);

                if (addressIsValid)
                    startDrawingAt = addr;

                startAddressInputAtBuf[0] = '\0'; // null terminate to clear the box
            }

            size_t currentAddress = startDrawingAt;

            ImGui::Text("Viewing: 0x%.8lx-0x%.8lx", startDrawingAt, startDrawingAt + settings.loadAtOnce);

            if (!addressIsValid)
                ImGui::Text("Nothing to see here...");
            else
            {
                for (currentAddress = startDrawingAt; currentAddress < (startDrawingAt + settings.loadAtOnce); currentAddress += settings.lineSize)
                {
                    ImGui::Text("%.8lx:\t", currentAddress);
                    ImGui::SameLine();

                    for (int32_t j = 0; j < settings.lineSize; j++)
                    {
                        ImGui::Text("%.2x", settings.buf[currentAddress + j]);
                        ImGui::SameLine();

                        // in the case where the user's chosen address partially overlaps the end of the buffer.
                        if ((currentAddress + j) >= settings.bufSize)
                            goto done; 
                    }

                    ImGui::NewLine();

                    // in the case where the user's chosen address partially overlaps the end of the buffer.
                    if (currentAddress >= settings.bufSize)
                        goto done; 
                }
                
            done:
            }
        }

        ImGui::End();
    }

    void CoherentEditor::SetDefaultSettings()
    {
        settings.loadAtOnce = 0x100; //test
        settings.lineSize = 16; 
    }
}; 