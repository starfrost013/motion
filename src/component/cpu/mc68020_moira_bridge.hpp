#pragma once 

#include <exception>

#include <Motion.hpp>
#include <coherent/coherent.hpp>
#include <component/component.hpp>
#include <component/cpu/cpu.hpp>
#include <component/cpu/moira/Moira.h>
#include <component/ip2/ip2_interrupt.hpp>
#include <base/emulation.hpp>

namespace Motion
{
    #define LOG_PREFIX_BRIDGE        "68020 CPU Lisburn-to-Motion Bridge"

    class MC68020MoiraBridge : public Motion::Lisburn::Moira 
    {
        friend class MC68020;

        uint8_t read8(uint32_t addr) const override 
        { 
            AddrSpace::ClearFault();
            uint8_t value = AddrSpace::ReadU8(addr); 
            FireBusErrorIfNeeded(false, true);
            return value;
        };

        uint16_t read16(uint32_t addr) const override 
        { 
            AddrSpace::ClearFault();
            uint16_t value = AddrSpace::ReadU16(addr); 
            FireBusErrorIfNeeded(false, false);
            return value;
        };

        // peek to not cause spurious bus errors
        uint16_t read16Dasm(uint32_t addr) const override
        {
            return AddrSpace::PeekU16(addr);
        };

        void write8(uint32_t addr, uint8_t value) const override 
        { 
            AddrSpace::ClearFault();
            AddrSpace::WriteU8(addr, value); 
            FireBusErrorIfNeeded(true, true);
        };  

        void write16(uint32_t addr, uint16_t value) const override 
        { 
            AddrSpace::ClearFault();
            AddrSpace::WriteU16(addr, value); 
            FireBusErrorIfNeeded(true, false);
        }; 

        uint16_t readIrqUserVector(uint8_t level) const override
        {
            if (!interrupts)
                interrupts = Emulation::GetMachine()->FindComponentByType<IP2Interrupt>();

            return interrupts->GetVector(level);
        }

        // what we want the 68020 to do after we fire the bus error
        static const uint16_t SSW_DATA_FAULT     = 0x0400;  // DF: rerun the data cycle
        static const uint16_t SSW_READ           = 0x0100;  // RW: set for a read
        static const uint16_t SSW_SIZE_BYTE      = 0x0040;
        static const uint16_t SSW_SIZE_WORD      = 0x0080;
        static const uint16_t SSW_FC_SUPERVISOR  = 0x0004; 

        // there is a fault, fire a bus error
        void FireBusErrorIfNeeded(bool isWrite, bool isByte) const
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
            frame.code = (uint16_t)((frame.ird & 0xFFE0) | (faultWasWrite ? 0x00 : 0x10));

            bool isSupervisor = (getSR() & 0x2000); // bit 13 of status register indicates supervsior mode

            // fcl is FC1|FC0, the program/data half of the function code; FC2 is supervisor state.
            frame.ssw = SSW_DATA_FAULT
                | (isWrite ? 0 : SSW_READ)
                | (isByte ? SSW_SIZE_BYTE : SSW_SIZE_WORD)
                | (isSupervisor ? SSW_FC_SUPERVISOR : 0)
                | fcl;

            /*  may be needed later ? 
            if (traceExceptions)
            {
                Logger::Log(LOG_PREFIX_68020_BRIDGE, std::format("bus error: {} {} of 0x{:x} by instruction at pc 0x{:x} ({})",
                    isByte ? "byte" : "word", isWrite ? "write" : "read", faultAddress, getPC0(),
                    IsSupervisor() ? "supervisor" : "user").c_str(), LogChannels::Warning);
            }
            */

            // TODO: make LISBURN not use EXCEPTIONS !!! EXCEPTIONS SUCK !!!


            // handle a fatal error
            if (std::current_exception())
            {
                Logger::Log("The emulator is going down due to a recursive bus error. This should never happen", LogChannels::FatalError);
                return;
            }

            throw Motion::Lisburn::BusError(frame);
        };

        void didExecuteException(Motion::Lisburn::M68kException exc, uint16_t vector) override { Coherent::Exception(vector); } ;

    private:
        mutable IP2Interrupt* interrupts;
    };
}