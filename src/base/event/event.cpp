/*
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    event.cpp: The implementation of a little backend independent arbitrary event system
*/

#include <base/program.hpp>
#include <base/event/event.hpp>
#include <base/emulation.hpp>

namespace Motion
{
    void EventSystem::FireEvent(Event& evt)
    {
        if (Program::GetState() != ProgramState::Emulation)
            return;
            
        Emulation::OnEvent(evt);
    }

    // debugging aide
    const char* eventTypeToString[] = 
    {
        "Key Down",
        "Key Up",
        "Mouse Down",
        "Mouse Up",
        "Serial Receive Byte",
        "Serial Transmit Byte",
        "Focus Lost",
        "Mouse Motion",
    };
}