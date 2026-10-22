
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
            ImGui::InputScalar("##BaseAddressInput", ImGuiDataType_U64, &symbolSet.baseAddress, &step, &stepFast, "%16llX", ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::TextColored(CoherentUI::COLOUR_HEADER, "New Symbol");
            ImGui::Text("Symbol Name: ");
            ImGui::SameLine();
            ImGui::InputTextWithHint("##SymbolNameInput", "Enter name...", symbolNameBuf, STRING_MAX_LONG, ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::Text("Address: ");
            ImGui::SameLine();

            bool addressEntered = false;
            
            if (!offsetAddressChecked)
                addressEntered = ImGui::InputTextWithHint("##AddressInput", "Enter physical address...", addressBuf, STRING_MAX_SHORT, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue);
            else
                addressEntered = ImGui::InputTextWithHint("##AddressInput", "Enter offset address...", addressBuf, STRING_MAX_SHORT, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::SameLine();
            ImGui::Checkbox("Use Offset Addresses", &offsetAddressChecked);

            ImGui::Text("Length (optional)");
            ImGui::SameLine();
            ImGui::InputScalar("##LengthInput", ImGuiDataType_U64, &symbolSizeBuf, &step, &stepFast, "%16llX", ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::Text("If you don't put a length in, it will try to determine the length for you while writing");

            ImGui::Text("Symbol Type:");
            ImGui::SameLine();
            ImGui::Combo("##SymbolTypeDropdown", &symbolSelectedType, "Function\0Global\0Struct Record\0Struct Field\0\0"); // MUST END WITH TWO NULL BYTES

            if (ImGui::Button("Add"))
            {
                CoherentSymbol symbol;

                symbol.type = (CoherentSymbolType)symbolSelectedType;
                symbol.offset = atoi(addressBuf);
                symbol.SetName(symbolNameBuf);
                symbol.length = symbolSizeBuf;

                // copy it in
                symbolSet.symbols[symbol.offset] = symbol;
            }

            ImGui::TextColored(CoherentUI::COLOUR_HEADER, "Symbols");

            int32_t symbolId = 0;

            for (auto& [key, value] : symbolSet.symbols) // has side efefcts ?!
            {
                const char* typeStr = "Function";

                switch (value.type)
                {
                    case CoherentSymbolType::Global:
                        typeStr = "Global";
                        break;
                    case CoherentSymbolType::StructEntry:
                        typeStr = "Struct Entry";
                        break;
                    case CoherentSymbolType::StructRecord:
                        typeStr = "Struct Record";
                        break;
                }

                ImGui::Text("0x%lX: Symbol %d (%s):", value.offset, symbolId, typeStr);
                ImGui::SameLine(); // name highlighed
                char* name = value.GetName();
                ImGui::TextColored(CoherentUI::COLOUR_HEADER, name);

                symbolId++;
            } 

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