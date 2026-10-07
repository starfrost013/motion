/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    am2903.hpp: The AMD Am2903 cascadable bitslice microcoded processor
    4 are used for the FBC
    
    In order to reduce complexity we basically model this strictly as four units rather than messing around with slice nonsense.

    Also, an AMD Am2910 Microcode Processor is 
    This is technically a problem but I don't think any other SGI systems use the AM2903.    
    Source: https://www.datasheets360.com/pdf/-6069213202016663880
*/

#pragma once
#include <component/component.hpp>
#include <component/gpu/juniper/gf2/gf2_ucode.hpp>

namespace Motion
{
    #define AM2903_LOG_PREFIX           "GF2 FBC Microcode Processor (AMD Am2903)"
    #define AM2903_INTERNAL_RAM_SIZE    16          // AM2903 Intenral ram size

    // i1 field in ucode
    #define AM2903_ALU_OP_HIGH          0x0         // If I0 = HIGH, Special Functions
    #define AM2903_ALU_OP_SUB_SR        0x1         // F = S - R - 1 + Cn
    #define AM2903_ALU_OP_SUB_RS        0x2         // F = R - S - 1 + Cn
    #define AM2903_ALU_OP_ADD           0x3         // F = R + S + Cn
    #define AM2903_ALU_OP_ADD_S         0x4         // F = S + Cn
    #define AM2903_ALU_OP_ADD_NOTS      0x5         // F = !S + Cn
    #define AM2903_ALU_OP_ADD_R         0x6         // F = R + Cn
    #define AM2903_ALU_OP_ADD_NOTR      0x7         // F = !R + Cn
    #define AM2903_ALU_ZERO             0x8         // F = LOW
    #define AM2903_ALU_AND_NOTR         0x9         // F = !R and S
    #define AM2903_ALU_XNOR             0xA         // F = R XNOR S
    #define AM2903_ALU_XOR              0xB         // F = R XOR S
    #define AM2903_ALU_AND_R            0xC         // F = R AND S
    #define AM2903_ALU_NOR              0xD         // F = R NOR S
    #define AM2903_ALU_NAND             0xE         // F = R NAND S
    #define AM2903_ALU_OR               0xF         // F = R OR S

    class AM2903
    {
    public: 
        AM2903(GF2Ucode* ucode)
        {
            this->ucode = ucode;
        }

        void Start();
        void ExecuteUcode(uint16_t ucode);

        uint16_t AluOp();
        void Tick(); 

    private: 
        uint16_t q;

        // latched on two ports
        uint16_t ram[AM2903_INTERNAL_RAM_SIZE] = {0}; 

        uint16_t ramAddrA, ramAddrB;
        uint16_t io;

        GF2Ucode* ucode; 

        bool running = false; 

    }; 
}; 