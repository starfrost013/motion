
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
        
    void CoherentExtensionSymbolEditor::AddUI()
    {
        if (ImGui::Begin("Symbol Editor"))
        {

        }

        ImGui::End();
    }
};