/* motion - The SGI Emulator. Copyright (c)2026 danifunker. ip2_mouse.cpp: the quadrature encoder, and the interrupt it raises. */

#include <base/emulation.hpp>
#include <component/ip2/ip2_mouse.hpp>

namespace Motion
{
    void IP2Mouse::Start()
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

        logEnabled = Cvar::Get("logMouse", "0")->GetValue();

        uint32_t ticksPerSecond = (uint32_t)Cvar::Get("mouseTicksPerSecond", MOUSE_TICKS_PER_SECOND)->GetValue();

        transitionPeriodNs = ticksPerSecond ? (1000000000ull / ticksPerSecond) : 0;
    }

    void IP2Mouse::OnEvent(Event& evt)
    {
        if (evt.type == EventType::MouseDown)
        {
            buttonState.fetch_or(ButtonMaskFor(((MouseDownEvent&)evt).mouse), std::memory_order_relaxed);

            if (logEnabled)
                Logger::Log(LOG_PREFIX_IP2MOUSE, std::format("button {} down, buttons now 0x{:02x}",
                    ((MouseDownEvent&)evt).mouse, buttonState.load()).c_str(), LogChannels::Warning);
            return;
        }

        if (evt.type == EventType::MouseUp)
        {
            buttonState.fetch_and((uint8_t)~ButtonMaskFor(((MouseUpEvent&)evt).mouse), std::memory_order_relaxed);
            return;
        }

        if (evt.type == EventType::MouseMotion)
        {
            MouseMotionEvent& motion = (MouseMotionEvent&)evt;

            if (logEnabled && linesLogged < MOUSE_MAX_LOGGED)
            {
                linesLogged++;
                Logger::Log(LOG_PREFIX_IP2MOUSE, std::format("host motion ({}, {}), backlog now ({}, {}), {} ticks handed over so far",
                    motion.deltaX, motion.deltaY, pendingX.load(), pendingY.load(), ticksDelivered).c_str(), LogChannels::Warning);
            }

            // The host's y grows downwards and GL's grows upwards, and the driver does not flip it anywhere: gl_valuators takes MOUSEY straight through to the.
            AddMotion((int32_t)motion.deltaX, -(int32_t)motion.deltaY);
        }
    }

    // Add to the backlog, saturating rather than wrapping.
    void IP2Mouse::AddMotion(int32_t deltaX, int32_t deltaY)
    {
        auto accumulate = [](std::atomic<int32_t>& pending, int32_t delta)
        {
            if (!delta)
                return;

            int32_t current = pending.load(std::memory_order_relaxed);
            int32_t wanted;

            do
            {
                wanted = current + delta;

                if (wanted > MOUSE_MAX_PENDING_TICKS)
                    wanted = MOUSE_MAX_PENDING_TICKS;
                else if (wanted < -MOUSE_MAX_PENDING_TICKS)
                    wanted = -MOUSE_MAX_PENDING_TICKS;
            }
            while (!pending.compare_exchange_weak(current, wanted, std::memory_order_relaxed));
        };

        accumulate(pendingX, deltaX);
        accumulate(pendingY, deltaY);
    }

    uint16_t IP2Mouse::QuadratureWord()
    {
        uint16_t word = MOUSE_QUADRATURE_IDLE;

        if (stepX)
        {
            word &= ~MOUSE_QUADRATURE_X_FIRED_;

            if (stepX > 0)
                word |= MOUSE_QUADRATURE_X_POSITIVE;
        }

        if (stepY)
        {
            word &= ~MOUSE_QUADRATURE_Y_FIRED_;

            if (stepY > 0)
                word |= MOUSE_QUADRATURE_Y_POSITIVE;
        }

        return word;
    }

    // The handler's one and only access.
    uint16_t IP2Mouse::ReadQuadrature()
    {
        uint16_t word = QuadratureWord();

        if (!irqAsserted)
            return word;

        pendingX.fetch_sub(stepX, std::memory_order_relaxed);
        pendingY.fetch_sub(stepY, std::memory_order_relaxed);

        ticksDelivered++;

        if (logEnabled && linesLogged < MOUSE_MAX_LOGGED && (ticksDelivered % 50) == 0)
        {
            linesLogged++;
            Logger::Log(LOG_PREFIX_IP2MOUSE, std::format("{} ticks acknowledged, backlog now ({}, {})",
                ticksDelivered, pendingX.load(), pendingY.load()).c_str(), LogChannels::Warning);
        }

        stepX = 0;
        stepY = 0;
        irqAsserted = false;

        if (interrupts)
            interrupts->SetLocalInterrupt(IP2_LOCAL_MOUSE, false);

        return word;
    }

    // Offer the next transition, if the guest has finished with the last one.
    void IP2Mouse::Tick()
    {
        if (irqAsserted)
            return;

        int32_t remainingX = pendingX.load(std::memory_order_relaxed);
        int32_t remainingY = pendingY.load(std::memory_order_relaxed);

        if (!remainingX && !remainingY)
            return;

        // The ball has to have rolled far enough for the next grating transition.
        if (transitionPeriodNs)
        {
            uint64_t now = Chrono_GetTicksNS(Chrono_GetTime());

            if (!lastTransitionNs)
                lastTransitionNs = now;
            else if ((now - lastTransitionNs) < transitionPeriodNs)
                return;

            lastTransitionNs = now;
        }

        if (!interrupts)
        {
            interrupts = Emulation::GetMachine()->FindComponentByType<IP2Interrupt>();

            if (!interrupts)
                return;
        }

        stepX = (remainingX > 0) - (remainingX < 0);
        stepY = (remainingY > 0) - (remainingY < 0);
        irqAsserted = true;

        interrupts->SetLocalInterrupt(IP2_LOCAL_MOUSE, true);
    }
};
