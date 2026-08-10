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
        };

        void AddUI() override; 

        /// @brief utility method to set the default settings
        void SetDefaultSettings();
    private:
        Settings settings; 

        bool shutupFatalError = false;

        size_t startDrawingAt = 0; 
        char startAddressInputAtBuf[STRING_MAX_SHORT] = {0};
        
        /// @brief determines if the current address to be viewed is valid
        bool addressIsValid = true; 

    }; 
}