/*
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    ip2_rtc.cpp: The MC146818 real time clock on the IP2.
*/

#include <ctime>
#include <base/emulation.hpp>
#include <component/ip2/ip2_rtc.hpp>

namespace Motion
{
    uint8_t IP2Clock::Read8(size_t addr)
    {
        if (addr < IP2_CLOCK_DATA)
            return (uint8_t)control;

        // Data only comes back while the read strobes are up. Otherwise the port reads back the
        // address that was latched into it, which is how the PROM checks the latch works.
        if (control == (RTC_CTRL_READ_ENABLE | RTC_CTRL_DATA_STROBE))
            return ReadRegister(address);

        return address;
    }

    void IP2Clock::Write8(size_t addr, uint8_t value)
    {
        if (addr < IP2_CLOCK_DATA)
        {
            control = value;
            return;
        }

        if (control != RTC_CTRL_CLOCK_ENABLE)
        {
            address = value & (RTC_NUM_REGISTERS - 1);
            return;
        }

        switch (address)
        {
            case RTC_REG_A:
                // The update-in-progress bit is read only.
                registers[RTC_REG_A] = value & ~RTC_A_UPDATE_IN_PROGRESS;

                // A change of rate restarts the divider.
                lastPeriodicNs = 0;
                break;

            case RTC_REG_B:
                registers[RTC_REG_B] = value;
                UpdateInterrupt();
                break;

            // C and D are read only.
            case RTC_REG_C:
            case RTC_REG_D:
                break;

            default:
                registers[address] = value;
                break;
        }
    }

    /*
        The clock and calendar registers are not stored - they are produced from the host clock on
        demand, so the emulated machine comes up believing it is whatever time it actually is.
        Everything else is a plain register or battery backed RAM.
    */
    uint8_t IP2Clock::ReadRegister(uint8_t reg)
    {
        if (reg == RTC_REG_C)
        {
            // "The IRQF, PF, AF and UF bits are cleared when register C is read."
            uint8_t value = registers[RTC_REG_C];

            registers[RTC_REG_C] = 0;
            UpdateInterrupt();

            return value;
        }

        if (reg > RTC_REG_YEAR)
            return registers[reg];

        std::time_t now = std::time(nullptr);
        std::tm local = {};

    #ifdef _WIN32
        localtime_s(&local, &now);
    #else
        localtime_r(&now, &local);
    #endif

        switch (reg)
        {
            case RTC_REG_SECONDS:
                return ToClockFormat(local.tm_sec);
            case RTC_REG_MINUTES:
                return ToClockFormat(local.tm_min);
            case RTC_REG_HOURS:
            {
                if (registers[RTC_REG_B] & RTC_B_24_HOUR)
                    return ToClockFormat(local.tm_hour);

                // 12 hour mode: 12am reads as 12, and the top bit marks pm.
                int32_t hour = local.tm_hour % 12;
                uint8_t value = ToClockFormat(hour ? hour : 12);

                return (local.tm_hour >= 12) ? (value | 0x80) : value;
            }
            case RTC_REG_DAY_OF_WEEK:
                return ToClockFormat(local.tm_wday + 1);        // the part counts from 1
            case RTC_REG_DAY_OF_MONTH:
                return ToClockFormat(local.tm_mday);
            case RTC_REG_MONTH:
                return ToClockFormat(local.tm_mon + 1);
            case RTC_REG_YEAR:
                return ToClockFormat(local.tm_year % 100);

            // the alarm registers are just storage until an alarm is implemented
            default:
                return registers[reg];
        }
    }

    uint8_t IP2Clock::ToClockFormat(int32_t value)
    {
        if (registers[RTC_REG_B] & RTC_B_BINARY)
            return (uint8_t)value;

        return (uint8_t)(((value / 10) << 4) | (value % 10));
    }

    /*
        Rate select picks a tap off the divider chain. Codes 1 and 2 are the odd ones out - they are
        the same two rates as 8 and 9 rather than continuing the halving - and 0 turns it off.
    */
    uint32_t IP2Clock::PeriodicRate()
    {
        uint32_t rate = registers[RTC_REG_A] & RTC_A_RATE_SELECT;

        if (!rate)
            return 0;

        if (rate == 1)
            rate = 8;
        else if (rate == 2)
            rate = 9;

        return RTC_OSCILLATOR_HZ >> (rate - 1);
    }

    void IP2Clock::UpdatePeriodic()
    {
        uint32_t rate = PeriodicRate();

        if (!rate)
            return;

        uint64_t now = Chrono_GetTicksNS(Chrono_GetTime());
        uint64_t periodNs = 1000000000ull / rate;

        if (!lastPeriodicNs)
        {
            lastPeriodicNs = now;
            return;
        }

        if ((now - lastPeriodicNs) < periodNs)
            return;

        /*
            Only ever one period behind. The emulated machine runs slower than the host clock, so
            catching up tick for tick after a slow patch would deliver a burst of clock interrupts
            the kernel would count as real elapsed time.
        */
        lastPeriodicNs = now;

        registers[RTC_REG_C] |= RTC_C_PERIODIC;
        UpdateInterrupt();
    }

    void IP2Clock::UpdateInterrupt()
    {
        uint8_t b = registers[RTC_REG_B];
        uint8_t c = registers[RTC_REG_C];

        bool asserted = ((c & RTC_C_PERIODIC) && (b & RTC_B_PERIODIC_ENABLE))
            || ((c & RTC_C_ALARM) && (b & RTC_B_ALARM_ENABLE))
            || ((c & RTC_C_UPDATE) && (b & RTC_B_UPDATE_ENABLE));

        if (asserted)
            registers[RTC_REG_C] |= RTC_C_IRQ;
        else
            registers[RTC_REG_C] &= ~RTC_C_IRQ;

        if (!interrupts)
            interrupts = Emulation::GetMachine()->FindComponentByType<IP2Interrupt>();

        if (interrupts)
            interrupts->SetLocalInterrupt(IP2_LOCAL_RTC, asserted);
    }

    void IP2Clock::Tick()
    {
        UpdatePeriodic();
    }
}
