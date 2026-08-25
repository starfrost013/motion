/* motion - The SGI Emulator. Copyright (c)2026 starfrost. ip2_interrupt.cpp: The interrupt logic on the IP2 board. */

#include <base/emulation.hpp>
#include <component/ip2/ip2_interrupt.hpp>

namespace Motion
{
    void IP2Interrupt::SetMultibusIRQ(int32_t number, bool asserted)
    {
        if (number < 0 || number >= IP2_NUM_MULTIBUS_IRQ)
        {
            Logger::Log(LOG_PREFIX_IP2INT, std::format("Tried to drive invalid Multibus IRQ #{}", number).c_str(), LogChannels::Warning);
            return;
        }

        uint8_t before = multibusAsserted;

        if (asserted)
            multibusAsserted |= (1 << number);
        else
            multibusAsserted &= ~(1 << number);

        if (multibusAsserted != before)
            Update();
    }

    void IP2Interrupt::SetLocalInterrupt(IP2LocalInterrupt source, bool asserted)
    {
        uint8_t before = localAsserted;

        if (asserted)
            localAsserted |= (1 << source);
        else
            localAsserted &= ~(1 << source);

        if (localAsserted != before)
            Update();
    }

    void IP2Interrupt::SetEnabled(bool value)
    {
        if (enabled == value)
            return;

        enabled = value;
        Update();
    }

    // Multibus 0 and 1 share level 1; 2 to 7 get a level each.
    uint8_t IP2Interrupt::PendingLevels()
    {
        uint8_t levels = 0;

        if (multibusAsserted & 0x03)
            levels |= (1 << 1);

        for (int32_t i = 2; i < IP2_NUM_MULTIBUS_IRQ; i++)
        {
            if (multibusAsserted & (1 << i))
                levels |= (1 << i);
        }

        if (localAsserted & IP2_LOCAL_MASK_LEVEL6)
            levels |= (1 << 6);

        if (localAsserted & IP2_LOCAL_MASK_LEVEL7)
            levels |= (1 << 7);

        return levels;
    }

    void IP2Interrupt::Update()
    {
        if (!cpu)
            cpu = Emulation::GetMachine()->FindComponentByType<ComponentCPU>();

        if (!cpu)
            return;

        // ST_ENABINT gates the lot, as in MAME: int_w only reaches the CPU while the status register enable is set.
        uint8_t levels = enabled ? PendingLevels() : 0;
        int32_t level = 0;

        for (int32_t i = IP2_NUM_IRQ_LEVELS - 1; i > 0; i--)
        {
            if (levels & (1 << i))
            {
                level = i;
                break;
            }
        }

        if (level == lastLevel)
            return;

        lastLevel = level;
        cpu->SetIRQLine(level);
    }

    // The level alone picks the vector for everything except levels 6 and 7, where whichever local source is asserting decides.
    uint8_t IP2Interrupt::GetVector(int32_t level)
    {
        uint8_t candidates = 0;

        if (level == 6)
            candidates = localAsserted & IP2_LOCAL_MASK_LEVEL6;
        else if (level == 7)
            candidates = localAsserted & IP2_LOCAL_MASK_LEVEL7;

        for (int32_t i = 0; i < IP2_NUM_LOCAL_INTERRUPTS; i++)
        {
            if (candidates & (1 << i))
                return (uint8_t)(IP2_VECTOR_LOCAL_BASE + i);
        }

        // nothing local, so it is the Multibus line that shares the level
        return (uint8_t)(IP2_VECTOR_MULTIBUS_BASE + level);
    }
}
