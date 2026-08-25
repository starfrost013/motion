/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    Memory.hpp: Defines the actual system RAM on the IRIS 3130.

    On the IRIS 3000 (Juniper) machines, memory is implemented as up to 4 IM1 (Inhouse Memory 1) boards, with 2 or 4 MBytes each for a max of 16mb ram.
    The MMU has 13-bit page numbers and 4kb pages for a theoretical maximum of 32 MB system RAM.
*/

#pragma once
#include <Motion.hpp>
#include <component/component.hpp>
#include <base/emulation.hpp>
#include <coherent/coherent_editor.hpp>

namespace Motion
{    
    // An access past the end of fitted RAM is normal during the PROM's memory sizing, so do not let
    // a storm of them bury everything else in the log.
    #define MEMORY_MAX_OUT_OF_RANGE_LOGGED  16

    class Memory : public Component
    {
        uint8_t* ram;
        
        void Start() override;
        void Shutdown() override;

    public: 
        uint8_t Read8(size_t addr) override; 
        uint16_t Read16(size_t addr) override; 
        uint32_t Read32(size_t addr) override;
        void Write8(size_t addr, uint8_t value) override; 
        void Write16(size_t addr, uint16_t value) override;
        void Write32(size_t addr, uint32_t value) override;

        /// @brief get the name of this component. immutable const char*.
        const char* GetName() override { return "System RAM"; };

        // other stuff may be dependent on the memory, so start it first
        bool IsEarlyStart() override { return true; };

    private: 
        size_t GetRamCapacity() { return Emulation::GetMachine()->totalRamInstalled; };

        /// @brief True if addr is backed by fitted RAM. Logs, rate limited, when it is not.
        /// @brief Does a `width` byte operand at `addr` fit inside the RAM fitted?
        bool IsInRange(size_t addr, size_t width, const char* what);

        int32_t outOfRangeLogged = 0;

        CoherentEditor* memoryEditor;
    };
}