/*
    C    O    H    E    R    E    N    T
    Extensible Emulator Debugging Tools!

    Coherent is an extensible debugger for emulators that is intended to allow the debugging of multiple types of CPU cores in an easy way.

    coherent_editorcpp: Coherent Memory Editor base code.
*/

#include <coherent/coherent_editor.hpp>

namespace Motion
{
    Cvar* dumpsFolder;

    // DUMP memory
    CoherentEditor::~CoherentEditor()
    {
        std::erase(editors, this);
    }

    void CoherentEditor::DumpAll(const char* reason)
    {
        Logger::Log(std::format("Dumping {} memory editors: {}", editors.size(), reason).c_str());

        for (CoherentEditor* editor : editors)
            editor->DumpMemory();
    }

    void CoherentEditor::DumpMemory()
    {
        // first check if the file exists
        int32_t dump = 0;

        dumpsFolder = Cvar::Get("dumpsFolder", "./dumps");

        if (!std::filesystem::exists(dumpsFolder->GetString()))
        {
            // create the directory
            std::filesystem::create_directory(dumpsFolder->GetString()); 
        }   
        
        char dumpFile[STRING_MAX_LONG] = {0};
        char formattedEditorName[STRING_MAX_SHORT] = {0}; // null term
        int32_t dumpFileNr = 0;
        bool dumpFileAlreadyExists = true; 
        
        int32_t j = 0; 

        // also we'll format the editor name to have no strings and normalise it to be lower-case too. maybe this should be a util method
        for (int i = 0; i < strlen(settings.name); i++)
        {
            if (isspace(settings.name[i]))
                continue;
            else
                formattedEditorName[j] = tolower(settings.name[i]);

            j++;         
        }

        while (dumpFileAlreadyExists)
        {
            // format the file 
            snprintf(dumpFile, STRING_MAX_LONG, "%s/dump_%s_%04d.bin", dumpsFolder->GetString(), formattedEditorName, dumpFileNr);

            if (!std::filesystem::exists(dumpFile))
            {
                dumpFileAlreadyExists = false;
                break; 
            }
            
            // file exists so don't dump it
            dumpFileNr++;
        }

        // open the file
        FileStream* file = Filesystem::Open(dumpFile, (Motion::FileFlags)(FileFlags::Binary | FileFlags::CreateOrOpen));

        if (!file)
        {
            Logger::Log(std::format("Failed to open dump file {}!", dumpFile).c_str(), LogChannels::Error);
            return;
        }

        file->stream.write((char*)(settings.buf), settings.bufSize); // write it
        Filesystem::Close(file);

        Logger::Log(std::format("Dumped {} data to {}", settings.name, dumpFile).c_str());
    }

    void CoherentEditor::AddUI()
    {
        ImGui::SetNextWindowSize(ImVec2(650, 350));

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

        if (ImGui::Begin(name, &enabled, ImGuiWindowFlags_MenuBar))
        {
            // temporary thing for v0.1.1 since we already wrote some code to do this

            // menu item
            if (ImGui::BeginMenuBar())
            {
                if (ImGui::MenuItem("Dump Memory"))
                    DumpMemory();
            
                ImGui::EndMenuBar();
            }


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
                if (ImGui::BeginChild("##HexView", ImVec2(450, 350)))
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
                }

            done:

                ImGui::EndChild();
                ImGui::SameLine();
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(1.0f, ImGui::GetStyle().ItemSpacing.y));

                // draw an ascii view
                if (ImGui::BeginChild("##AsciiView", ImVec2(200, 350)))
                {

                    for (currentAddress = startDrawingAt; currentAddress < (startDrawingAt + settings.loadAtOnce); currentAddress += settings.lineSize)
                    {
                        for (int32_t j = 0; j < settings.lineSize; j++)
                        {
                            char ch = settings.buf[currentAddress + j];
                            if (isprint(ch))
                                ImGui::Text("%c", settings.buf[currentAddress + j]);
                            else
                                ImGui::Text(".");

                            ImGui::SameLine();

                            // in the case where the user's chosen address partially overlaps the end of the buffer.
                            if ((currentAddress + j) >= settings.bufSize)
                                goto done2; 
                        }

                        ImGui::NewLine();

                        // in the case where the user's chosen address partially overlaps the end of the buffer.
                        if (currentAddress >= settings.bufSize)
                            goto done2; 
                    }

                }
            done2:
                ImGui::PopStyleVar();
                ImGui::EndChild();
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