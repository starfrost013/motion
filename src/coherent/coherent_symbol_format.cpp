
/*
    C    O    H    E    R    E    N    T
    Extensible Emulator Debugging Tools!

    Coherent is an extensible debugger for emulators that is intended to allow the debugging of multiple types of CPU cores in an easy way.

    coherent_symbol_format.cpp: Coherent CSYM file format

    Symbol files are static classes that are used to serialise CoherentSymbolSet class instances to files.

    CSYM specification:

*/

#include <coherent/coherent.hpp>
#include <coherent/coherent_symbol.hpp>

namespace Motion
{
    /// @brief ensure that a symbol file is open
    /// @param fileName 
    bool CoherentSymbolFormat::OpenFile(const char* fileName, bool write)
    {
        FileFlags flags = FileFlags::Binary;

        if (write)
            flags = (FileFlags)(FileFlags::Binary | FileFlags::Create);

        file = Filesystem::Open(fileName, flags); 

        if (file == nullptr)
            Logger::Log(COHERENT_SYMBOL_LOG_PREFIX, std::format("CoherentSymbolFormat::OpenFile - Failed to open file {} for {}!", fileName, (write) ? "writing" : "reading").c_str(), LogChannels::Error);

        return (file != nullptr);
    }

    ///
    // CoherentSymbolFormatCsym
    ///

    void CoherentSymbolFormatCsym::Read(const char* fileName, CoherentSymbolSet& set)
    {
        if (!OpenFile(fileName))
            return;
      
        CoherentSymbol symbol;

        CoherentSymbolFormatCsym::Header header;
        file->stream.read((char*)(&header), sizeof(CoherentSymbolFormatCsym::Header));
        
        if (header.magic != COHERENT_SYMBOL_FORMAT_MAGIC)
        {
            Logger::Log(COHERENT_SYMBOL_LOG_PREFIX, std::format("CoherentSymbolFormat::Read ({}) - magic is {:x}, should be {:x}", fileName, header.magic, COHERENT_SYMBOL_FORMAT_MAGIC).c_str(), LogChannels::Error);
            goto done;          
        }

        if (header.version != COHERENT_SYMBOL_FORMAT_VERSION)
        {
            Logger::Log(COHERENT_SYMBOL_LOG_PREFIX, std::format("CoherentSymbolFormat::Read ({}) - version is {:x}, should be {:x}", fileName, header.version, COHERENT_SYMBOL_FORMAT_VERSION).c_str(), LogChannels::Error);
            goto done;          
        }

        if (header.nrOfSymbols == 0)
        {
            Logger::Log(COHERENT_SYMBOL_LOG_PREFIX, std::format("CoherentSymbolFormat::Read ({}) - no symbols!", fileName).c_str(), LogChannels::Error);
            goto done;          
        }


        for (uint64_t i = 0; i < header.nrOfSymbols; i++)
        {
            file->stream.read((char*)(&(symbol.offset)), sizeof(CoherentSymbol));
            file->stream.read((char*)(&(symbol.type)), sizeof(CoherentSymbolType));
            file->stream.read((char*)(&(symbol.nameLength)), sizeof(uint16_t));

            // make sure the name length is sane
            if (symbol.nameLength >= COHERENT_SYMBOL_NAME_MAX_LENGTH)
            {
                Logger::Log(COHERENT_SYMBOL_LOG_PREFIX, std::format("CoherentSymbolFormat::Read ({}) - unreasonable symbol length {}!", fileName, symbol.nameLength).c_str(), LogChannels::Error);
                goto done; // probably not safe to keep going              
            }

            file->stream.read((char*)(&symbol.name), symbol.nameLength);

            if (Filesystem::IsEOF(file))
            {
                Logger::Log(COHERENT_SYMBOL_LOG_PREFIX, std::format("CoherentSymbolFormat::Read ({}) - unexpected end of file!", fileName).c_str(), LogChannels::Error);
                goto done; // probably not safe to keep going                 
            }

            if (set.symbols.count(symbol.offset) >= 1)
            {
                Logger::Log(COHERENT_SYMBOL_LOG_PREFIX, std::format("CoherentSymbolFormat::Read ({}) - symbol at 0x{:x} already defined, skipping", 
                    fileName, symbol.offset).c_str(), LogChannels::Error);
                
                // already read whole thing, can go
                continue;
            }

            set.symbols[symbol.offset] = symbol;

            Logger::Log(COHERENT_SYMBOL_LOG_PREFIX, std::format("CoherentSymbolFormat::Read ({}) - added symbol {}, type {} at 0x{:x}!", 
                fileName, symbol.GetName(), (int32_t)symbol.type, symbol.offset).c_str(), LogChannels::Debug);
        }
        
    done:
        Filesystem::Close(file);
    }

    void CoherentSymbolFormatCsym::Write(const char* fileName, CoherentSymbolSet& set)
    {
        if (!OpenFile(fileName, true))
            return;

        CoherentSymbolFormatCsym::Header header; 

        header.version = COHERENT_SYMBOL_FORMAT_VERSION;
        header.magic = COHERENT_SYMBOL_FORMAT_MAGIC;
        header.nrOfSymbols = set.symbols.size();
        header.exeBaseAddress = set.baseAddress;

        file->stream.write((char*)&(header), sizeof(CoherentSymbolFormatCsym::Header));

        Logger::Log(COHERENT_SYMBOL_LOG_PREFIX, "Wrote .CSYM header", LogChannels::Debug);

        uint64_t numSymbolsWritten = 0; 
        // write out the symbols 
        for (const auto& [key, value] : set.symbols)
        {
            file->stream.write((char*)(&(value.offset)), sizeof(CoherentSymbol));
            file->stream.write((char*)(&(value.type)), sizeof(CoherentSymbolType));
            file->stream.write((char*)(&(value.length)), sizeof(uint64_t));
            file->stream.write((char*)(&(value.nameLength)), sizeof(uint16_t));    
            numSymbolsWritten++;     
        }

        Logger::Log(COHERENT_SYMBOL_LOG_PREFIX, std::format("Wrote {} symbols to .CSYM folder", numSymbolsWritten).c_str(), LogChannels::Debug);

        Filesystem::Close(file);
    }

    ///
    // CoherentSymbolSet
    ///

    /// @brief resolve an address to a symbol
    /// @param address 
    CoherentSymbol* CoherentSymbolSet::ResolveAddress(uint64_t address, CoherentSymbolSearchMode searchMode)
    {
        // subtract to get absolute addr
        if (searchMode == CoherentSymbolSearchMode::Absolute)
            address -= baseAddress;

        // linear search for now. if it's slow change it 
        for (auto& [ key, value ] : symbols)
        {
            if (address >= (value.offset)
            && address < (value.offset + value.length)) // range is [inclusive,exclusive]
                return &value; // c+=26 use std::optional ???????????????????
        }

        return nullptr;
    }
}