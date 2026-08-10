
/*
    C    O    H    E    R    E    N    T
    Extensible Emulator Debugging Tools!

    Coherent is an extensible debugger for emulators that is intended to allow the debugging of multiple types of CPU cores in an easy way.

    coherent_gui_logging.cpp: Pipes logging to a coherent window
*/

#include <coherent/coherent.hpp>
#include <coherent/coherent_gui_imgui.hpp>

namespace Motion
{
    void CoherentUI::DrawLogWindow()
    {        
        size_t length = strlen(CoherentUI::logBuffer);
        size_t offset = 0;
        size_t copySize = LOGBUF_PURGE_SIZE;
        bool newlineFound = true;
        
        if (!Coherent::active)
            return;

        // don't bother doing anything if there is no buffer
        if (strlen(logBuffer) == 0)
            return;

        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Emulator Log", &Coherent::active))
        {
    // this code is a test
            char logWindowBuf[LOGBUF_PURGE_SIZE];

            // copy only the last 1024 characters if > 1024
            if (length > LOGBUF_PURGE_SIZE)
                offset = length - LOGBUF_PURGE_SIZE;

            // make sure the buffer starts on a new line

            while (logBuffer[offset] != '\n')
            {
                offset++;
                copySize--;

                if (offset >= LOGBUF_MAX_SIZE
                || copySize == 0)
                {
                    newlineFound = false;
                    break;
                }
            }

            // start ont he character after the newline so we don't have a stray newline
            if (newlineFound)
            {
                offset++;
                copySize--;
            }

            strncpy(logWindowBuf, (logBuffer + offset), copySize);
            logWindowBuf[copySize] = '\0';
            ImGui::Text("%s", logWindowBuf); // doing this shuts up the compiler

            ImGui::SetScrollHereY(1.0f);
        } 

        ImGui::End();
    }

    void CoherentUI::AddTextToLogWindowBuffer(const char* str)
    {
        size_t bufLength = strlen(logBuffer);
        size_t strLength = strlen(str);
        size_t position = bufLength; 

        // its too small to store the string. so copy over it.
        // chop off a certain amount at a time. to reduce the n umber of times we do this copy.
        if ((LOGBUF_MAX_SIZE - bufLength) < strLength)
        {
            memmove(logBuffer, (logBuffer + LOGBUF_PURGE_SIZE), (bufLength - LOGBUF_PURGE_SIZE));
            bufLength -= LOGBUF_PURGE_SIZE; 
            logBuffer[bufLength] = '\0';
        }

        // we already ensured that there is enough space left.
        if ((LOGBUF_MAX_SIZE - bufLength) > strLength)
            strncat(logBuffer, str, strLength);
    }
}