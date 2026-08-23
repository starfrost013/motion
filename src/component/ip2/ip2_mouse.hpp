/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    ip2_mouse.hpp: IP2 mouse button and quadrature registers.

    Two read-only registers hanging off the IP2 rather than a real device. The PROM reads the button
    register during boot phase 0 to work out which revision of the board it is running on, long before
    it cares about the mouse, so these have to answer even with nothing plugged in.
*/

#pragma once 
#include <Motion.hpp>
#include <base/event/event.hpp>
#include <component/addrspace.hpp>
#include <component/component.hpp>

namespace Motion
{
    #define MOUSE_BUTTONS_ADDR          0x30800000      // 8-bit
    #define MOUSE_QUADRATURE_ADDR       0x31000000      // 16-bit

    // Buttons are active high. Bit 3 is unused.
    #define MOUSE_BUTTON_RIGHT          (1 << 0)
    #define MOUSE_BUTTON_MIDDLE         (1 << 1)
    #define MOUSE_BUTTON_LEFT           (1 << 2)

    // Not a button: the PROM reads this bit to tell a Rev B board from a Rev A one. Held clear, because
    // a Rev A board is what the rest of this emulator models - Rev B makes the PROM program DUART1 too.
    #define MOUSE_BOARD_REVISION_B      (1 << 4)

    // Quadrature register. The PROM/GL2 poll this to accumulate movement rather than reading a position.
    #define MOUSE_QUADRATURE_X_MOVED    (1 << 0)
    #define MOUSE_QUADRATURE_X_POSITIVE (1 << 1)
    #define MOUSE_QUADRATURE_Y_MOVED    (1 << 2)
    #define MOUSE_QUADRATURE_Y_POSITIVE (1 << 3)

    #define LOG_PREFIX_IP2MOUSE         "Emulation - IP2 Mouse"

    class IP2Mouse : public Component
    {
    public:
        void Start() override
        {
            AddrSpaceMapping buttons = AddrSpaceMapping();

            buttons.startAddr = MOUSE_BUTTONS_ADDR;
            buttons.endAddr = MOUSE_BUTTONS_ADDR + 1;
            buttons.component = this;
            AddrSpace::AddMapping(buttons);

            AddrSpaceMapping quadrature = AddrSpaceMapping();

            quadrature.startAddr = MOUSE_QUADRATURE_ADDR;
            quadrature.endAddr = MOUSE_QUADRATURE_ADDR + 1;
            quadrature.component = this;
            AddrSpace::AddMapping(quadrature);
        };

        const char* GetName() override { return "IP2 Mouse"; };

        uint8_t Read8(size_t addr) override
        {
            if (addr >= MOUSE_QUADRATURE_ADDR)
                return (uint8_t)ReadQuadrature();

            return buttonState;
        };

        uint16_t Read16(size_t addr) override
        {
            if (addr >= MOUSE_QUADRATURE_ADDR)
                return ReadQuadrature();

            return buttonState;
        };

        // Both registers are read only - the IP2 has nothing to write here.
        void Write8(size_t addr, uint8_t value) override { };
        void Write16(size_t addr, uint16_t value) override { };

        void OnEvent(Event& evt) override
        {
            if (evt.type == EventType::MouseDown)
                buttonState |= ButtonMaskFor(((MouseDownEvent&)evt).mouse);
            else if (evt.type == EventType::MouseUp)
                buttonState &= ~ButtonMaskFor(((MouseUpEvent&)evt).mouse);
        };

    private:
        uint8_t buttonState = 0;

        /// @brief Movement is reported as "something moved since you last looked", so reading clears it.
        uint16_t ReadQuadrature()
        {
            uint16_t value = quadratureState;
            quadratureState = 0;
            return value;
        };

        static uint8_t ButtonMaskFor(uint32_t sdlButton)
        {
            // SDL numbers them left/middle/right from 1, the IP2 packs them the other way round
            switch (sdlButton)
            {
                case 1: return MOUSE_BUTTON_LEFT;
                case 2: return MOUSE_BUTTON_MIDDLE;
                case 3: return MOUSE_BUTTON_RIGHT;
                default: return 0;
            }
        };

        // TODO: drive this from relative mouse motion once the graphics side is usable
        uint16_t quadratureState = 0;
    };
};
