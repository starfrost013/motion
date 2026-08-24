/*
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    ip2_mouse.hpp: IP2 mouse button and quadrature registers.

    Two read-only registers hanging off the IP2 rather than a real device. The PROM reads the button
    register during boot phase 0 to work out which revision of the board it is running on, long before
    it cares about the mouse, so these have to answer even with nothing plugged in.

    The mouse itself is an optical quadrature encoder and the IP2 does not count for it. Every time
    the mouse rolls far enough for a grating transition the board raises an interrupt, and the handler
    reads one register to find out which axis moved and in which direction, and adds or subtracts one
    from a counter in kernel memory. There is no position anywhere in the hardware - _mousex and
    _mousey are free running 16-bit counters that the GL2 driver differentiates, so the only thing
    that can be handed to the guest is "it moved one tick, this way".

    That is why the host pointer and the guest pointer drift apart: nothing here can say where the
    host pointer is, only how far it went. gl_valuators clamps the guest one to the screen, so they
    part company whenever the host pointer runs off the window or the guest one hits an edge.
*/

#pragma once
#include <atomic>
#include <Motion.hpp>
#include <base/event/event.hpp>
#include <component/addrspace.hpp>
#include <component/component.hpp>
#include <component/ip2/ip2_interrupt.hpp>

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

    /*
        The quadrature register. A trailing underscore is SGI's notation for active low, and the two
        fire bits are: a *clear* bit means that axis produced a transition, and the direction bit
        beside it says which way. _mouseintr reads this word, tests bit 8 and bit 10, and adds or
        subtracts one from __mousex / __mousey accordingly (sys/ipII/locore.c, and the shipped kernel
        agrees - kd.py 200007a6).

        Reading the register is what dismisses the interrupt, which the driver relies on: the handler
        does one word read and nothing else acknowledges anything.
    */
    #define MOUSE_QUADRATURE_X_FIRED_   (1 << 8)
    #define MOUSE_QUADRATURE_X_POSITIVE (1 << 9)
    #define MOUSE_QUADRATURE_Y_FIRED_   (1 << 10)
    #define MOUSE_QUADRATURE_Y_POSITIVE (1 << 11)

    // Nothing has moved: both fire bits sitting high.
    #define MOUSE_QUADRATURE_IDLE       (MOUSE_QUADRATURE_X_FIRED_ | MOUSE_QUADRATURE_Y_FIRED_)

    /*
        How much unreported movement to keep. One tick is one guest pixel, so this is a screen width
        and a bit of backlog. It matters because the interrupt is on level 7, which a 68020 cannot
        mask: while the guest is not servicing them - the whole of early boot, before the vectors are
        installed - the host can still be moved, and without a cap the pointer would coast for
        seconds afterwards working through movement nobody remembers making.
    */
    #define MOUSE_MAX_PENDING_TICKS     1024

    /*
        How fast the ball is allowed to roll, in transitions a second. This is not a throttle bolted
        on for safety - it is the missing half of the device. A real mouse produces ticks at the rate
        a hand moves it across a desk, a couple of hundred counts an inch, so a thousand a second is
        a brisk sweep and the interrupt costs the guest a fraction of a percent of its time.

        Handing them over as fast as the guest will take them instead - which is what "one interrupt,
        then wait for the acknowledgement" does on its own - gives it tens of thousands a second, and
        the mouse is on level 7, which a 68020 cannot mask. There is no spl the kernel can hide
        behind, so a backlog drains at the cost of every other thing the machine was doing. That is
        invisible while the emulator runs near full speed and starves it outright when it does not:
        it was reproducible with +set logGF2 1 on, where mex would sit runnable and never finish
        starting, and it would have been just as reproducible on a busy host.
    */
    #define MOUSE_TICKS_PER_SECOND      "1000"

    #define LOG_PREFIX_IP2MOUSE         "Emulation - IP2 Mouse"

    // Enough to follow a gesture from end to end without the trace becoming the thing being read.
    #define MOUSE_MAX_LOGGED            2000

    class IP2Mouse : public Component
    {
    public:
        void Start() override;
        void Tick() override;

        const char* GetName() override { return "IP2 Mouse"; };

        uint8_t Read8(size_t addr) override
        {
            if (addr >= MOUSE_QUADRATURE_ADDR)
                return (uint8_t)ReadQuadrature();

            return buttonState.load(std::memory_order_relaxed);
        };

        uint16_t Read16(size_t addr) override
        {
            if (addr >= MOUSE_QUADRATURE_ADDR)
                return ReadQuadrature();

            return buttonState.load(std::memory_order_relaxed);
        };

        // Both registers are read only - the IP2 has nothing to write here.
        void Write8(size_t addr, uint8_t value) override { };
        void Write16(size_t addr, uint16_t value) override { };

        void OnEvent(Event& evt) override;

    private:
        /// @brief Take one tick off the pending movement and dismiss the interrupt that reported it.
        uint16_t ReadQuadrature();

        /// @brief The word the register currently reads, built from the step being reported.
        uint16_t QuadratureWord();

        /// @brief Add host movement to the backlog, clamped so it cannot run away.
        void AddMotion(int32_t deltaX, int32_t deltaY);

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

        /*
            Written by the host's event pump and read by the emulation thread, so both of these are
            atomic. The buttons would survive a torn read; the movement would not, because it is a
            read-modify-write on both sides and a lost update is a pointer that stops short.
        */
        std::atomic<uint8_t> buttonState{ 0 };
        bool logEnabled = false;
        int32_t linesLogged = 0;
        int32_t ticksDelivered = 0;

        std::atomic<int32_t> pendingX{ 0 };
        std::atomic<int32_t> pendingY{ 0 };

        // The transition currently being reported, and whether the guest has been told about it yet.
        int32_t stepX = 0;
        int32_t stepY = 0;
        bool irqAsserted = false;

        // When the last transition was offered, so the next one waits for the ball to have rolled.
        uint64_t lastTransitionNs = 0;
        uint64_t transitionPeriodNs = 0;

        IP2Interrupt* interrupts = nullptr;
    };
};
