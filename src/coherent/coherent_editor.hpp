/*
    C    O    H    E    R    E    N    T
    Extensible Emulator Debugging Tools!

    Coherent is an extensible debugger for emulators that is intended to allow the debugging of multiple types of CPU cores in an easy way.

    coherent_editor.hpp: Coherent Memory Editor base code.
*/

#pragma once
#include <coherent/coherent.hpp>

namespace Motion
{
    class CoherentEditor : public CoherentExtension
    {
    public: 
        struct Settings
        {
            uint8_t* buf;
            size_t bufSize;     /// buffer size
            size_t lineSize;    /// @brief the number of lines
            size_t loadAtOnce;
            const char* name;
        }; 

        CoherentEditor(Component* component, Settings settings) : CoherentExtension(component) 
        { 
            this->settings = settings;
            SetDefaultSettings();
            editors.push_back(this);
        };

        ~CoherentEditor();

        void AddUI() override; 

        /// @brief utility method to set the default settings
        void SetDefaultSettings();

        /// @brief Write this editor's buffer out to the dumps folder.
        void DumpMemory();

        /*
            Dumping is otherwise only reachable through the editor's menu, which is no use when the
            machine is being run headless or from a script - and the interesting moment is usually one
            you cannot click fast enough for anyway. Every editor registers itself here so the whole
            set can be written out at once, see dumpOnConsoleMatch.
        */
        static void DumpAll(const char* reason);

    private:
        inline static std::vector<CoherentEditor*> editors;

        Settings settings; 

        bool shutupFatalError = false;

        size_t startDrawingAt = 0; 
        char startAddressInputAtBuf[STRING_MAX_SHORT] = {0};
        
        /// @brief determines if the current address to be viewed is valid
        bool addressIsValid = true; 

    }; 
}