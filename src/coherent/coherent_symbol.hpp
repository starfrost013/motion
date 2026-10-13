
/*
    C    O    H    E    R    E    N    T
    Extensible Emulator Debugging Tools!

    Coherent is an extensible debugger for emulators that is intended to allow the debugging of multiple types of CPU cores in an easy way.

    coherent_symbol.hpp: Coherent Symbolic Debugging System
*/

#pragma once
#include <coherent/coherent.hpp>

namespace Motion
{
    /// @brief maximum length of a symbol set. picked ot reduce dynamic allocation
    #define SYMBOL_NAME_MAX_LENGTH              128

    // CSYM stuff
    #define COHERENT_SYMBOL_FORMAT_EXTENSION    ".csym"
    #define COHERENT_SYMBOL_FORMAT_VERSION      1
    #define COHERENT_SYMBOL_FORMAT_MAGIC        0x20261013 // October 13, 2026
    
    /// @brief a symbolic information set
    class CoherentSymbolSet
    {
        std::unordered_map<uint64_t, CoherentSymbol> symbols; 
    };

    /// @brief a symbol file format
    class CoherentSymbolFormat
    {
    public:
        
        static void Read(const char* fileName, CoherentSymbolSet& set) { }
        static void Write(const char* fileName, CoherentSymbolSet& set) { }
    private:
        static FileStream* stream; 

        static bool OpenFile(const char* fileName);
        static void CloseFile();
    }; 

    // todo: allow loading dwarf, pdb, e.t.c.
    class CoherentSymbolFormatCsym : public CoherentSymbolFormat
    {
    public:
        class Header
        {
            uint32_t magic;
            uint8_t version; 
            uint64_t nrOfSymbols;       // with the rise of ai 4,294,967,296 symbols is no longer a dream 
            uint64_t firstSymbolPtr;
        };

        static void Read(const char* fileName, CoherentSymbolSet& set);
        static void Write(const char* fileName, CoherentSymbolSet& set); 
    };

    enum CoherentSymbolType
    {
        Function = 0,
        Global = 1,
        StructRecord = 2,
        StructEntry = 3
    }; 

    /// @brief an individual symbol
    class CoherentSymbol
    {        
        uint64_t addr;                      // relative(?) address of 
        CoherentSymbolType type; 
        // Getters for privates
        char* GetName() { return name; }; 

        // Setters for privates

    private: 
        char name[SYMBOL_NAME_MAX_LENGTH];
    }; 

 
};