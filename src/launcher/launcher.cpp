#include <base/program.hpp>
#include <launcher/launcher.hpp>
#include <base/profile/profile.hpp>

namespace Motion
{
    void Launcher::Start()
    {  

    }
    void Launcher::RenderEmulatorAbout()
    {
        if (ImGui::Begin("About", &aboutWindowState.enabled))
        {
            if (aboutWindowState.scrollPos == ABOUT_WINDOW_POS_NOT_SET_YET)
                aboutWindowState.scrollPos = ImGui::GetWindowWidth();

            ImGui::SetWindowFontScale(2.0f);
            ImGui::TextColored(ImVec4(0.2, 0.2, 0.9, 1.0), "Motion!");
            ImGui::SetWindowFontScale(1.0f);

            ImGui::Text("An emulator for very expensive old computers");
            ImGui::Text("© 2026 starfrost and contributors");
            ImGui::SetWindowFontScale(1.5f);
            ImGui::Text("Thanks to:");
            ImGui::SetWindowFontScale(1.0f);

            aboutWindowState.scrollPos -= ImGui::GetIO().DeltaTime * aboutWindowState.speed;

            int32_t textSizeX = ImGui::CalcTextSize(ABOUT_WINDOW_TICKER_TEXT).x;

            // if it's off screen put it back
            if (aboutWindowState.scrollPos < -textSizeX)
                aboutWindowState.scrollPos = (ImGui::GetWindowWidth() + textSizeX); // epsilon
            // can also go in reverse
            if (aboutWindowState.scrollPos > ImGui::GetWindowWidth() + textSizeX)
                aboutWindowState.scrollPos = -textSizeX;

            ImGui::SetCursorPosX(aboutWindowState.scrollPos);
            ImGui::Text(ABOUT_WINDOW_TICKER_TEXT);

            if (ImGui::Button("OK"))
                aboutWindowState.enabled = false; 

            ImGui::SameLine(); 
            // AI would never add this feature
            if (ImGui::Button("+ Ticker Speed"))
                aboutWindowState.speed += 10.0f;

            ImGui::SameLine(); 
            if (ImGui::Button("-"))
                aboutWindowState.speed -= 10.0f;
            ImGui::SameLine();

            ImGui::Text("Ticker Speed: %.1f", aboutWindowState.speed);
        }

        ImGui::End();
    }

    void Launcher::RenderGridItem(ImVec2 size, Cvar* cvar, GridCvarType type, const char* friendlyName)
    {
        if (!cvar)
            return;

        if (ImGui::BeginChild("##gridItem", size))
        {
            const char* name = cvar->GetName();
            float floatValue = cvar->GetValue();

            // DISGUSTING hack 0.01 seconds before 0.2.0-rc1
            int32_t intValue = static_cast<int32_t>(floatValue);
            bool boolValue = static_cast<bool>(floatValue);

            ImGui::Text("%s: ", friendlyName); 
            ImGui::SameLine();

            bool checkboxHit = false; 
            char buf[STRING_MAX_CVAR] = {0};

            char nameBuf[STRING_MAX_SHORT];
            
            // WARNING: This Code SUCSK!
            switch (type)
            {
                case GridCvarType::Boolean:
                    snprintf(nameBuf, STRING_MAX_SHORT, "##Checkbox%d", lastImguiNum);
                    if (ImGui::Checkbox(nameBuf, &boolValue))
                    {
                        // because our crappy cvar api depends on strings we have to do this
                        (checkboxHit) ? Cvar::Set(name, "1") : Cvar::Set(name, "0");
                    }
                    break;
                case GridCvarType::Integer:
                    snprintf(nameBuf, STRING_MAX_SHORT, "##IntEntry%d", lastImguiNum);
                    if (ImGui::InputInt(nameBuf, &intValue))
                        Cvar::Set(name, std::to_string(intValue).c_str());
                    break;
                case GridCvarType::Float:
                    snprintf(nameBuf, STRING_MAX_SHORT, "##FloatEntry%d", lastImguiNum);
                    if (ImGui::InputFloat(nameBuf, &floatValue))
                        Cvar::Set(name, std::to_string(floatValue).c_str());
                    break;
                case GridCvarType::String:
                    snprintf(nameBuf, STRING_MAX_SHORT, "##StringEntry%d", lastImguiNum);
                    if (ImGui::InputTextWithHint(nameBuf, cvar->GetString(), buf, STRING_MAX_LONG, ImGuiInputTextFlags_EnterReturnsTrue))
                        Cvar::Set(name, buf);
                    break;
            }

            // we don't care about the name or label but imgui does.
            // so increment this so imgui doesn't complain about duplicated elementssss

            lastImguiNum++;

        } 

        ImGui::EndChild();
    }
     
    void Launcher::Frame()
    {
        Program::GetRenderer()->FramePreRender();
 
        // can't be closed
        if (ImGui::Begin("Launcher", nullptr, ImGuiWindowFlags_MenuBar))
        {
            if (ImGui::BeginMenuBar())
            {
                if (ImGui::MenuItem("About"))
                    aboutWindowState.enabled = true;

                ImGui::EndMenuBar();
            }
            
            // reset the number
            lastImguiNum = 0; 

            ImGui::SetWindowFontScale(2.0f);
            ImGui::Text("Motion Launcher");
            ImGui::SetWindowFontScale(1.0f);

            ImVec2 gridEntrySize = ImVec2(400, 250);

            RenderGridItem(gridEntrySize, machineName, GridCvarType::String, "Machine Name");
            RenderGridItem(gridEntrySize, profileFolder, GridCvarType::String, "Profile Folder");
            ImGui::SameLine();
            RenderGridItem(gridEntrySize, ramInstalled, GridCvarType::Integer, "RAM (bytes)");
            ImGui::SameLine();
            RenderGridItem(gridEntrySize, numBitplanes, GridCvarType::Integer, "Bitplanes (4 = 1 BP3 board)");
            ImGui::SameLine();
            RenderGridItem(gridEntrySize, startPaused, GridCvarType::Boolean, "Start Paused");
            RenderGridItem(gridEntrySize, skipLauncher, GridCvarType::Boolean, "Skip Launcher (only effective on next boot)");
            ImGui::SameLine();
            RenderGridItem(gridEntrySize, vidScale, GridCvarType::Float, "Video Scale (1-4)");
            ImGui::SameLine();
            RenderGridItem(gridEntrySize, logChannels, GridCvarType::Integer, "Log Channel Mask");
            ImGui::SameLine();
            RenderGridItem(gridEntrySize, logDestinations, GridCvarType::Integer, "Log Destination Mask");
            ImGui::SameLine();
            RenderGridItem(gridEntrySize, profileDisk0Path, GridCvarType::String, "Disk 0 Path");
            ImGui::SameLine();
            RenderGridItem(gridEntrySize, profileDisk1Path, GridCvarType::String, "Disk 1 Path");

            ImGui::NewLine();
            if (ImGui::Button("Go"))
                Program::SetState(ProgramState::Emulation);
        }
        
        ImGui::End();

        if (aboutWindowState.enabled)
            RenderEmulatorAbout();

        Program::GetRenderer()->FramePostRender();
    }
};