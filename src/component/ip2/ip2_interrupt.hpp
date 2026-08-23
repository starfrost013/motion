/*
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    ip2_interrupt.hpp: The interrupt logic on the IP2 board.

    The 68020 has three IPL pins and the IP2 drives them from two groups of sources: the eight shared
    Multibus interrupt lines, and a handful of "local" interrupts from the parts on the board itself.
    Several sources share a level, so an interrupt acknowledge cycle reads a vector number out of a
    PROM addressed by the level and the state of the local lines.

    Nothing here decodes an address of its own. The master enable lives in the status register, which
    belongs to the MMU (0x38000000), and that forwards it here.
*/

#pragma once
#include <Motion.hpp>
#include <component/component.hpp>
#include <component/cpu/cpu.hpp>

namespace Motion
{
    #define LOG_PREFIX_IP2INT               "Emulation - IP2 Interrupts"

    #define IP2_NUM_IRQ_LEVELS              8
    #define IP2_NUM_MULTIBUS_IRQ            8

    /*
        U118, a 27S29 512x8 PROM, turns the interrupt level and the state of the local interrupt lines
        into a vector number. The part is not in the archive, so this reproduces its contents from the
        table in the IP2 documentation, which the kernel's own vector table agrees with exactly:

            level 1   0x41  Multibus 0 and 1
            level 2   0x42  Multibus 2
            level 3   0x43  Multibus 3
            level 4   0x44  Multibus 4
            level 5   0x45  Multibus 5
            level 6   0x46  Multibus 6,  or 0x50 uart0, 0x51 uart1, 0x52 ext, 0x53 rtc
            level 7   0x47  Multibus 7,  or 0x55 parity, 0x56 mouse, 0x57 mouse unplugged

        and the handlers the kernel installs at those vectors:

            0x41-0x46  Xmbintr2..Xmbintr7      0x50  ducom      0x51  Xclock
            0x53       _save                   0x55  Xaddrerr   0x56/0x57  Xduart0

        Note that the scheduler clock is uart1, not the RTC.
    */
    #define IP2_VECTOR_MULTIBUS_BASE        0x40        // + level
    #define IP2_VECTOR_LOCAL_BASE           0x50        // + local source index

    /// @brief The interrupt sources on the board itself. The index is also the vector offset.
    enum IP2LocalInterrupt : int32_t
    {
        IP2_LOCAL_DUART0        = 0,        // vector 0x50, level 6 - console and keyboard
        IP2_LOCAL_DUART1        = 1,        // vector 0x51, level 6 - the scheduler clock
        IP2_LOCAL_EXTERNAL      = 2,        // vector 0x52, level 6
        IP2_LOCAL_RTC           = 3,        // vector 0x53, level 6
        IP2_LOCAL_PARITY        = 5,        // vector 0x55, level 7
        IP2_LOCAL_MOUSE         = 6,        // vector 0x56, level 7
        IP2_LOCAL_MOUSE_UNPLUG  = 7,        // vector 0x57, level 7
        IP2_NUM_LOCAL_INTERRUPTS = 8,
    };

    // Which sources sit on which level. Local 4 is not wired to anything.
    #define IP2_LOCAL_MASK_LEVEL6           0x0F        // duart0, duart1, ext, rtc
    #define IP2_LOCAL_MASK_LEVEL7           0xE0        // parity, mouse, mouse unplugged

    class IP2Interrupt : public Component
    {
    public:
        // The MMU forwards the status register here and devices assert through here, so both need to
        // be able to find it already started.
        bool IsEarlyStart() override { return true; };

        const char* GetName() override { return "IRIS 3130 IP2 Interrupt Logic"; };

        void Start() override {};
        void Shutdown() override { cpu = nullptr; };

        /// @brief Assert or release one of the eight shared Multibus interrupt lines.
        void SetMultibusIRQ(int32_t number, bool asserted);

        /// @brief Assert or release one of the interrupt sources on the IP2 itself.
        void SetLocalInterrupt(IP2LocalInterrupt source, bool asserted);

        /// @brief Master interrupt enable, from ST_ENABINT in the status register.
        void SetEnabled(bool enabled);

        /// @brief The vector an interrupt acknowledge cycle at this level reads out of U118.
        uint8_t GetVector(int32_t level);

        /// @brief Which levels are currently being driven, for the debugger.
        uint8_t GetPendingLevels() { return PendingLevels(); };

    private:
        /// @brief Fold the sources into a bitmask of the seven interrupt levels.
        uint8_t PendingLevels();

        /// @brief Work out the highest pending level and hand it to the CPU.
        void Update();

        uint8_t multibusAsserted = 0;
        uint8_t localAsserted = 0;
        bool enabled = false;
        int32_t lastLevel = 0;

        ComponentCPU* cpu = nullptr;
    };
}
