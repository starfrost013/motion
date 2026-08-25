/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    addrspace.hpp : Implements the address space mapping system and also interfaces with an optional MMU
*/

#pragma once

#include <Motion.hpp>
#include <component/component.hpp>
#include <component/mmu/mmu.hpp>

#define LOG_PREFIX_MAPPING  "Emulation - Memory Mapping"

// Everything at or above this is devices rather than RAM, so a hole in it is a bus timeout.
#define ADDRSPACE_DEVICE_SPACE_START 0x30000000

// The PROM sizes memory by writing patterns into RAM that is not fitted, and the kernel probes for
// boards that are not there. Neither is news after the first few, so do not let them bury the log.
#define ADDRSPACE_MAX_UNMAPPED_LOGGED 32

namespace Motion
{
    // This class implements an address space mapping.
    // The 
    class AddrSpaceMapping
    {
    public: 
        // The start address is used as the key in the map.
        size_t startAddr;
        size_t endAddr; 

        Component* component;
    };

    /// @brief Suppresses fault reporting for reads the emulated machine is not really making.
    class AddrSpacePeek
    {
    public:
        AddrSpacePeek();
        ~AddrSpacePeek();
    };

    // Class implementing address space.
    // It is tied to the Machine.
    // By default each machine has
    class AddrSpace
    {
        public:
            // 16-bit - 65536; 24-bit - 16777216; 
            inline static size_t maxAddr;

            static uint8_t ReadU8(size_t addr);
            static uint16_t ReadU16(size_t addr);
            static uint32_t ReadU32(size_t addr);
            static int8_t ReadS8(size_t addr);
            static int16_t ReadS16(size_t addr);
            static int32_t ReadS32(size_t addr);
 
            static void WriteU8(size_t addr, uint8_t value);
            static void WriteU16(size_t addr, uint16_t value);
            static void WriteU32(size_t addr, uint32_t value);
            static void WriteS8(size_t addr, int8_t value);
            static void WriteS16(size_t addr, int16_t value);
            static void WriteS32(size_t addr, int32_t value);
 
            static void AddMapping(AddrSpaceMapping mapping);
            static AddrSpaceMapping* GetMapping(size_t addr);

            /*
                An MMU can detect a fault, but it can't raise the exception: only the CPU core knows how to
                build the stack frame for one. So a failed translation is recorded here, and whoever made
                the access decides what to do with it. The CPU turns it into a bus error; the debugger and
                anything else reading memory behind the machine's back can just ignore it.
            */
            /// @brief Faults are ignored until the CPU is out of reset, the same way the MMU declines to
            /// translate during reset - the reset vector fetch happens before every device has mapped itself.
            static void SetFaultsEnabled(bool enabled) { faultsEnabled = enabled; };

            /*
                Bring-up instrumentation. An unmapped access is nearly always a pointer that was
                corrupted somewhere upstream, and the only way to find upstream is to see who was
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

            /*
                A read made on behalf of the debugger is not a bus cycle. Disassembling whatever the
                PC happens to point at, or drawing the stack window, must not record a fault - the
                CPU would then raise a bus error for an access the emulated machine never made, and
                because those reads happen outside the execute loop there is nothing to catch it.
            */
            static void PushPeek() { peekDepth++; };
            static void PopPeek() { if (peekDepth) peekDepth--; };

            /// @brief True while the current read is the debugger's rather than the machine's. A
            /// device register with a read side effect has to ask, or the debugger drives the device.
            static bool IsPeeking() { return peekDepth > 0; };

            static void SignalFault(size_t addr, bool isWrite);
            static void SignalFaultIfDeviceSpace(size_t addr, bool isWrite);
            static void ClearFault() { faultPending = false; };
            static bool TakeFault(size_t* addr, bool* isWrite);

            /// @brief Reigister a memory management unit
            /// @param mmu The MMU to register.
            static void RegisterMMU(ComponentMMU* mmu);

            static void Shutdown();
        private: 
            inline static std::unordered_map<size_t, AddrSpaceMapping> mappings;
            

            ///pointer to an MMU component
            inline static ComponentMMU* mmu;

            /// @brief Whether a failed translation should be recorded at all. Set once at startup.
            inline static bool faultsEnabled = false;

            /*
                Thread local, and it matters. These are a handshake between one memory access and the
                code right after it that turns a failed translation into an exception, so they belong
                to whoever is making the access - and the emulation thread is not the only one making
                them. The debugger disassembles around the PC from the render thread every frame,
                inside an AddrSpacePeek, and with a shared peekDepth that window suppressed faults on
                the *emulation* thread: SignalFault returned early, the CPU read 0xFF instead of
                taking a bus error, and carried on into whatever that decoded as. It showed up as a
                boot that died with an illegal instruction roughly one run in four, because it
                depended on a debugger frame happening to overlap a page fault.
            */
            inline static thread_local bool faultPending = false;
            inline static thread_local size_t faultAddress = 0;
            inline static thread_local bool faultWasWrite = false;
            inline static thread_local int32_t peekDepth = 0;

            /// @brief Rate limit for the unmapped access warning. Approximate across threads, which is fine.
            inline static int32_t unmappedLogged = 0;

    };
}
