/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    am2910.hpp: The AMD Am2910 Microcode Sequencer
    This one controls the AM2903...To perform actions
    
     
    Source: https://www.datasheets360.com/pdf/-6069213202016663880
*/

#pragma once
#include <component/component.hpp>
#include <component/gpu/juniper/gf2/am2903/am2903.hpp>

namespace Motion
{
    #define AM2910_STACK_SIZE           5
    #define AM2910_LOG_PREFIX           "GF2 FBC Microinstruction Sequencer (AMD Am2910)"

    class AM2910
    {
    public: 
        AM2910(AM2903* new2903)
        {
            if (!new2903)
                Logger::Log(AM2910_LOG_PREFIX, "AM2910::AM2910(): Am2903 IS NULL!", LogChannels::FatalError);

            the2903 = new2903;
        }

        void Start();

        uint16_t StackPush();
        void StackPop();

        /// @brief Basically goes HEY 2903, EXECUTE THIS MICROCODE NOW!
        /// @param nextUcode the microcode instruction to execute
        void YellAt2903(uint16_t nextUcode);

    private: 
        bool stackFull;

        // WARNING: YOU MUST AND WITH 0xFFF! BITS 15-12 DO NOT EXIST! IF YOU SEE VALUE OF >=0X1000, IT'S INVALID!

        
        // LIFO 
        uint16_t pcPtr;
        uint16_t pcReg;        
        uint16_t stack[AM2910_STACK_SIZE];
        uint8_t stackPtr; 
        uint16_t d;                             // Direct input

        AM2903* the2903;                        // used to contrl us. this code sucks but it's r&d

    }; 
};