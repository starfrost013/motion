
/*
    C    O    H    E    R    E    N    T
    Extensible Emulator Debugging Tools!

    Coherent is an extensible debugger for emulators that is intended to allow the debugging of multiple types of CPU cores in an easy way.

    coherent_symbol_format_gl2.cpp: Coherent Symbolic Debugging System GL2 format import
*/

#pragma once
#include <coherent/coherent.hpp>
#include <coherent/coherent_symbol.hpp>

namespace Motion
{
    #define COHERENT_LOG_PREFIX_GL2     "Coherent A.out-GL2 Symbol Parser"

    void CoherentSymbolFormatGL2::Read(const char* fileName, CoherentSymbolSet& set)
    {
        if (!OpenFile(fileName))
            return;

        //TODO : Multibus endian (big endian with swapped lanes like in HDD dump) and actual IRIS BE32 format

        // gl2 a.out header 
        CoherentSymbolFormatGL2::GL2Header gl2header; 

        // read in the header
        file->stream.read((char*)(&gl2header), sizeof(CoherentSymbolFormatGL2::GL2Header));
        
        bool validMagic = true;
        const char* hdrTypeString = "GL2 a.out object file";

        switch (gl2header.magic)
        {
            case GL2_HEADER_MAGIC_BINARY:
                break;
            case GL2_HEADER_MAGIC_DEMAND_PAGED: 
                hdrTypeString = "GL2 a.out object file (demand paged)";
                break;
            case GL2_HEADER_MAGIC_OBJECT:
                hdrTypeString = "GL2 a.out object file (impure e.g. kernel)";
                break;
            default:
                validMagic = false;
                break; 
        }

        if (!validMagic)
        {
            Logger::Log(COHERENT_LOG_PREFIX_GL2, std::format("Invalid magic in GL2 binary: 0x{:x}", gl2header.magic).c_str(), LogChannels::Error);
            return; 
        }

        if (gl2header.symSize == 0
        || gl2header.symSize >= Filesystem::GetSize(file))
        {
            Logger::Log(COHERENT_LOG_PREFIX_GL2, std::format("GL2 binary has unreasonable symbol size {}", gl2header.symSize).c_str(), LogChannels::Error);
            return; 
        }

        // symbols should be after this
        uint64_t symLocation = sizeof(CoherentSymbolFormatGL2::GL2Header) + gl2header.textSize + gl2header.dataSize + gl2header.textRelocSize + gl2header.dataRelocSize;
        uint64_t symBytesRead = 0, numSymsRead = 0;
        
        CoherentSymbolFormatGL2::GL2SymbolEntry symbolEntry; 
        CoherentSymbol finalSymbol; 
        char nameBuf[STRING_MAX_LONG] = {0};

        while (!Filesystem::IsEOF(file) && symLocation < symBytesRead)
        {
            file->stream.read((char*)(&symbolEntry), sizeof(CoherentSymbolFormatGL2::GL2SymbolEntry));

            // read in the name ptr
            auto location = Filesystem::Tell(file);

            // seek to the string
            Filesystem::Seek(file, symbolEntry.namePtr);

            // read the string
            Filesystem::ReadString(file, nameBuf, STRING_MAX_LONG);

            Filesystem::Seek(file, location); // go back

            finalSymbol.SetName(nameBuf);
            finalSymbol.offset = symbolEntry.symbolAddress;

            if (symbolEntry.flags & GL2SymbolType::Data)
                finalSymbol.type = CoherentSymbolType::Global;
            else if (symbolEntry.flags & GL2SymbolType::Function)
                finalSymbol.type = CoherentSymbolType::Function;

            // add it
            set.symbols[finalSymbol.offset] = finalSymbol;

            // roll
            numSymsRead++;
        }


    }

    void CoherentSymbolFormatGL2::Write(const char* fileName, CoherentSymbolSet& set)
    {
        if (!OpenFile(fileName))
            return;

        Logger::Log(COHERENT_LOG_PREFIX_GL2, "Writing GL2 format files is not supported!", LogChannels::Debug);
    }
}