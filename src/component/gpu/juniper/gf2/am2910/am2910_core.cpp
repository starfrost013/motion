/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    am2910.cpp: The AMD Am2910 Microcode Sequencer Implementation
    This one uses the am2903 to perform actions 
     
    Source: https://www.datasheets360.com/pdf/-6069213202016663880
*/

#include <component/gpu/juniper/gf2/am2910/am2910.hpp>

namespace Motion
{
    /// @brief Start exeuction of the am2910. Default is at 0x0.
    /// @param pcReg 
    void AM2910::Start(uint16_t pcReg)
    {
        this->pcReg = pcReg;
        running = true; 

        // WHAT IS PCCTR ?? WHY ???
    }

    void AM2910::Tick()
    {
        // next the next state
        if (running)
        {
            // slices are 16 bit, states are 64 bit.
            // ucode is uint16_t[4096][4] or 32768 bytes. Even though a bit slice is 4 kb. 
            // Perhaps an individual micro-operation for each "unit" is 4 bytes

            // Convert: (addr<<3)
            uint32_t currentSlice = (pcReg & 0x07) >> 1;            // is this even accurate
            uint16_t next = (ucode->data[pcReg][0]);
            uint16_t word1 = (ucode->data[pcReg][1]);
            uint16_t word2 = (ucode->data[pcReg][2]);
            uint16_t word3 = (ucode->data[pcReg][3]);

            // ok
            // microword is 55 bits i'm cr0orjhnr=jhnqgb”»•·

            // 2903 inputs
            bool i0 = (word1) & 0x01;                               // control input
            uint8_t i1 = (word1 >> 1) & 0x0F;                       // function select
            uint8_t i2 = (word1 >> 5) & 0x0F;                       // F/Y/U register write select
            
            bool carryIn = (word1 >> 9) & 0x01; 
            bool earbar = (word1 >> 10) & 0x01;                     // R input register address
            bool ealbar = (word1 >> 11) & 0x01;                     // L(?) input register address
            
            uint8_t addrA = (word1 >> 12) & 0x0F;
            uint8_t addrB = word2 & 0x0F;
            uint8_t seqop = (word2 >> 12) & 0x0F;                   // the 2903 op
            
            bool ccsel = word3 & 0x07;
            // don't need ram information

            // perform CCSEL 
            switch (ccsel)
            {
                
            }


            pcReg = next; // this it he only control flow
        }
    }
    
    uint16_t AM2910::StackPush()
    {
        return 0xFF; 
    }

    void AM2910::StackPop()
    {

    }

    void AM2910::Perform2903Op()
    {

    }

    /// @brief Basically goes HEY 2903, EXECUTE THIS MICROCODE NOW!
    /// @param nextUcode the microcode instruction to execute
    void AM2910::YellAt2903(uint16_t nextUcode)
    {
        the2903->ExecuteUcode(nextUcode);
    }
}