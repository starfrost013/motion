/*
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    keyboard.hpp: It's the keyboard.
    Generic keyboard interface

    On IRIS and early IRIS 4D, the keyboard I/O Interface is DUART0 Channel A. So we will hook to this. But this is the generic keyboard interface.#
    This class mostly exists so Machine::FindComponentByType can find all types of keyboards.

    Technically this is an Intel 8748 but HLE will be perfectly acceptable for now, we will emulate the MCU (variant of the 8048) later.

    The one thing the high level model cannot skip is that this keyboard reports both halves of a
    keystroke. IRIX keeps shift, control and caps lock from the make and break codes alone, and drops
    any key it cannot account for the modifier state of, so a keyboard that only ever says "down"
    leaves the guest believing a modifier is held forever and quietly stops working. See the note in
    keyboard_iris.hpp for the encoding.
*/

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

    /*
        Let go of everything. The host delivers a key-up to whoever has focus, so a key held while
        the user switches away never comes up here - and with shift or control that is the difference
        between a working keyboard and one that has silently stopped accepting input.
    */
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

            /*
                Answer "who are you" every time it is asked, not just the first time. Both the PROM
                and IRIX ask, and an unanswered request leaves the driver waiting, so it takes the
                next thing it hears - the user's first keystroke - as the reply instead.
            */
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

            /*
                The host's auto-repeat is the host's. A real keyboard here would repeat at whatever
                rate its own MCU was set to, not at the rate the user's desktop is configured for,
                and forwarding these is what makes a single deliberate keypress arrive as two or
                three characters whenever the emulator stalls long enough for a repeat to be
                queued behind it.
            */
            if (keyDownEvent.repeat)
                return;

            /*
                A key going down that the guest already believes is down means its release went
                somewhere else. Say it came up first, so the guest's idea of what is held matches
                the user's fingers again rather than drifting further out.
            */
            if (keyDown[scancode & KEYBOARD_SCANCODE_KEY_MASK])
                SendKey(scancode, false);

            /*
                Nothing else to do. Shift, control and caps lock are not folded in here: they are
                keys in their own right, and IRIX derives the modifier state from their own make and
                break codes. Setting bit 7 to mean "shifted" would mean "released" to the guest.
            */
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
