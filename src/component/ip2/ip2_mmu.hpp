/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    ip2_mmu.hpp: Defines the MMU sitting on the IP2 board
*/

#pragma once 
#include <Motion.hpp>
#include <base/filesystem/filesystem.hpp>
#include <component/addrspace.hpp>
#include <coherent/coherent.hpp>
#include <component/mmu/mmu.hpp>
#include <component/cpu/cpu.hpp>

namespace Motion
{
    extern Cvar* logIP2MMU;

    #define MMU_LOG_CHANNEL_NAME            "IP2 MMU"
    #define LOG_PREFIX_IP2MMU               "Emulation - IP2 MMU"
    //
    // Registers
    //

    #define MMU_START                       0x36000000
    #define MMU_END                         0x3F000000

    #define PAGETABLE_MAX_PAGES             (1 << 14) // pagenumber is 13 bits but there might be 2 ptes (one supervisor and one user?)
    #define PAGETABLE_INDEX(x)              0x3B000000 + (x*sizeof(uint32_t))

    #define REG_OS_BASE                     0x36000000
    #define REG_STATUS                      0x38000000
    #define REG_PARITY                      0x39000000
    #define REG_MULTIBUS_PROTECT            0x3A000000
    #define REG_PAGETABLE_BASE              0x3B000000
    #define REG_TEXTDATA_BASE               0x3C000000
    #define REG_TEXTDATA_LIMIT              0x3D000000
    #define REG_STACK_BASE                  0x3E000000
    #define REG_STACK_LIMIT                 0x3F000000

    // IP2 MMU segments
    #define MMU_SEGMENT_TEXTDATA            0x00000000      // System RAM, Text/Data
    #define MMU_SEGMENT_STACK               0x10000000      // System RAM, Stack
    #define MMU_SEGMENT_KERNEL              0x20000000      // System RAM, Kernel
    #define MMU_SEGMENT_SYSTEM              0x30000000      // Misc shit on IP2 Board & PROM
    #define MMU_SEGMENT_MULTIBUS_MEMORY     0x40000000      // multibus memory
    #define MMU_SEGMENT_MULTIBUS_IO         0x50000000      // multibus io
    #define MMU_SEGMENT_GEOMETRY_ENGINE     0x60000000      // GE
    #define MMU_SEGMENT_FPA                 0xF0000000      // FPA

    // page masks

    #define MMU_MASK_IS_PROTECTED           0x30000000      // page is protected
    #define MMU_MASK_NO_ACCESS              0x00000000      // no access
    #define MMU_MASK_READ_ONLY              0x10000000      // read only
    #define MMU_MASK_SUPERVISOR_ONLY        0x20000000      // supervisor only
    #define MMU_MASK_READ_WRITE             0x30000000      // read write
    #define MMU_MASK_REFERENCED             0x40000000      // referenced
    #define MMU_MASK_MODIFIED               0x80000000      // modified
    #define MMU_MASK_ALWAYS_SET             0xF0001FFF      // Bits which mame always sets. these seem to be wrong compared with the implementation 

    #define MMU_SEGMENT_GET_ID(x)           ((x >> 28) & 0x0F)

    /// The coherent extnension
    class CoherentExtensionIP2MMU : public CoherentExtension
    {
    public:
        CoherentExtensionIP2MMU(Component* owner) : CoherentExtension(owner) {}
        void AddUI() override;
        void DrawPagetableUI();

    private:
        bool pagetableUiEnabled;
    };

    // not sure hwy sgi decided that addresses must be so sparse that bit fucking 24 needed to be the register selector.
    // FOR COMPONENTS, WE DON'T NEED TO BOUNDS CHECK BECAUSE WE ALREADY MAPPED IT!

    class IP2MMU : public ComponentMMU
    {
        friend class CoherentExtensionIP2MMU;
    public: 
        void Start() override
        {
            ComponentMMU::Start();

            // map the private ram
            AddrSpaceMapping mapping = AddrSpaceMapping();

            logIP2MMU = Cvar::Get("logIP2MMU", "0");

            mapping.startAddr = MMU_START;
            mapping.endAddr = MMU_END;
            mapping.component = this;
            AddrSpace::AddMapping(mapping);

            mmuExtension = new CoherentExtensionIP2MMU(this);
            Coherent::RegisterExtension(mmuExtension);

            mmuChannel = LogChannel(MMU_LOG_CHANNEL_NAME, ConsoleColor::BrightCyan, ConsoleColor::White);
            Logger::AddChannel(mmuChannel);
            logEnabled = logIP2MMU->GetValue();

            if (logEnabled)
                Logger::SetChannelEnabled(MMU_LOG_CHANNEL_NAME);
        }

        void Shutdown() override
        {
            delete mmuExtension;
            ComponentMMU::Shutdown();
        }

        const char* GetName() override { return "IRIS 3130 TTL MMU"; };

        // Register I/O
        uint8_t Read8(size_t addr) override;
        uint16_t Read16(size_t addr) override;
        uint32_t Read32(size_t addr) override;
        void Write8(size_t addr, uint8_t value) override;
        void Write16(size_t addr, uint16_t value) override;
        void Write32(size_t addr, uint32_t value) override; 

        bool Translate(size_t addr, size_t* finalAddress, bool isWrite) override;

        // well probably need to change these so just make them public

        // This one is an 8bit register but it is easier for the MMU if we store it like this.
        uint16_t osBase = 0x0;
        uint16_t status = 0x0;
        uint16_t parity = 0x0;
        uint16_t multibusProtect = 0x0;
        uint32_t pagetable[PAGETABLE_MAX_PAGES] = {0};
        uint16_t textdataBase = 0x0;
        uint16_t textdataLimit = 0x0;
        uint16_t stackBase = 0x0;
        uint16_t stackLimit = 0x0;

        bool logEnabled = false; 

    private: 
        CoherentExtensionIP2MMU* mmuExtension; 
        ComponentCPU* cpu = nullptr;
        LogChannel mmuChannel;

    };


}