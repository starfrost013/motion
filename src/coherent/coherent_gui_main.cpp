/*
    C    O    H    E    R    E    N    T
    Extensible Emulator Debugging Tools!

    Coherent is an extensible debugger for emulators that is intended to allow the debugging of multiple types of CPU cores in an easy way.
*/

// coherent_gui.cpp: Coherent Imgui Implementation

// not sure where else to put this. I didn't want it to be accessible from outside of UI parts, but i don't feel like a UI backend is a good use of time rn

#include <base/program.hpp>
#include <coherent/coherent.hpp>
#include <coherent/coherent_gui_imgui.hpp>
#include <component/cpu/cpu.hpp>

namespace Motion
{
    /// @brief a basic about window since coherent is retargetable
    void CoherentUI::DrawAboutWindow()
    {
        if (ImGui::Begin("About", &CoherentUI::aboutActive))
        {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), COHERENT_VERSION);
            ImGui::Text("An architecture-agnostic, retargetable, embedded debugger for emulators");
            ImGui::Text("© 2026 starfrost:");
            ImGui::SameLine();
            ImGui::TextLinkOpenURL("https://starfrost.net/");

            if (ImGui::Button("Close"))
                CoherentUI::aboutActive = false; 
        }

        ImGui::End();
    }

    void CoherentUI::DrawMainWindow()
    {
        ImGui::SetNextWindowPos(ImVec2(
        (Program::GetRenderer()->GetWindowSizeX() / 2) - 800,
        (Program::GetRenderer()->GetWindowSizeY() / 2) - 600), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(800, 600));

        int i = 0, pcOffset = 0;
        ImVec2 topBarSize = ImVec2(800, 25);
        
        ImVec2 registerPaneSize = ImVec2(180, 600);
        ImVec2 disasmPaneSize = ImVec2(340, 600);
        ImVec2 debugContainerPaneSize = ImVec2(290, 600);
        ImVec2 debugContainerChildWindowSize = ImVec2(290, 170);

        for (CoherentExtension* extension : Coherent::extensions)
        {
            if (extension->enabled
            && extension->GetExtensionType() != CoherentExtensionType::CustomMenu)
                extension->AddUI();
        }

        if (ImGui::Begin("Debugger", &Coherent::active, ImGuiWindowFlags_MenuBar))
        {
            if (ImGui::BeginMenuBar())
            {
                // Extensions

                if (ImGui::BeginMenu("Peripherals"))
                {
                    // see which extensions are enabled 

                    for (CoherentExtension* extension : Coherent::extensions)
                    {
                        if (extension->GetExtensionType() == CoherentExtensionType::PeripheralsMenu)
                            if (ImGui::MenuItem(extension->component->GetName()))
                                extension->enabled = true;
                    }

                    ImGui::EndMenu();
                }

                // Add custom menu type extensions
                for (CoherentExtension* extension : Coherent::extensions)
                {
                    const char* name = extension->component->GetName();

                    // if the extension has a specified menu name do that
                    if (extension->GetMenuName()[0] != '\0')
                        name = extension->GetMenuName();

                    if (extension->GetExtensionType() == CoherentExtensionType::CustomMenu)
                    {

                        // we checked already so it can only be custommenunochildren
                        bool clicked = false;

                        // tell the extension that we clicked it in case they don't want to add any child menu options
                        clicked = ImGui::BeginMenu(name);
                        extension->enabled = clicked;

                        if (clicked)
                        {
                            extension->AddUI();
                            ImGui::EndMenu();                              
                        }
                    }
                    else if (extension->GetExtensionType() == CoherentExtensionType::CustomMenuItem)
                    {
                        bool clicked = ImGui::MenuItem(name);

                        if (clicked)
                            extension->enabled = true;
                    }
                }

                // Misc options menu

                if (ImGui::BeginMenu("Options"))
                {
                    ImGui::MenuItem("Break on Exception", nullptr, &Coherent::breakOnException);
                    ImGui::EndMenu();
                }

                // Style menu

                if (ImGui::BeginMenu("Style"))
                {
                    if (ImGui::MenuItem("IMGUI Default"))
                        CoherentUI::InitStyle(UIStyle::Default);
                    
                    if (ImGui::MenuItem("SGI mex-style"))
                        CoherentUI::InitStyle(UIStyle::MEX);

                    if (ImGui::MenuItem("Experimental"))
                        CoherentUI::InitStyle(UIStyle::Experimental);

                    ImGui::EndMenu();
                }


                // Help menu

                if (ImGui::BeginMenu("Help"))
                {
                    if (ImGui::MenuItem("About"))
                        CoherentUI::aboutActive = true;

                    ImGui::EndMenu();
                }

                ImGui::EndMenuBar();
            }

            // top pane

            if (ImGui::BeginChild("DebugViewPane", topBarSize))
            {
                if (Coherent::currentSystem == nullptr)
                    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 0.0f), "***** Error - No System Active *****");
                else
                {
                    CoherentSystem::RunState runState = Coherent::currentSystem->GetRunState();

                    if (runState == CoherentSystem::RunState::Running)
                    {
                        if (ImGui::Button("Pause CPU"))
                            Coherent::currentSystem->SetRunState(CoherentSystem::RunState::Paused);
                    }
                    else
                    {
                        if (ImGui::Button("Start CPU"))
                            Coherent::currentSystem->SetRunState(CoherentSystem::RunState::Running); 
                    }

                    ImGui::SameLine();

                    if (ImGui::Button("Reset"))
                        Coherent::currentSystem->SetRunState(CoherentSystem::RunState::Reset);

                    if (runState == CoherentSystem::RunState::Paused
                    || runState == CoherentSystem::RunState::NotYetStarted)
                    {
                        ImGui::SameLine();
                        
                        if (ImGui::Button("Step"))
                            Coherent::currentSystem->SetRunState(CoherentSystem::RunState::SingleStepNormal);
                        
                        ImGui::SameLine();
                    }

                    ImGui::SameLine();
                    ImGui::Text("Clock Speed: %.2f MHz", ((float)Emulation::GetMachine()->FindComponentByType<ComponentCPU>()->GetClockSpeed()) / 1000000.0);
                }
                
            }  

            ImGui::EndChild();

            char nameBuf[STRING_MAX_SHORT] = {0};
            uint32_t id = 0;
            // "left" (register) pane
            if (ImGui::BeginChild("RegisterPane", registerPaneSize))
            {
                ImGui::TextColored(CoherentUI::COLOUR_HEADER, "Registers");

                for (auto& aRegister : Coherent::currentSystem->registers)
                {
                    auto value = aRegister->Read();

                    snprintf(nameBuf, STRING_MAX_SHORT, "##RegisterPane%d", id);
                    id++;

                    // evil
                    // throw the current value into aRegister->valBuf
                    if (value.type() == typeid(uint8_t)
                    || value.type() == typeid(int8_t))
                    {
                        uint8_t formattedValue = std::any_cast<uint8_t>(value);
                        snprintf(aRegister->valBuf, STRING_MAX_SHORT, "%02x", formattedValue);    
                    }
                    else if (value.type() == typeid(uint16_t)
                    || value.type() == typeid(int16_t))
                    {
                        uint16_t formattedValue = std::any_cast<uint16_t>(value);
                        snprintf(aRegister->valBuf, STRING_MAX_SHORT, "%04x", formattedValue);    
                    }
                    else if (value.type() == typeid(uint32_t)
                    || value.type() == typeid(int32_t))
                    {
                        uint32_t formattedValue = std::any_cast<uint32_t>(value);
                        snprintf(aRegister->valBuf, STRING_MAX_SHORT, "%08x", formattedValue);    
                    }
                    else if (value.type() == typeid(uint64_t)
                    || value.type() == typeid(int64_t))
                    {
                        uint32_t formattedValue = std::any_cast<uint64_t>(value);
                        snprintf(aRegister->valBuf, STRING_MAX_SHORT, "%16x", formattedValue);    
                    }     
                    
                    // print the name of the register
                    ImGui::Text("%s:", aRegister->name);
                    ImGui::SameLine();

                    // this makes the register text look like normal text
                    CoherentUI::PushStylelessTextBox();
                    
                    // then the value
                    if (ImGui::InputText(nameBuf, aRegister->valBuf, 32, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue))
                    {
                        // do some horrible kludge so we don't need to do this
                        auto newValue = static_cast<uint64_t>(strtoull(aRegister->valBuf, NULL, 16));

                        // for non 64 bit registers this just masks off the bits we don't need
                        aRegister->Write(newValue);
                    }

                    CoherentUI::PopStylelessTextBox();

                    i++;
                }

            }

            ImGui::EndChild();
            ImGui::SameLine();

            // middle pane; disassembly pane
            if (ImGui::BeginChild("DisassemblyPane", disasmPaneSize))
            {
                ImGui::TextColored(CoherentUI::COLOUR_HEADER, "Disassembly");

                // todo: MUST put in a buffer...
                for (i = 0; i < 30; i++)
                {
                    if (i == 0)
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.6f, 0.9f, 1.0f));

                    auto addr = Coherent::currentSystem->GetPC() + pcOffset;

                    ImGui::Text("0x%lx:    %s", addr, Coherent::currentSystem->DisasmInstruction(addr));

                    if (i == 0)
                        ImGui::PopStyleColor();

                    pcOffset += Coherent::currentSystem->GetNextInstructionSize();
                }
            }

            ImGui::EndChild();
            ImGui::SameLine();

            // right pane: debug controls
            if (ImGui::BeginChild("DebugControlsPane", debugContainerPaneSize))
            {
                CoherentUI::DrawGuardWindow(CoherentUI::GuardWindowType::GuardWindowBreakpoint, debugContainerChildWindowSize);
                CoherentUI::DrawGuardWindow(CoherentUI::GuardWindowType::GuardWindowWatchpoint, debugContainerChildWindowSize);
                CoherentUI::DrawStackWindow(debugContainerChildWindowSize);
            }
           
            ImGui::EndChild();
        }

        ImGui::End();
    }
    
     /// @brief draw one of the right-pane guard windows of the Coherent debugger
    /// @param windowType the type of the subwindow to draw 
    /// @param size the size of the window to draw
    void CoherentUI::DrawGuardWindow(CoherentUI::GuardWindowType windowType, ImVec2 size)
    {
        const char* headerText = "";
        // buf used for inserting the guard's addrss
        char* addrBuf = addrBufForWatchpoints;

        // so this is what lambdas are used for
        auto processItem = [](auto& pair, GuardWindowType windowType, int32_t index)
        {
            Coherent::Guard& guard = pair.second;

            if (!guard.enabled) // bp is disabled
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
            else if (guard.active) // bp is active
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.2f, 0.1f, 1.0f));
            else // bp is enabled but not hit
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

            char addrBuf[STRING_MAX_LONG] = {0};

            if (windowType == GuardWindowBreakpoint)
            {
                // some flavour text
                if (!guard.active && guard.enabled)
                    snprintf(addrBuf, STRING_MAX_LONG, "[%d] break at 0x%lx", index, guard.addr);
                else if (!guard.enabled)
                    snprintf(addrBuf, STRING_MAX_LONG, "[%d] break at 0x%lx [disabled]", index, guard.addr);
                else if (guard.active)
                    snprintf(addrBuf, STRING_MAX_LONG, "[%d] break at 0x%lx [hit!]", index, guard.addr);
            }
            else if (windowType == GuardWindowWatchpoint)
            {
                Coherent::Watchpoint& watchpoint = (Coherent::Watchpoint&)pair.second;
                snprintf(addrBuf, STRING_MAX_LONG, "[%d] addr [%lx] = %x", index, guard.addr, watchpoint.GetValue());
            }

            if (ImGui::Selectable(addrBuf))
                guard.selected = !guard.selected;
            
            ImGui::PopStyleColor();
        };

        switch (windowType)
        {
            case GuardWindowBreakpoint:
                headerText = "Breakpoints";
                addrBuf = addrBufForBreakpoints;
                break;
            case GuardWindowWatchpoint:
                headerText = "Watchpoints";
                addrBuf = addrBufForWatchpoints;
                break;
            default: 
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "***** INVALID guard window type created *****");
                return; // code will die anyway
        }       

        // draw the "top" of the window
        if (ImGui::BeginChild(headerText, size, ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
        {                
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 0.8f, 1.0f, 1.0f));
            ImGui::Text("%s", headerText); //shutup compiler by doing this
            ImGui::PopStyleColor();

            // if the user clicked the add button or hit enter, add the guard
            bool createGuard = false;

            if (ImGui::Button("Add"))
                createGuard = true;

            ImGui::SameLine();

            if (ImGui::InputTextWithHint("##AddressInput", "Address...", addrBuf, STRING_MAX_LONG, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue))
            {
                createGuard = true;
                ImGui::SetKeyboardFocusHere(-1); // we want the input box to automatically reselect
            }
        
            if (createGuard
            && strlen(addrBuf) > 0)
            {
                auto addr = (size_t)strtol(addrBuf, NULL, 16);

                if (windowType == GuardWindowBreakpoint)
                {
                    Coherent::Breakpoint breakpoint = Coherent::Breakpoint(addr);
                    breakpoint.enabled = true;
                    Coherent::AddBreakpoint(breakpoint);
                }
                else if (windowType == GuardWindowWatchpoint)
                {
                    Coherent::Watchpoint watchpoint = Coherent::Watchpoint(addr);
                    watchpoint.enabled = true;
                    Coherent::AddWatchpoint(watchpoint);
                }

                // effectively clears the buffer
                addrBuf[0] = '\0';
            }

            int32_t index = 0;

            switch (windowType)
            {
                case GuardWindowBreakpoint:
                    for (auto& guard : Coherent::breakpoints) processItem(guard, windowType, index++);
                    break;
                case GuardWindowWatchpoint:
                    for (auto& guard : Coherent::watchpoints) processItem(guard, windowType, index++);
                    break;
            }   

            // remove button

            if (ImGui::Button("Remove"))
            {
                switch (windowType)
                {
                    case GuardWindowBreakpoint:
                        std::erase_if(Coherent::breakpoints, [](const auto& pair) { return pair.second.selected; });
                        break;
                    case GuardWindowWatchpoint:
                        std::erase_if(Coherent::watchpoints, [](const auto& pair) { return pair.second.selected; });
                        break;
                }
            }
        }
        
        ImGui::EndChild();
    }

    void CoherentUI::DrawStackWindow(ImVec2 size)
    {

        if (ImGui::BeginChild("Stack", size, ImGuiChildFlags_None))
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 0.8f, 1.0f, 1.0f));
            ImGui::Text("%s", "Stack"); //shutup compiler by doing this
            ImGui::PopStyleColor();

            ImVec4 color = ImVec4(1.0, 1.0, 1.0, 1.0);
            
            // not yet started so do not bother
            if (Coherent::currentSystem->GetRunState() != CoherentSystem::RunState::NotYetStarted)
            {
                for (int32_t offset = 0; offset < 8; offset++)
                {
                    switch (Coherent::currentSystem->GetWordSize())
                    {
                        case CoherentSystem::WordSize::WordSize8:
                            ImGui::TextColored(color, "[%d]: 0x%02x", offset, Coherent::currentSystem->GetStack8(offset));
                            break;
                        case CoherentSystem::WordSize::WordSize16:
                            ImGui::TextColored(color, "[%d]: 0x%04x", offset, Coherent::currentSystem->GetStack16(offset));
                            break;
                        case CoherentSystem::WordSize::WordSize32:
                            ImGui::TextColored(color, "[%d]: 0x%08x", offset, Coherent::currentSystem->GetStack32(offset));
                            break;
                        case CoherentSystem::WordSize::WordSize64:
                            ImGui::TextColored(color, "[%d]: 0x%16lx", offset, Coherent::currentSystem->GetStack64(offset));
                            break;
                    }

                    // make it fade out despite its lack of computational sense because it looks cool
                    color.w -= 0.080 - (0.010 * offset) + 0.065;
                }
            }              
        }

        ImGui::EndChild();
    }
    
    void Coherent::Frame()
    {
        CoherentUI::DrawMainWindow();
        CoherentUI::DrawLogWindow();

        if (CoherentUI::aboutActive)
            CoherentUI::DrawAboutWindow();
    }
}
