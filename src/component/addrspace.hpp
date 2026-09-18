/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    addrspace.hpp : Implements the address space mapping system, bus erroring if not present. and also interfaces with an optional MMU
*/

#pragma once

#include <Motion.hpp>
#include <component/component.hpp>
#include <component/mmu/mmu.hpp>

#define LOG_PREFIX_MAPPING  "Emulation - Memory Mapping"

// Everything at or above this is devices rather than RAM, so a hole in it is a bus timeout.
#define ADDRSPACE_DEVICE_SPACE_START 0x30000000

// PROM memory sizing and device probes access unmapped memory
#define ADDRSPACE_MAX_UNMAPPED_LOGGED 32

namespace Motion
{
    // This class implements an address space mapping.
    class AddrSpaceMapping
    {
    public: 
        // The start address is used as the key in the map.
        size_t startAddr;
        size_t endAddr; 

        Component* component;
    };

    // Class implementing address space.
    // It is tied to the Machine.
    // By default each machine has an addres space
    class AddrSpace
    {
        public:
            // 16-bit - 65536; 24-bit - 16777216; 
            inline static size_t maxAddr;

            static uint8_t ReadU8(size_t addr);
            static uint16_t ReadU16(size_t addr);
            static uint32_t ReadU32(size_t addr);

            /* Peeks have no side effects and are used by the debugger */
            static uint8_t PeekU8(size_t addr);
            static uint16_t PeekU16(size_t addr);
            static uint32_t PeekU32(size_t addr);
 
            static void WriteU8(size_t addr, uint8_t value);
            static void WriteU16(size_t addr, uint16_t value);
            static void WriteU32(size_t addr, uint32_t value);

            static void AddMapping(AddrSpaceMapping mapping);
            static AddrSpaceMapping* GetMapping(size_t addr);
            static AddrSpaceMapping* PeekMapping(size_t addr);

            /*
                An MMU can detect a fault, but it can't raise the exception: only the CPU core knows how to
                build the stack frame for one. So a failed translation is recorded here, and whoever made
                the access decides what to do with it. The CPU turns it into a bus error; the debugger and
                anything else reading memory behind the machine's back can just ignore it.
            */
            /// @brief Faults are ignored until the CPU is out of reset - the reset vector fetch precedes every device mapping itself.
            static void SetFaultsEnabled(bool enabled) { faultsEnabled = enabled; };

            /*
                An unmapped access is nearly always a pointer that was corrupted somewhere earlier int he boot process,
                and the only way to find upstream is to see who was
                executing at the time. The CPU installs a hook here because AddrSpace cannot include
                the CPU headers without a cycle.
            */
            inline static void (*unmappedHook)(size_t addr, bool isWrite, int32_t width) = nullptr;
            /// @brief Report an access that landed in a hole, rate limited.
            static void LogUnmapped(const char* what, size_t addr, bool isWrite, uint32_t value);

            static void NotifyUnmapped(size_t addr, bool isWrite, int32_t width)
            {
                if (unmappedHook)
                    unmappedHook(addr, isWrite, width);
            }


            static void SignalFault(size_t addr, bool isWrite);
            static void SignalFaultIfDeviceSpace(size_t addr, bool isWrite);
            static void ClearFault() { faultPending = false; };
            static bool TakeFault(size_t* addr, bool* isWrite);

            /// @brief Reigister a memory management unit
            /// @param mmu The MMU to register.
            static void RegisterMMU(ComponentMMU* mmu);

            static void Shutdown();
        private: 

            static AddrSpaceMapping* ReadCommon(size_t addr, size_t* physAddr);     // Translates and maps an address for reading
            static AddrSpaceMapping* PeekCommon(size_t addr, size_t* physAddr);     // Translates and maps an address for peeking.
            static AddrSpaceMapping* WriteCommon(size_t addr, size_t* physAddr);    // Translates and maps an address for writing

            // fire a bus error
            static void BusError(size_t addr, bool isWrite, size_t bits);    

            static bool Translate(size_t addr, size_t* physAddr, bool isWrite); // Translates an address if an MMU exists, bus erroring if not possible
            
            inline static std::unordered_map<size_t, AddrSpaceMapping> mappings;

            ///pointer to an MMU component
            inline static ComponentMMU* mmu;

            // last cached mapping (we don't use lastread or lastwrite like in the multibus. probably we should just change it there too.)
            inline static AddrSpaceMapping* cachedMapping;

            /// @brief Whether a failed translation should be recorded at all. Set once at startup.
            inline static bool faultsEnabled = false;

            /* this is terrible. debugger does not need thread_local it should just be peeking */
            inline static thread_local bool faultPending = false;
            inline static thread_local size_t faultAddress = 0;
            inline static thread_local bool faultWasWrite = false;

            /// @brief Rate limit for the unmapped access warning. Approximate across threads, which is fine.
            inline static int32_t unmappedLogged = 0;

    };
}
