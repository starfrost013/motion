//
// The MIPS-I core
// For validating the emulator design and looking forward to potential emulation of the 4D series (professional & personal iris)
//

#pragma once
#include <Motion.hpp>
#include <base/emulation.hpp>
#include <component/cpu/cpu.hpp>

namespace Motion
{
    // Flavours of MIPS-I CPUs

    enum MIPSFlavour
    {
        R2000 = 0,
        R3000 = 1,
    }; 

    #define MIPS_NUM_GENERAL_REGS   32
    #define MIPS_TLB_SIZE           64

    /// @brief Mips-I CP0 (MMU)
    class MIPSCP0
    {
        friend class MIPS; 

    private: 
        uint32_t entryHi;           // selected tlb index bits 63:32
        uint32_t entryLo;           // selected tlb index bits 31:0

        uint32_t index;             // programmed index into tlb
        uint32_t random;            // random index into tlb

        uint32_t procRevId;

        uint32_t status;
        uint32_t context; 
        uint32_t cause;
        uint32_t badVa; 
        uint32_t epc; 

        // 56 random replacement entries, 8 entries
        uint64_t tlb[MIPS_TLB_SIZE]; // tlb
    };

    class MIPS : public ComponentCPU
    {
    private:
        uint32_t r[MIPS_NUM_GENERAL_REGS]; // the general registers. r0 is always 0

        uint32_t pc; 

        // multiply/divide registers
        uint32_t mdhi;
        uint32_t mdlo; 

        MIPSFlavour flavour;

        MIPSCP0 cp0; 

        void DecodeI(uint32_t opcode);          // DECODE I-Type instruction
        void DecodeJ(uint32_t opcode);          // DECODE J-Type instruction
        void DecodeR(uint32_t opcode);          // DECODE R-Type instruction

    }; 
}