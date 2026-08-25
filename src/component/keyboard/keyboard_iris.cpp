/* motion - The SGI Emulator. Copyright (c)2026 starfrost. keyboard_iris.cpp:  */

#include <component/keyboard/keyboard_iris.hpp>

namespace Motion
{
    Cvar* logKeyboard;
    // find(), not operator[]: that would insert a zero and return it, and button 0 is the BREAK KEY.
    bool KeyboardIris::LookupScancode(uint32_t key, uint8_t& scancode)
    {
        auto entry = sdlToSgi.find((SDL_Keycode)key);

        if (entry == sdlToSgi.end())
            return false;

        scancode = entry->second;

        return scancode < KEYBOARD_SCANCODE_COUNT;
    }

    // One half of a keystroke. Bit 7 clear is the key going down, bit 7 set is it coming up.
    void KeyboardIris::SendKey(uint8_t scancode, bool down)
    {
        scancode &= KEYBOARD_SCANCODE_KEY_MASK;

        keyDown[scancode] = down;

        uint8_t wire = down ? scancode : (uint8_t)(scancode | KEYBOARD_SCANCODE_RELEASE);

        if (logKeyboard && logKeyboard->GetValue())
            Logger::Log(KEYBOARD_LOG_PREFIX, std::format("key {} {} (scancode 0x{:02x})",
                scancode, down ? "down" : "up", wire).c_str(), LogChannels::Warning);

        duart->GetLine(KEYBOARD_DUART_LINE).AddRxByte(wire);
    }

    // Let go of everything.
    void KeyboardIris::ReleaseEverything()
    {
        for (int32_t scancode = 0; scancode < KEYBOARD_SCANCODE_COUNT; scancode++)
        {
            if (keyDown[scancode])
                SendKey((uint8_t)scancode, false);
        }
    }

    void KeyboardIris::OnEvent(Event& evt)
    {
        if (!logKeyboard)
            logKeyboard = Cvar::Get("logKeyboard", "0");

        if (!duart)
        {
            duart = Emulation::GetMachine()->FindComponentByType<DUART68681>();

            // if still no duart return
            if (!duart)
                return;
        }

        if (evt.type == EventType::SerialTransmit)
        {
            SerialTransmitEvent transmitEvent = *static_cast<SerialTransmitEvent*>(&evt);

            if (transmitEvent.lineId != 0)
                return;

            uint8_t previous = lastHostByte;

            lastHostByte = transmitEvent.data;

            // Answer "who are you" every time it is asked, not just the first time.
            if (previous == KEYBOARD_WHO_ARE_YOU_0
            && transmitEvent.data == KEYBOARD_WHO_ARE_YOU_1)
            {
                // put the kbd type on the bus
                duart->GetLine(KEYBOARD_DUART_LINE).AddRxByte(KEYBOARD_TYPE_IRIS);
                initialised = true;

                return;
            }

            // lol
            switch (transmitEvent.data)
            {
                default:
                    if (!shutUpLed)
                    {
                        if (transmitEvent.data & 1)
                        {
                            Logger::Log("Just imagine the shiny keyboard LEDs flashing here for now");
                            shutUpLed = true;
                        }
                    }

                    if (!shutUpBeep)
                    {
                        if (!(transmitEvent.data & 1)) // bit 0 beeping
                        {
                            Logger::Log("Just imagine the keyboard beeping here for now");
                            shutUpBeep = true;
                        }
                    }
                    break;
            }
        }
        else if (evt.type == EventType::FocusLost)
        {
            if (initialised)
                ReleaseEverything();
        }
        else if (evt.type == EventType::KeyDown)
        {
            if (!initialised)
                return;

            KeyDownEvent& keyDownEvent = static_cast<KeyDownEvent&>(evt);
            uint8_t scancode = 0;

            if (!LookupScancode(keyDownEvent.key, scancode))
                return;

            // The host's auto-repeat is the host's.
            if (keyDownEvent.repeat)
                return;

            // A key going down that the guest already believes is down means its release went somewhere else.
            if (keyDown[scancode & KEYBOARD_SCANCODE_KEY_MASK])
                SendKey(scancode, false);

            // Nothing else to do.
            SendKey(scancode, true);
        }
        else if (evt.type == EventType::KeyUp)
        {
            if (!initialised)
                return;

            KeyUpEvent& keyUpEvent = static_cast<KeyUpEvent&>(evt);
            uint8_t scancode = 0;

            if (!LookupScancode(keyUpEvent.key, scancode))
                return;

            SendKey(scancode, false);
        }
    }

    void KeyboardIris::Shutdown()
    {
        duart = nullptr;
    }
}
