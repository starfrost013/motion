#pragma once 

#include <Motion.hpp>
#include <coherent/coherent.hpp>
#include <component/component.hpp>
#include <component/cpu/cpu.hpp>
#include <component/cpu/moira/Moira.h>
#include <component/ip2/ip2_interrupt.hpp>
#include <base/emulation.hpp>

namespace Motion
{
    #define LOG_PREFIX_68020_BRIDGE         "68020 CPU"

    // The vectors that are just the machine working normally.
    #define MC68020_VECTOR_SYSCALL          32
    #define MC68020_VECTOR_INTERRUPT_FIRST  0x40
    #define MC68020_VECTOR_INTERRUPT_LAST   0x57

    // Page 0 of a user address space is left unmapped on purpose as a null pointer guard.
    #define MC68020_USER_NULL_GUARD_END     0x1000

    class MC68020MoiraBridge : public Motion::Lisburn::Moira 
    {
        friend class MC68020;

        // The special status word describes the failed access to whoever handles the fault.
        static constexpr uint16_t SSW_DATA_FAULT     = 0x0400;  // DF: rerun the data cycle
        static constexpr uint16_t SSW_READ           = 0x0100;  // RW: set for a read
        static constexpr uint16_t SSW_SIZE_BYTE      = 0x0040;
        static constexpr uint16_t SSW_SIZE_WORD      = 0x0080;
        static constexpr uint16_t SSW_FC_SUPERVISOR  = 0x0004;  // FC2

        // The MMU records a failed translation rather than raising it, because building a bus error stack frame needs the core.
        void RaiseBusErrorIfFaulted(bool isWrite, bool isByte) const
        {
            size_t faultAddress = 0;
            bool faultWasWrite = false;

            if (!AddrSpace::TakeFault(&faultAddress, &faultWasWrite))
                return;

            Motion::Lisburn::StackFrame frame = {};

            frame.addr = (uint32_t)faultAddress;
            frame.pc = getPC();
            frame.sr = getSR();
            frame.ird = getIRD();
            frame.code = (uint16_t)((frame.ird & 0xFFE0) | (isWrite ? 0x00 : 0x10));

            // fcl is FC1|FC0, the program/data half of the function code; FC2 is supervisor state.
            frame.ssw = SSW_DATA_FAULT
                | (isWrite ? 0 : SSW_READ)
                | (isByte ? SSW_SIZE_BYTE : SSW_SIZE_WORD)
                | (IsSupervisor() ? SSW_FC_SUPERVISOR : 0)
                | fcl;

            if (traceExceptions)
            {
                Logger::Log(LOG_PREFIX_68020_BRIDGE, std::format("bus error: {} {} of 0x{:x} by instruction at pc 0x{:x} ({})",
                    isByte ? "byte" : "word", isWrite ? "write" : "read", faultAddress, getPC0(),
                    IsSupervisor() ? "supervisor" : "user").c_str(), LogChannels::Warning);
            }

            // Almost every user fault is demand paging working correctly.
            if (fatalUserFaultHook && !IsSupervisor() && faultAddress < MC68020_USER_NULL_GUARD_END)
                fatalUserFaultHook(faultAddress, isWrite);

            throw Motion::Lisburn::BusError(frame);
        }

        bool IsSupervisor() const { return (getSR() & 0x2000) != 0; }

        uint8_t read8(uint32_t addr) const override
        {
            AddrSpace::ClearFault();
            uint8_t value = AddrSpace::ReadU8(addr);
            RaiseBusErrorIfFaulted(false, true);
            return value;
        };

        uint16_t read16(uint32_t addr) const override
        {
            AddrSpace::ClearFault();
            uint16_t value = AddrSpace::ReadU16(addr);
            RaiseBusErrorIfFaulted(false, false);
            return value;
        };

        void write8(uint32_t addr, uint8_t value) const override
        {
            AddrSpace::ClearFault();
            AddrSpace::WriteU8(addr, value);
            RaiseBusErrorIfFaulted(true, true);
        };

        void write16(uint32_t addr, uint16_t value) const override
        {
            AddrSpace::ClearFault();
            AddrSpace::WriteU16(addr, value);
            RaiseBusErrorIfFaulted(true, false);
        }; 

        static bool WorthReporting(uint16_t vector)
        {
            // Interrupts and the syscall trap are the normal traffic and would drown everything else.
            return traceExceptions && vector != MC68020_VECTOR_SYSCALL
                && (vector < MC68020_VECTOR_INTERRUPT_FIRST || vector > MC68020_VECTOR_INTERRUPT_LAST);
        }

        // didExecuteException runs after the vector has been taken, so the PC it can report is the handler's, which says nothing about what went wrong.
        void willExecuteException(Motion::Lisburn::M68kException exc, uint16_t vector) override
        {
            if (WorthReporting(vector))
            {
                Logger::Log(LOG_PREFIX_68020_BRIDGE, std::format("exception vector {} raised by instruction at pc 0x{:x}, sr 0x{:04x} ({})",
                    vector, getPC0(), getSR(), IsSupervisor() ? "supervisor" : "user").c_str(), LogChannels::Warning);
            }
        };

        void didExecuteException(Motion::Lisburn::M68kException exc, uint16_t vector) override
        {
            // Gated behind logCpuTrace along with the rest of the bring-up instrumentation.
            if (WorthReporting(vector))
            {
                Logger::Log(LOG_PREFIX_68020_BRIDGE, std::format("exception vector {} taken at pc 0x{:x}, sr 0x{:04x} ({})",
                    vector, getPC(), getSR(), (getSR() & 0x2000) ? "supervisor" : "user").c_str(), LogChannels::Warning);
            }

            Coherent::Exception(vector);
        };

        // Both set from the logCpuTrace cvar by MC68020::Start.
        inline static bool traceExceptions = false;
        inline static void (*fatalUserFaultHook)(size_t addr, bool isWrite) = nullptr;

    public:
        // The IP2 does not autovector.
        void UseVectoredInterrupts() { irqMode = Motion::Lisburn::IrqMode::USER; };

        // Moira's default routes disassembly reads through read16, which for us is a real bus cycle that can raise a bus error.
        uint16_t read16Dasm(uint32_t addr) const override
        {
            AddrSpacePeek peek;
            return AddrSpace::ReadU16(addr);
        }

        uint16_t readIrqUserVector(uint8_t level) const override
        {
            if (!interrupts)
                interrupts = Emulation::GetMachine()->FindComponentByType<IP2Interrupt>();

            // With no interrupt logic to ask, fall back to what the CPU would do on its own.
            if (!interrupts)
                return (uint16_t)(24 + level);

            return interrupts->GetVector(level);
        }

    private:
        mutable IP2Interrupt* interrupts = nullptr;
    };
}