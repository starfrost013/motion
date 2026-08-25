/*
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    ip2_duart.hpp: Emulates two SCN68681 UARTs mapped at 32000000 and 32800000 respectively

    Largely adapted from the MAME emulation:
    https://github.com/mamedev/mame/blob/master/src/devices/machine/mc68681.h
    https://github.com/mamedev/mame/blob/master/src/devices/machine/mc68681.cpp
*/

#pragma once
#include <Motion.hpp>
#include <base/filesystem/filesystem.hpp>
#include <component/addrspace.hpp>
#include <component/ip2/ip2_interrupt.hpp>
#include <component/serial/serial.hpp>
#include <coherent/coherent.hpp>

namespace Motion
{
    extern Cvar* logIP2DUART;

    #define DUART_LOG_CHANNEL_NAME                  "IP2 DUART"

    #define DUART0_START                            0x32000000
    #define DUART1_START                            0x32800000
    #define DUART_NUM_REGS                          16
    #define DUART_NUM_INPUT_PORTS                   7

    /*
        The counter/timer is a 16 bit down counter clocked from whichever source ACR[6:4] selects.
        Only the crystal sources are wired on this board - the external IP2 pin and the two
        transmitter clocks go nowhere. X1 is the standard 3.6864MHz DUART crystal, which is the same
        one the baud rate tables further down assume.

        The kernel needs this before it will get anywhere: _calibuzz starts the counter, spins its
        software delay loop, stops the counter and divides to work out how many loop iterations make a
        millisecond. With a counter that never counts, that division produces a delay loop that never
        finishes and _msdelay never returns.
    */
    #define DUART_X1_HZ                             3686400
    #define DUART_COUNTER_MODE(acr)                 (((acr) >> 4) & 0x7)
    #define DUART_COUNTER_MODE_IS_TIMER(acr)        (DUART_COUNTER_MODE(acr) & 0x4)

    // There is a small fifo in the 68681 for receiving bits
    #define DUART_FIFO_SIZE                         3

    #define DUART_NUM_CHANNELS                      2

    // Which (duart chip, channel) raw serial line is connected to the host console by default. Also gets shown by the COherent window
    #define DUART_PORT2_DUART_INDEX                 0
    #define DUART_PORT2_CHANNEL                     1

    //
    // Registers
    //

    // Universal registers (read/write) [bit3 selects A/B]
    #define DUART_MODE_A                            0x0             // 0x0: [Read/Write] Mode Register A
    #define DUART_MODE_B                            0x8             // 0x8: [Read/Write] Mode Register B
    #define DUART_INTERRUPT_VECTOR                  0xC             // 0xC: [Read/Write] Interrupt Vector

    // Mode register bits

    #define DUART_MODE_RX_INT_SELECT_BIT            (1 << 6)
    #define DUART_MODE_BLOCK_ERROR                  (1 << 5)
    #define DUART_MODE_PARITY_MODE                  (3 << 3)
    #define DUART_MODE_PARITY_MODE_WITH             0x0
    #define DUART_MODE_PARITY_MODE_FORCE            0x1
    #define DUART_MODE_PARITY_MODE_NONE             0x2
    #define DUART_MODE_PARITY_MODE_MULTIDROP        0x3
    #define DUART_MODE_BITS_PER_CHAR                 (3 << 0)

    // Read registers

    // Interrupts
    #define DUART_INT_INPUT_PORT_CHANGE             (1 << 7)        // input port changed
    #define DUART_INT_DELTA_BREAK_B                 (1 << 6)        // delta break on channel b
    #define DUART_INT_RXRDY_FFULLB                  (1 << 5)        // rx fifo full on channel b
    #define DUART_INT_TXRDYB                        (1 << 4)        // tx ready on channel b
    #define DUART_INT_COUNTER_READY                 (1 << 3)        // counter ready
    #define DUART_INT_DELTA_BREAK_A                 (1 << 2)        // delta break on channel a
    #define DUART_INT_RXRDY_FFULLA                  (1 << 1)        // rx fifo full on channel a
    #define DUART_INT_TXRDYA                        (1 << 0)        // tx ready on channel a

    // Status register bits
    #define DUART_STATUS_RECEIVED_BREAK             (1 << 7)        // break received
    #define DUART_STATUS_FRAMING_ERROR              (1 << 6)        // data framing failed
    #define DUART_STATUS_PARITY_ERROR               (1 << 5)        // parity error
    #define DUART_STATUS_OVERRUN_ERROR              (1 << 4)        // overrun fifo
    #define DUART_STATUS_TRANSMITTER_EMPTY          (1 << 3)        // transmitter empty
    #define DUART_STATUS_TRANSMITTER_READY          (1 << 2)        // ready to transmit
    #define DUART_STATUS_FIFO_FULL                  (1 << 1)        // FIFO is full
    #define DUART_STATUS_RECEIVER_READY             (1 << 0)        // receiver is ready.

    // Command IDs for 0x2 write
    #define DUART_COMMAND_NOP                       0x0             // do nothing
    #define DUART_COMMAND_RESET_MR_PTR              0x1             // reset modereg pointer (autoincrements but not autoresets lmao)
    #define DUART_COMMAND_RESET_CHAN_RECEIVER       0x2             // reset channel receiver
    #define DUART_COMMAND_RESET_CHAN_TRANSMITTER    0x3             // reset channel transmitter
    #define DUART_COMMAND_RESET_ERROR_STATUS        0x4             // reset error status
    #define DUART_COMMAND_RESET_CHANNEL_BRK_CHANGE  0x5             // reset channel brk change
    #define DUART_COMMAND_START_TX_BREAK            0x6             // start tx break
    #define DUART_COMMAND_STOP_TX_BREAK             0x7             // stop tx break

    // Lower nibble of the command register: independent enable/disable commands (CRx[3:0])
    #define DUART_COMMAND_DISABLE_TX                (1 << 3)
    #define DUART_COMMAND_ENABLE_TX                 (1 << 2)
    #define DUART_COMMAND_DISABLE_RX                (1 << 1)
    #define DUART_COMMAND_ENABLE_RX                 (1 << 0)

    // On-read registers
    #define DUART_READ_STATUS_A                     0x1             // 0x1: [Read] Status Register A
    #define DUART_READ_BRG_TEST                     0x2             // 0x2: [Read] BRG Test
    #define DUART_READ_RX_HOLD_A                    0x3             // 0x3: [Read] Rx Holding Register A
    #define DUART_READ_INPUT_PORT_CHANGE            0x4             // 0x4: [Read] Input Port Change
    #define DUART_READ_INTERRUPT_STATUS             0x5             // 0x5: [Read] Interrupt Status
    #define DUART_READ_COUNTER_UPPER                0x6             // 0x6: [Read] Counter/Timer Upper Byte
    #define DUART_READ_COUNTER_LOWER                0x7             // 0x7: [Read] Counter/Timer Lower Byte
    #define DUART_READ_STATUS_B                     0x9             // 0x9: [Read] Status Register B
    #define DUART_READ_1X16X                        0xA             // 0xA: [Read] 1x/16x Test
    #define DUART_READ_RX_HOLD_B                    0xB             // 0xB: [Read] Rx Holding Register B
    #define DUART_READ_INPUT_PORTS                  0xD             // 0xD: [Read] Input Ports IP0-IP6

    /*
        Carrier detect, and it is **active low** - a clear bit means carrier present. sduart.c says so
        outright: "the input is low-true", and du_act() clears DP_DCD when the bit reads set.

            #if defined(IP2) || defined(IP4)
            #define IPORT_DCDA  0x08        // dcd input bit for A ports
            #define IPORT_DCDB  0x04        // dcd input bit for B ports

        This matters the moment anything reaches multi-user. /etc/gettydefs' co_9600 is
        "B9600 SANE TAB3" with no CLOCAL, so du_open() waits for carrier before it will open the line,
        and a getty that never opens is a console that never prints `login:`.
    */
    #define DUART_IPORT_DCDA                        0x08
    #define DUART_IPORT_DCDB                        0x04
    #define DUART_READ_START_COUNTER_CMD            0xE             // 0xE: [Read] Start Counter Command
    #define DUART_READ_STOP_COUNTER_CMD             0xF             // 0xF: [Read] Stop Counter Command

    // Write registers
    #define DUART_WRITE_CLOCKSEL_A                  0x1             // 0x1: [Write] Clock Select Register A
    #define DUART_WRITE_COMMAND_A                   0x2             // 0x2: [Write] Command Register A
    #define DUART_WRITE_TX_HOLD_A                   0x3             // 0x3: [Write] Tx Holding Register A
    #define DUART_WRITE_AUX_CONTROL                 0x4             // 0x4: [Write] Aux Control
    #define DUART_WRITE_INTERRUPT_MASK              0x5             // 0x5: [Write] Interrupt Mask
    #define DUART_WRITE_COUNTER_UPPER               0x6             // 0x6: [Write] Counter/Timer Preset Upper Byte
    #define DUART_WRITE_COUNTER_LOWER               0x7             // 0x7: [Write] Counter/Timer Preset Lower Byte
    #define DUART_WRITE_CLOCKSEL_B                  0x9             // 0x9: [Write] Clock Select Register B
    #define DUART_WRITE_COMMAND_B                   0xA             // 0xA: [Write] Command Register B
    #define DUART_WRITE_TX_HOLD_B                   0xB             // 0xB: [Write] Tx Holding Register B
    #define DUART_WRITE_OUTPUT_PORT_CONF            0xD             // 0xD: [Write] Output Port Configuration
    #define DUART_WRITE_SET_OUTPUT_PORT_BITS_CMD    0xE             // 0xE: [Write] Set Output Port Bits
    #define DUART_WRITE_RESET_OUTPUT_PORT_BITS_CMD  0xF             // 0xF: [Write] Reset Output Port Bits

    #define DUART_LOG_PREFIX                        "Emulation - IP2 DUART"
    // FOR COMPONENTS, WE DON'T NEED TO BOUNDS CHECK BECAUSE WE ALREADY MAPPED THE RIGHT SIZE!

    class DUART68681;

    /// @brief The DUART's Coherent extension. Check ip2_duart_debug.cpp!
    class CoherentExtensionDUART68681 : public CoherentExtension
    {
    public:
        CoherentExtensionDUART68681(Component* owner) : CoherentExtension(owner) {}

        void AddUI() override;

        CoherentExtensionType GetExtensionType() override { return CoherentExtensionType::CustomMenuItem; }; 
        const char* GetMenuName() override { return "System Console"; };
    private: 
        void DrawChannelUI(DUART68681* duartComponent, int32_t duartId, int32_t channelId);
        void DrawConsole(SerialLine& line, int32_t lineNum, float outputHeight);
        void DrawStatusFlag(const char* name, bool set, bool badWhenSet = false);
    };

    class DUART68681 : public ComponentSerial
    {

    public:
        void Start() override
        {
            ComponentSerial::Start();
            
            // map the DUARTs
            AddrSpaceMapping mapping0 = AddrSpaceMapping();

            mapping0.startAddr = DUART0_START;
            mapping0.endAddr = DUART0_START + DUART_NUM_REGS - 1;   // GetMapping's end is inclusive
            mapping0.component = this;

            AddrSpaceMapping mapping1 = AddrSpaceMapping();

            mapping1.startAddr = DUART1_START;
            mapping1.endAddr = DUART1_START + DUART_NUM_REGS - 1;
            mapping1.component = this;

            AddrSpace::AddMapping(mapping0);
            AddrSpace::AddMapping(mapping1);

            // RESETN initialises the IVR to 0x0F and establishes sane default framing for each channel so that
            // GetFrameBits() doesn't return garbage before the PROM has had a chance to program MR1/MR2.
            for (int32_t d = 0; d < 2; d++)
            {
                duarts[d].ivr = 0x0F;

                for (int32_t c = 0; c < DUART_NUM_CHANNELS; c++)
                    UpdateDataFrameState(d, c);
            }

            duartExtension = new CoherentExtensionDUART68681(this);
            Coherent::RegisterExtension(duartExtension);

            // init misc stuff
            logIP2DUART = Cvar::Get("logIP2DUART", "0");
            duartChannel = LogChannel(DUART_LOG_CHANNEL_NAME, ConsoleColor::BrightGreen, ConsoleColor::White);
            Logger::AddChannel(duartChannel);
            logEnabled = logIP2DUART->GetValue();

            if (logEnabled)
                Logger::SetChannelEnabled(DUART_LOG_CHANNEL_NAME);
        }

        void Shutdown() override
        {
            delete duartExtension;
        }

        const char* GetName() override { return "Dual Signetics SCN68681 DUART (IP2/U130 & U131)"; };

        int GetDuartIONum(size_t addr) { return (addr & 0x800000) ? 1 : 0; }

        /// @brief Maps a (duart chip, channel) pair onto this component's raw serial line index (see ComponentSerial).
        int32_t GetLineIndex(int32_t duartId, int32_t channelId) { return duartId * DUART_NUM_CHANNELS + channelId; }

        void Tick() override;

        uint8_t Read8(size_t addr) override;
        uint16_t Read16(size_t addr) override;
        uint32_t Read32(size_t addr) override;
        void Write8(size_t addr, uint8_t value) override;
        void Write16(size_t addr, uint16_t value) override;
        void Write32(size_t addr, uint32_t value) override;

        // there are two uart channels
        struct UARTChannel
        {
            int32_t baudRateRX, baudRateTX;
            uint8_t command;
            uint8_t clocksel;
            uint8_t mode1, mode2;
            uint8_t modeRegCurrent;
            uint16_t status;

            // transmit/receive
            uint8_t txEnabled, rxEnabled;
            uint8_t txHold, rxHold;                             // THR (txHold); rxHold is currently unused, the FIFO is authoritative for RX
            uint8_t txData, rxData;                             // shift register bytes currently being (de)serialised

            bool txBreak;
            bool txHoldFull;                                    // true if txHold holds a byte waiting to move into the shift register
            bool txShiftBusy;                                   // true if the transmit shift register is currently sending a byte
            bool rxShiftBusy;                                   // true if the receive shift register is currently assembling a byte
            uint8_t txBitsTransmitted;
            uint8_t rxBitsReceived;
            uint8_t txPrescale, rxPrescale;

            // Data framing, derived from mode1/mode2 by UpdateDataFrameState(). Since bytes are moved whole
            // (rather than bit-by-bit) between here and the raw SerialLine, these are only used to work out
            // roughly how many bit-clock ticks one character should take (see GetFrameBits()).
            uint8_t dataBits;
            uint8_t stopBitPeriods;                             // rounded to 1 or 2 whole bit-times
            bool parityEnabled;

            uint8_t rxFifo[DUART_FIFO_SIZE + 1];
            uint8_t rxFifoReadPtr, rxFifoWritePtr;              // read/write rx fifo ptrs
            uint8_t rxFifoFree;                                 // count of unread bytes currently buffered in the FIFO

            // Per-channel bit clocks - each channel has its own independent baud rate, so these can't be shared
            // at the DUART (chip) level.
            uint64_t rxClkNs, lastRxClkNs;
            uint64_t txClkNs, lastTxClkNs;
        };

        struct DUART
        {
            uint8_t isr;                // interrupt status register [Read]
            uint8_t ivr;                // interrupt vector register [Read/Write]
            uint8_t imr;                // interrupt mask
            uint16_t counter;           // counter/timer
            uint16_t counterPreset;     // counter/timer preset value
            bool counterRunning;        // true between a start counter command and a stop counter command
            uint64_t counterStartNs;    // host time the counter was last loaded, for UpdateCounter
            uint8_t auxControl;         // auxillary control (misc.)

            uint8_t brgTest;            // allows extended / nonstandard baud rates ? maybe only on later models
            uint8_t opr;                // output port register (OP0-OP7, active low - real pin state is the complement of this)
            uint8_t opcr;               // output port configuration register
            uint8_t ipcr;               // input port change register (delta bits 7:4, current-state bits 3:0)

            // Input ports
            uint8_t inputPorts[DUART_NUM_INPUT_PORTS] = {0};
            UARTChannel channels[DUART_NUM_CHANNELS] = {0};

        };

        DUART duarts[2] = {0};

        // clocks

        int32_t ip2clk;             // General purpose input, or Channel B receiver external clock input (RxCB),
        int32_t ip3clk;             // General purpose input, or Channel A transmitter external clock input (TxCA).
        int32_t ip4clk;             // General purpose input, or Channel A receiver external clock input (RxCA). .
        int32_t ip5clk;             // General purpose input, or Channel B transmitter external clock input (TxCB). .

        /// @brief Returns the clock speed of the component in hertz. This is deliberately set to zero so it will be called AFAP.
        /// The reason for this is that there are two clocks.
        /// @return Returns the clock speed of the component in hertz
        uint32_t GetClockSpeed() override { return 0; };

        bool logEnabled = false;

        IP2Interrupt* interrupts = nullptr;
    private:
        /// @brief Bring the counter/timer up to the current time and update the counter ready bit.
        void UpdateCounter(int32_t duartId);

        /// @brief One counter/timer tick in nanoseconds, or 0 if ACR selects a source nothing is connected to.
        uint64_t GetCounterTickNs(int32_t duartId);

        /// @brief set the baud rate.
        /// @param channel The channel to use the clock source for.
        /// @param isRx TRUE - set transmit baud rate; FALSE - set receive baud rate.
        /// @param data Source data.
        void SetBaudRate(int32_t duart, int32_t channelId, bool isRx, uint8_t data);

        /// @brief Recomputes both the RX and TX baud rate (and bit-clock periods) for a channel from its
        /// clock select register. Called whenever CSRx or ACR (which affects the whole chip) is written.
        void RecomputeChannelClocks(int32_t duartId, int32_t channelId);

        void UpdateDataFrameState(int32_t duart, int32_t channel);
        void UpdateInterruptState(int32_t duart, int32_t channel);

        /// @brief Number of bit-clock ticks (start + data + parity + stop) one character takes for a channel.
        uint32_t GetFrameBits(UARTChannel& channel);

        void OnRxClock(int32_t duartId, int32_t channelId);
        void OnTxClock(int32_t duartId, int32_t channelId);
        void SetRxClock(int32_t duartId, int32_t channelId, uint32_t hz);
        void SetTxClock(int32_t duartId, int32_t channelId, uint32_t hz);

        // Baud rate constants

        inline static const int baudRateACR0[] = { 50, 110, 134, 200, 300, 600, 1200, 1050, 2400, 4800, 7200, 9600, 38400, 0, 0, 0 };
        inline static const int baudRateACR1[] = { 75, 110, 134, 150, 300, 600, 1200, 2000, 2400, 4800, 1800, 9600, 19200, 0, 0, 0 };

        CoherentExtensionDUART68681* duartExtension;

        LogChannel duartChannel;
    };

}
