
/*
    C    O    H    E    R    E    N    T
    Extensible Emulator Debugging Tools!

    Coherent is an extensible debugger for emulators that is intended to allow the debugging of multiple types of CPU cores in an easy way.

    coherent_symbol_format.cpp: Coherent CSYM file format

    CSYM specification:

*/

#include <coherent/coherent.hpp>
#include <coherent/coherent_symbol.hpp>

namespace Motion
{
    /// @brief ensure that a symbol file is open
    /// @param fileName 
    bool CoherentSymbolFormat::OpenFile(const char* fileName)
    {
        stream = Filesystem::Open(fileName, FileFlags::Binary); 
    }

    void CoherentSymbolFormatCsym::Read(const char* fileName, CoherentSymbolSet& set)
    {

    }

    void CoherentSymbolFormatCsym::Write(const char* fileName, CoherentSymbolSet& set)
    {

    }
}