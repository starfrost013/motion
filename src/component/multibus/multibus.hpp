/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    multibus.hpp: The Intel Multibus bus.
    This is the bus that is used for everything on the early IRISes, and most things on the later IRIS 3000.

    Most likely it was inheited from the SUN design that SGI bought back in '82. PM1/PM2/IP1 boards are fully multibus, IP2 boards,
    except for the CPU, GPU and FPU (?) (thees use their own private bus.)

    TODO:
        Cards that use memory space, not just I/O space.
        Parity Control
        MB Protection
*/

#pragma once
#include <Motion.hpp>
#include <coherent/coherent.hpp>
#include <component/addrspace.hpp>
#include <component/component.hpp>

// also depends on the CPU
#include <component/cpu/cpu.hpp>
#include <component/ip2/ip2_interrupt.hpp>
#include <component/memory.hpp>

namespace Motion
{
    #define MULTIBUS_MEMORY_START           0x40000000
    #define MULTIBUS_MEMORY_END             0x40FFFFFF
    
    #define MULTIBUS_IO_START               0x50000000
    #define MULTIBUS_IO_END                 0x5000FFFF

    // The IP2 maps its RAM onto the backplane; upstream spells the same region absolutely as MULTIBUS_PAGING_START/END, so keep both in step.
    #define MULTIBUS_SLAVE_WINDOW_END       0x0FFFFF
    #define MULTIBUS_SLAVE_MAP_START        0x100000
    #define MULTIBUS_SLAVE_MAP_END          0x1FFFFF
    #define MULTIBUS_SLAVE_MAP_ENTRIES      256
    #define MULTIBUS_SLAVE_PAGE_SHIFT       12
    #define MULTIBUS_SLAVE_PAGE_MASK        0xFFF
    #define MULTIBUS_SLAVE_FRAME_MASK       0x3FFF

    // The 5217 drives 24 address lines.
    #define MULTIBUS_ADDRESS_MASK           0xFFFFFF

    #define MULTIBUS_LOG_PREFIX             "Multibus"

    // The kernel probes for every board SGI ever shipped, and a missing one is not news after the first time.
    #define MULTIBUS_MAX_UNMAPPED_LOGGED    200

    #define MULTIBUS_NUM_IRQ                0x8
    // we use a raw array because its the fastest and a lot of this stuff is EXTREMELY Hot path! Like UC4/DC4. 
    #define MULTIBUS_MAX_SLOTS              20  

    class CoherentExtensionMultibus : public CoherentExtension
    {
    public: 
        CoherentExtensionMultibus(Component* owner) : CoherentExtension(owner) {}

        void AddUI() override; 
    }; 

    class Multibus : public Component
    {
        // friend so we can access random etc
        friend class CoherentExtensionMultibus;

    public: 
        void Start() override; 
        void Shutdown() override;

        const char* GetName() override { return "Intel Multibus"; };

        /* 
            Defines a multibus backplane slot mapping
            Aparently multibus kind of sucks and basically it seems like it just provides a mechanism of firing the IRQs and protection.
        */
        class SlotMapping
        {  
            friend class Multibus; 
            friend class CoherentExtensionMultibus; 

        public: 
            uint32_t irq; 
            size_t memStart = 0;
            size_t memEnd = 0;
            size_t ioStart = 0;
            size_t ioEnd = 0; 

            // debug slot number
            int32_t id;
            
            // Yes we are hardcoding the addresses.
            // HOW do these devices find out whta part of the address space they decode ???? WHAT ??? NO BARs ????
            
            Component* component;

            SlotMapping(Component* component) : SlotMapping()
            {
                this->component = component;
            }

        private: 
            bool active = false;

            // parameterless constructor should be private
            SlotMapping()
            {
            }
        }; 

        // other stuff may be dependent on the multibus, so start it first
        bool IsEarlyStart() override { return true; };

        // our slots
        // each slot mapping has its won vector
        std::vector<SlotMapping> slotMappings;
        
        // this simulates the action of the user inserting a slot into the Multibus backplane.
        // NOTE: no attempt is made to prevent the addresses overlapping. the first slot in the range will be used.
        bool AddSlotMapping(SlotMapping& slot);

        uint8_t Read8(size_t addr) override;
        uint16_t Read16(size_t addr) override;
        uint32_t Read32(size_t addr) override;
        void Write8(size_t addr, uint8_t value) override;
        void Write16(size_t addr, uint16_t value) override;
        void Write32(size_t addr, uint32_t value) override;

        // For bus MASTERS other than the CPU. Raw backplane addresses, resolved exactly as a CPU access through segment 4.
        uint8_t ReadMB8(size_t addr) { return Read8(MULTIBUS_MEMORY_START + (addr & MULTIBUS_ADDRESS_MASK)); }; 
        uint16_t ReadMB16(size_t addr) { return Read16(MULTIBUS_MEMORY_START + (addr & MULTIBUS_ADDRESS_MASK)); }; 
        uint32_t ReadMB32(size_t addr) { return Read32(MULTIBUS_MEMORY_START + (addr & MULTIBUS_ADDRESS_MASK)); }; 
        void WriteMB8(size_t addr, uint8_t value) { Write8(MULTIBUS_MEMORY_START + (addr & MULTIBUS_ADDRESS_MASK), value); }
        void WriteMB16(size_t addr, uint16_t value) { Write16(MULTIBUS_MEMORY_START + (addr & MULTIBUS_ADDRESS_MASK), value); }
        void WriteMB32(size_t addr, uint32_t value) { Write32(MULTIBUS_MEMORY_START + (addr & MULTIBUS_ADDRESS_MASK), value); }

        /// @brief Copy a block out of Multibus memory, uncrossing the byte lanes: dst[i] is the Multibus byte at addr + i.
        void ReadMBBlock(size_t addr, uint8_t* dst, size_t length);
        /// @brief The reverse: the Multibus byte at addr + i becomes src[i].
        void WriteMBBlock(size_t addr, const uint8_t* src, size_t length);

        /// @brief Drive or release one of the eight shared Multibus interrupt lines.
        void SetMultibusIRQ(int32_t number, bool asserted);


        /// @brief What, if anything, on the IP2 itself answers for a Multibus address no card claimed.
        enum class SlaveTarget
        {
            None,       // nothing drove DSACK, the cycle times out and BERR is asserted
            Ram,        // system RAM, through the slave map
            Map,        // the slave map SRAM itself
        };

        /// @brief Resolve a segment 4 address no card claimed; `target` comes back a RAM address or a map entry index.
        SlaveTarget DecodeSlave(size_t addr, size_t* target);

        /// @brief Report an access nothing answered for, rate limited.
        void LogUnmapped(const char* what, size_t addr, bool isWrite, uint32_t value);

        int32_t unmappedLogged = 0;

        /// @brief The slave map. Public so the debugger can show it.
        uint16_t slaveMap[MULTIBUS_SLAVE_MAP_ENTRIES] = {0};

    private:
        // This is an optimisation, because of the way our bus modelling works we can't actually reliably determine what slot is being written to or read from
        // Since multibus stuff needs to share irq we filter everything through the multibus class. 
        // Let's store the last read and written slot and cache it so we don't need to iterate it. POinter because it needs to be a nullptr.
        SlotMapping* lastSlotRead = nullptr;
        SlotMapping* lastSlotWritten = nullptr;
        
        bool UseCachedReadSlot(size_t addr);
        bool UseCachedWriteSlot(size_t addr);

        // these are never meant to fail. MB writes should not occur unmapped.
        bool SetCachedReadMapping(size_t addr);
        bool SetCachedWriteMapping(size_t addr);

        // THE CPU, so we can fire an irq
        ComponentCPU* cpu; 
        IP2Interrupt* interrupts = nullptr;
        Memory* memory;
        // our coherent extension
        CoherentExtensionMultibus* multibusExtension;
    
    };
};