/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    am2910.cpp: The AMD Am2910 Microcode Sequencer Implementation
    This one controls the AM2903...To perform actions
    
     
    Source: https://www.datasheets360.com/pdf/-6069213202016663880
*/

#include <component/gpu/juniper/gf2/am2910/am2910.hpp>

namespace Motion
{
    void AM2910::Start()
    {

    }

    uint16_t AM2910::StackPush()
    {

    }

    void AM2910::StackPop()
    {

    }

    /// @brief Basically goes HEY 2903, EXECUTE THIS MICROCODE NOW!
    /// @param nextUcode the microcode instruction to execute
    void AM2910::YellAt2903(uint16_t nextUcode)
    {
        the2903->ExecuteUcodeAt(nextUcode);
    }
}