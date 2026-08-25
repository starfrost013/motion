/* motion - The SGI Emulator. Copyright (c)2026 starfrost. ip2_mouse.hpp: IP2 mouse button and quadrature registers. */

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

    // Not a button - the PROM reads it to tell Rev B from Rev A. Held clear, because Rev A is what the rest of this models.
    #define MOUSE_BOARD_REVISION_B      (1 << 4)

    // The quadrature register.
    #define MOUSE_QUADRATURE_X_FIRED_   (1 << 8)
    #define MOUSE_QUADRATURE_X_POSITIVE (1 << 9)
    #define MOUSE_QUADRATURE_Y_FIRED_   (1 << 10)
    #define MOUSE_QUADRATURE_Y_POSITIVE (1 << 11)

    // Nothing has moved: both fire bits sitting high.
    #define MOUSE_QUADRATURE_IDLE       (MOUSE_QUADRATURE_X_FIRED_ | MOUSE_QUADRATURE_Y_FIRED_)

    // How much unreported movement to keep.
    #define MOUSE_MAX_PENDING_TICKS     1024

    // How fast the ball is allowed to roll, in transitions a second.
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

        // Written by the host's event pump and read by the emulation thread, so both of these are atomic.
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
