
#include <coherent/coherent_symbol.hpp>

namespace Motion
{
    void CoherentSymbolFormatGL2::Read(const char* fileName, CoherentSymbolSet& set)
    {
        if (!OpenFile(fileName))
            return; 
    }

    void CoherentSymbolFormatGL2::Write(const char* fileName, CoherentSymbolSet& set)
    {
        if (!OpenFile(fileName, true))
            return;
    }
}