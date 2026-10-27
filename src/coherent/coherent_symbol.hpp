
/*
    C    O    H    E    R    E    N    T
    Extensible Emulator Debugging Tools!

    Coherent is an extensible debugger for emulators that is intended to allow the debugging of multiple types of CPU cores in an easy way.

    coherent_symbol.hpp: Coherent Symbolic Debugging System
*/

#pragma once

namespace Motion
{
    /// @brief maximum length of a symbol set. picked ot reduce dynamic allocation
    #define COHERENT_SYMBOL_NAME_MAX_LENGTH     1024                // 1024 due to c++ name mangling

    // CSYM stuff
    #define COHERENT_SYMBOL_FORMAT_EXTENSION    ".csym"
    #define COHERENT_SYMBOL_FORMAT_VERSION      1
    #define COHERENT_SYMBOL_FORMAT_MAGIC        0x746E657265686F43  // Coherent
    #define COHERENT_SYMBOL_RESERVE             128                 // reserve symbols to prevent massive amount of resizing while reading small files

    #define COHERENT_SYMBOL_LOG_PREFIX          "Coherent Symbolic Debugger"
    

    /// @brief the type of symbol 
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
        friend class CoherentSymbolFormatCsym; // ll

    public:  
        uint64_t offset;                // relative(?) address of 
        uint64_t length = 0xFFFFFFFF;   // length (fffffff = auto)
        CoherentSymbolType type; 
        // Getters for privates
        char* GetName() { return name; }; 

        // Setters for privates
        void SetName(const char* name) 
        { 
            strncpy(this->name, name, COHERENT_SYMBOL_NAME_MAX_LENGTH); 
            nameLength = strlen(this->name); // set the name length
        };

    private: 
        char name[COHERENT_SYMBOL_NAME_MAX_LENGTH];
        uint16_t nameLength;            // limit name length to reduce size

    }; 

    /// @brief enumerates the search modes
    enum CoherentSymbolSearchMode
    {
        Absolute = 0,   // search on the basis of absolute addressing
        Relative = 1,   // search on the basis of relative addressing
    };

    /// @brief a symbolic information set
    class CoherentSymbolSet
    {
    public: 
        std::unordered_map<uint64_t, CoherentSymbol> symbols; 
        uint64_t baseAddress;
    
        CoherentSymbolSet() : symbols()
        {
            symbols.reserve(COHERENT_SYMBOL_RESERVE);
        }

        CoherentSymbol* ResolveAddress(uint64_t address, CoherentSymbolSearchMode searchMode = CoherentSymbolSearchMode::Relative);
    
    };

    /// @brief a symbol file format
    class CoherentSymbolFormat
    {
    public:
        
        static void Read(const char* fileName, CoherentSymbolSet& set) { }
        static void Write(const char* fileName, CoherentSymbolSet& set) { }

    protected: 
        static bool OpenFile(const char* fileName, bool write = false);
        inline static FileStream* file; 

    private:

    }; 

    // todo: allow loading dwarf, pdb, e.t.c.
    class CoherentSymbolFormatCsym : public CoherentSymbolFormat
    {
    public:
        struct Header
        {
            uint64_t magic = 0;
            uint8_t version = 0; 
            uint64_t nrOfSymbols = 0;     // with the rise of ai 4,294,967,296 symbols is no longer a dream 
            uint64_t exeBaseAddress = 0;  // base address for binary, for e.g. IDA Pro import
        };

        static void Read(const char* fileName, CoherentSymbolSet& set);
        static void Write(const char* fileName, CoherentSymbolSet& set); 
    };

    // bsd-gl2 format
    /// @brief might work with other bsds
    class CoherentSymbolFormatGL2 : public CoherentSymbolFormat
    {   
        //
        enum SymbolType
        {
            Undefined = 0x00,
            External = 0x01,
            Absolute = 0x02, 
            Test = 0x04, 
            Data = 0x06, 
            BSS = 0x08, 
            Indr = 0x0A,                /// @brief not sure what this happens
            AbsoluteSetElement = 0x14,
            Function = 0x15,            /// @brief the mask for a function
            TextSetElement = 0x16, 
            DataSetElement = 0x18, 
            BssSetElement = 0x1A, 
            SetVectorPtr = 0x1C,

        };

        struct SymbolEntry
        {
            uint32_t namePtr = 0;
            uint8_t flags = 0; 
            uint8_t unused = 0;
            uint16_t debugStab = 0;
            uint32_t symbolAddress = 0;
        };

    public: 
        static void Read(const char* fileName, CoherentSymbolSet& set);
        static void Write(const char* fileName, CoherentSymbolSet& set);  
    };
};