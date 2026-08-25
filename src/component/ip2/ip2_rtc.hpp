/* motion - The SGI Emulator. Copyright (c)2026 starfrost. ip2_rtc.hpp: The MC146818 real time clock on the IP2, and the periodic interrupt the kernel schedules on. */

#pragma once
#include <Motion.hpp>
#include <base/emulation.hpp>
#include <component/component.hpp>
#include <component/addrspace.hpp>
#include <component/ip2/ip2_interrupt.hpp>

namespace Motion
{
    #define IP2_CLOCK_START                     0x34000000
    #define IP2_CLOCK_END                       0x35000000

    #define IP2_CLOCK_CTRL                      0x34000000          // strobe lines to the RTC
    #define IP2_CLOCK_DATA                      0x35000000          // address latch, or register data

    #define LOG_PREFIX_IP2RTC                   "Emulation - IP2 RTC"

    // Strobe lines on the control port.
    #define RTC_CTRL_ADDRESS_STROBE             0x01
    #define RTC_CTRL_DATA_STROBE                0x02
    #define RTC_CTRL_READ_ENABLE                0x04
    #define RTC_CTRL_CLOCK_ENABLE               0x08

    // 10 clock/alarm registers, 4 control registers, then 50 bytes of battery backed RAM.
    #define RTC_NUM_REGISTERS                   64

    #define RTC_REG_SECONDS                     0x00
    #define RTC_REG_SECONDS_ALARM               0x01
    #define RTC_REG_MINUTES                     0x02
    #define RTC_REG_MINUTES_ALARM               0x03
    #define RTC_REG_HOURS                       0x04
    #define RTC_REG_HOURS_ALARM                 0x05
    #define RTC_REG_DAY_OF_WEEK                 0x06
    #define RTC_REG_DAY_OF_MONTH                0x07
    #define RTC_REG_MONTH                       0x08
    #define RTC_REG_YEAR                        0x09
    #define RTC_REG_A                           0x0A
    #define RTC_REG_B                           0x0B
    #define RTC_REG_C                           0x0C
    #define RTC_REG_D                           0x0D

    #define RTC_A_UPDATE_IN_PROGRESS            0x80
    #define RTC_A_RATE_SELECT                   0x0F

    #define RTC_B_SET                           0x80        // stop updating the clock
    #define RTC_B_PERIODIC_ENABLE               0x40
    #define RTC_B_ALARM_ENABLE                  0x20
    #define RTC_B_UPDATE_ENABLE                 0x10
    #define RTC_B_SQUARE_WAVE_ENABLE            0x08
    #define RTC_B_BINARY                        0x04        // clear means the clock reads as BCD
    #define RTC_B_24_HOUR                       0x02
    #define RTC_B_DAYLIGHT_SAVING               0x01

    #define RTC_C_IRQ                           0x80
    #define RTC_C_PERIODIC                      0x40
    #define RTC_C_ALARM                         0x20
    #define RTC_C_UPDATE                        0x10

    #define RTC_D_VALID_RAM_AND_TIME            0x80

    // The oscillator is a 32.768kHz watch crystal.
    #define RTC_OSCILLATOR_HZ                   32768

    class IP2Clock : public Component
    {
    public:
        void Start() override
        {
            AddrSpaceMapping mapping = AddrSpaceMapping();

            mapping.component = this;
            mapping.startAddr = IP2_CLOCK_START;
            mapping.endAddr = IP2_CLOCK_END;

            AddrSpace::AddMapping(mapping);

            // A real one has a battery, so the machine expects to find its RAM and time valid.
            registers[RTC_REG_D] = RTC_D_VALID_RAM_AND_TIME;
        }

        const char* GetName() override { return "IP2 Real Time Clock [MC146818]"; };

        // Free running, like the DUART - the periodic interrupt is derived from elapsed time.
        uint32_t GetClockSpeed() override { return 0; };

        void Tick() override;

        uint8_t Read8(size_t addr) override;
        void Write8(size_t addr, uint8_t value) override;

        // The ports are byte wide. A word access lands on the low half.
        uint16_t Read16(size_t addr) override { return Read8(addr); };
        void Write16(size_t addr, uint16_t value) override { Write8(addr, (uint8_t)value); };

    private:
        /// @brief Read one of the RTC's own registers, filling in the time ones as we go.
        uint8_t ReadRegister(uint8_t reg);

        /// @brief Set the periodic flag if another period has gone by.
        void UpdatePeriodic();

        /// @brief Recompute IRQF and drive local interrupt 3.
        void UpdateInterrupt();

        /// @brief Periodic interrupt rate in Hz, or 0 if the rate select is off.
        uint32_t PeriodicRate();

        /// @brief The clock registers read as BCD unless register B says otherwise.
        uint8_t ToClockFormat(int32_t value);

        uint8_t registers[RTC_NUM_REGISTERS] = {0};
        uint8_t address = 0;
        uint8_t control = 0;

        uint64_t lastPeriodicNs = 0;

        IP2Interrupt* interrupts = nullptr;
    };
}
