
/*
    C    O    H    E    R    E    N    T
    Extensible Emulator Debugging Tools!

    Coherent is an extensible debugger for emulators that is intended to allow the debugging of multiple types of CPU cores in an easy way.

    coherent_symbol_editor.cpp: Coherent symbol editor

*/

#include <coherent/coherent.hpp>
#include <coherent/coherent_symbol.hpp>

namespace Motion
{
    // this extension's lifetime is managed by coherentt itself so we can't easily attach it to a component.

    void CoherentExtensionSymbolEditor::Init()
    {
        singleton = new CoherentExtensionSymbolEditor();
        Coherent::RegisterExtension(singleton);
    }

    void CoherentExtensionSymbolEditor::Shutdown()
    {
        delete singleton; 
    }

    void CoherentExtensionSymbolEditor::LoadCurrentSet(const char* fileName)
    {
        CoherentSymbolFormatCsym::Read(fileName, symbolSet);
    }  
        
    void CoherentExtensionSymbolEditor::SaveCurrentSet(const char* fileName)
    {
        CoherentSymbolFormatCsym::Write(fileName, symbolSet);
    }
    
    void CoherentExtensionSymbolEditor::AddUI()
    {
        if (ImGui::Begin("Symbol Editor"))
        {
            ImGui::TextColored(CoherentUI::COLOUR_HEADER, "Symbol Set Info");

            uint64_t step = 1;
            uint64_t stepFast = 0x1000000;

            ImGui::Text("Base Address: ");
            ImGui::SameLine();
            ImGui::InputScalar("##BaseAddressInput", ImGuiDataType_U64, &symbolSet.baseAddress, &step, &stepFast, "%d", ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::TextColored(CoherentUI::COLOUR_HEADER, "New Symbol");
            ImGui::Text("Symbol Name: ");
            ImGui::SameLine();
            ImGui::InputTextWithHint("##SymbolNameInput", "Enter name...", symbolNameBuf, STRING_MAX_LONG, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::Text("Address: ");
            ImGui::SameLine();

            bool addressEntered = false;
            
            if (!offsetAddressChecked)
                addressEntered = ImGui::InputTextWithHint("##AddressInput", "Enter physical address...", addressBuf, STRING_MAX_SHORT, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue);
            else
                addressEntered = ImGui::InputTextWithHint("##AddressInput", "Enter offset address...", addressBuf, STRING_MAX_SHORT, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::SameLine();
            ImGui::Checkbox("Use Offset Addresses", &offsetAddressChecked);

            ImGui::TextColored(CoherentUI::COLOUR_HEADER, "Symbols");
            
            if (ImGui::Button("Load from .csym"))
                LoadCurrentSet("test0000.csym");
            
            ImGui::SameLine();
            
            if (ImGui::Button("Save"))
                SaveCurrentSet("test0000.csym");

            ImGui::SameLine();
            ImGui::Button("Set as current symbol set");
        }

        ImGui::End();
    }
};