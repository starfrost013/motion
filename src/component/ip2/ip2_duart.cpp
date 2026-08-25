/*
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    ip2_duart.cpp: Emulates two SCN68681 UARTs mapped at 32000000 and 32800000 respectively

    Largely adapted from the MAME emulation:
    https://github.com/mamedev/mame/blob/master/src/devices/machine/mc68681.h
    https://github.com/mamedev/mame/blob/master/src/devices/machine/mc68681.cpp
*/

#include <component/ip2/ip2_duart.hpp>

namespace Motion
{
    Cvar* logIP2DUART;

    uint8_t DUART68681::Read8(size_t addr)
    {
        // must be computed BEFORE addr is masked down to a register index - GetDuartIONum looks at a bit far
        // above the low nibble that selects the register.
        auto duartId = GetDuartIONum(addr);
        addr = addr & (DUART_NUM_REGS - 1);

        DUART& duart = duarts[duartId];

        // bit 4 is used for channel selection on channel regs
        int32_t channelId = 0;

        if (addr & 0x08)
            channelId = 1;

        UARTChannel& channel = duarts[duartId].channels[channelId];

        uint8_t ret = 0;
        uint8_t mrPtr = 0;

        bool mustUpdateFrame = false;           // true if state of the DUART data framing must be updated
        bool mustUpdateInterrupts = false;      // true if state of the interrupts must be updated

        switch (addr)
        {
            case DUART_MODE_A:
            case DUART_MODE_B:
                mrPtr = channel.modeRegCurrent;

                if (mrPtr == 0)
                {
                    ret = channel.mode1;
                    channel.modeRegCurrent++;
                }
                else
                    ret = channel.mode2;

                mustUpdateFrame = true;
                mustUpdateInterrupts = true;
                break;
            case DUART_READ_STATUS_A:
            case DUART_READ_STATUS_B:
                ret = (uint8_t)channel.status;
                break;
            // nonstandard bit rates
            case DUART_READ_BRG_TEST:
                duart.brgTest ^= 1;
                ret = duart.brgTest;
                break;
            // FIFO read
            case DUART_READ_RX_HOLD_A:
            case DUART_READ_RX_HOLD_B:
                if (!channel.rxFifoFree)
                {
                    Logger::Log(DUART_LOG_PREFIX, std::format("DUART{} RX FIFO {} underflow...", duartId, channelId).c_str(), LogChannels::Warning);
                    mustUpdateInterrupts = true;
                    break;
                }

                ret = channel.rxFifo[channel.rxFifoReadPtr];
                channel.rxFifoReadPtr = (channel.rxFifoReadPtr + 1) % DUART_FIFO_SIZE;

                // decrement available count
                channel.rxFifoFree--;

                // Handle the fifo flags
                channel.status &= ~(DUART_STATUS_FIFO_FULL);

                // handle no error - real hardware carries parity/framing/break status alongside each FIFO byte
                // (character mode) which this simplified FIFO doesn't track per-byte, so just clear once drained
                if (!(channel.mode1 & DUART_MODE_BLOCK_ERROR) && !channel.rxFifoFree)
                    channel.status &= ~(DUART_STATUS_RECEIVED_BREAK | DUART_STATUS_FRAMING_ERROR | DUART_STATUS_PARITY_ERROR);

                // can't receive anything if we have not parsed anything
                if (!channel.rxFifoFree)
                    channel.status &= ~(DUART_STATUS_RECEIVER_READY);

                mustUpdateInterrupts = true;

                break;
            case DUART_READ_INPUT_PORT_CHANGE:
                // IP0-3 are pulled up (idle high) since nothing external drives them in this emulation, so the
                // current-state bits always read high; only the delta bits reflect anything we actually latched.
                ret = (duart.ipcr & 0xF0) | 0x0F;
                duart.ipcr &= 0x0F;
                duart.isr &= ~DUART_INT_INPUT_PORT_CHANGE;
                break;
            case DUART_READ_INTERRUPT_STATUS:
                ret = duart.isr;
                break;
            case DUART_READ_COUNTER_UPPER:
                UpdateCounter(duartId);
                ret = (duart.counter >> 8) & 0xFF;
                break;
            case DUART_READ_COUNTER_LOWER:
                UpdateCounter(duartId);
                ret = duart.counter & 0xFF;
                break;
            case DUART_READ_1X16X:
                // test mode toggle, similar in spirit to the BRG test - not meaningfully emulated beyond this
                break;
            case DUART_READ_INPUT_PORTS:
                /*
                    D7 always reads 1; D6 reflects IACKN, which isn't modeled here (no interrupt
                    acknowledge cycles yet), so it's also left high; IP0-5 are pulled up with nothing
                    external attached.

                    Except the two carrier-detect inputs, which are **active low** - see the note in
                    the header. Both lines report carrier present, because in an emulator the terminal
                    on the other end is always plugged in and always powered on. Answering 0xFF here
                    meant "no carrier on every line", so du_open() blocked forever and the console
                    getty never printed `login:` - the machine reached run level 2 and then went
                    silent, which looked like a hang and was a modem control line.
                */
                ret = 0xFF & ~(DUART_IPORT_DCDA | DUART_IPORT_DCDB);
                break;
            case DUART_READ_START_COUNTER_CMD:
                // "the counter/timer is loaded with the value in CTUR/CTLR and begins counting down"
                duart.counter = duart.counterPreset;
                duart.counterStartNs = Chrono_GetTicksNS(Chrono_GetTime());
                duart.counterRunning = true;
                break;
            case DUART_READ_STOP_COUNTER_CMD:
                // Latch where the counter got to: the host then reads CTU/CTL to see how long something took.
                UpdateCounter(duartId);
                duart.counterRunning = false;
                duart.isr &= ~DUART_INT_COUNTER_READY;
                mustUpdateInterrupts = true;
                break;
            case DUART_INTERRUPT_VECTOR:
                ret = duart.ivr;
                break;
        }

        if (mustUpdateFrame)
            UpdateDataFrameState(duartId, channelId);

        if (mustUpdateInterrupts)
            UpdateInterruptState(duartId, channelId);

        if (logEnabled)
            Logger::Log(DUART_LOG_PREFIX, std::format("DUART{} read8 0x{:x} from addr 0x{:x}", duartId, ret, addr).c_str(), DUART_LOG_CHANNEL_NAME);

        return ret;
    }

    uint16_t DUART68681::Read16(size_t addr)
    {
        // big endian
        return ((uint16_t)Read8(addr) << 8) | (uint16_t)Read8(addr + 1);
    }

    uint32_t DUART68681::Read32(size_t addr)
    {
        // big endian
        return ((uint32_t)Read8(addr) << 24) | ((uint32_t)Read8(addr + 1) << 16) |
               ((uint32_t)Read8(addr + 2) << 8) | (uint32_t)Read8(addr + 3);
    }

    void DUART68681::Write8(size_t addr, uint8_t value)
    {
        // must be computed BEFORE addr is masked down to a register index - see Read8
        auto duartId = GetDuartIONum(addr);
        addr = addr & (DUART_NUM_REGS - 1);

        DUART& duart = duarts[duartId];
        uint8_t mrPtr;

        // bit 4 is used for channel selection on channel regs
        int32_t channelId = 0;

        if (addr & 0x08)
            channelId = 1;

        UARTChannel& channel = duarts[duartId].channels[channelId];

        bool mustUpdateFrame = false;           // true if state of the DUART data framing must be updated
        bool mustUpdateInterrupts = false;      // true if state of the interrupts must be updated

        switch (addr)
        {
            // mode reg
            case DUART_MODE_A:
            case DUART_MODE_B:
                mrPtr = channel.modeRegCurrent;

                if (mrPtr == 0)
                {
                    channel.mode1 = value;
                    channel.modeRegCurrent++;
                }
                else
                    channel.mode2 = value;

                mustUpdateFrame = true;
                mustUpdateInterrupts = true;
                break;
            case DUART_WRITE_CLOCKSEL_A:
            case DUART_WRITE_CLOCKSEL_B:
                channel.clocksel = value;
                RecomputeChannelClocks(duartId, channelId);
                break;
            case DUART_WRITE_COMMAND_A:
            case DUART_WRITE_COMMAND_B:
                // Command register - upper nibble is a single miscellaneous command
                // lower nybble is
                // independent enable/disable of the transmitter and receiver.
                switch ((value >> 4) & 0x0F)
                {
                    case DUART_COMMAND_NOP:
                        break;
                    case DUART_COMMAND_RESET_MR_PTR:
                        channel.modeRegCurrent = 0;
                        break;
                    case DUART_COMMAND_RESET_CHAN_RECEIVER:
                        channel.rxEnabled = false;
                        channel.status &= ~(DUART_STATUS_RECEIVER_READY | DUART_STATUS_RECEIVED_BREAK | DUART_STATUS_FRAMING_ERROR | DUART_STATUS_PARITY_ERROR);
                        // MAME guy was not sure of this and i ran out of screen space anyway
                        channel.status &= ~(DUART_STATUS_OVERRUN_ERROR);

                        channel.rxFifoReadPtr = channel.rxFifoWritePtr = channel.rxFifoFree = 0;
                        channel.rxShiftBusy = false;
                        channel.rxBitsReceived = 0;
                        break;
                    case DUART_COMMAND_RESET_CHAN_TRANSMITTER:
                        channel.txEnabled = false;
                        channel.txHoldFull = false;
                        channel.txShiftBusy = false;
                        channel.txBitsTransmitted = 0;
                        channel.status &= ~(DUART_STATUS_TRANSMITTER_READY | DUART_STATUS_TRANSMITTER_EMPTY);
                        break;
                    case DUART_COMMAND_RESET_ERROR_STATUS:
                        channel.status &= ~(DUART_STATUS_RECEIVED_BREAK | DUART_STATUS_FRAMING_ERROR | DUART_STATUS_PARITY_ERROR | DUART_STATUS_OVERRUN_ERROR);
                        break;
                    case DUART_COMMAND_RESET_CHANNEL_BRK_CHANGE:
                        duart.isr &= ~(channelId ? DUART_INT_DELTA_BREAK_B : DUART_INT_DELTA_BREAK_A);
                        break;
                    case DUART_COMMAND_START_TX_BREAK:
                        channel.txBreak = true;
                        break;
                    case DUART_COMMAND_STOP_TX_BREAK:
                        channel.txBreak = false;
                        break;
                }

                // Lower 4 bits: independent enable/disable commands for Tx and Rx. Per the datasheet,
                // performing a disable and enable at the same time results in disable.
                if (value & DUART_COMMAND_DISABLE_TX)
                {
                    channel.txEnabled = false;
                    channel.status &= ~(DUART_STATUS_TRANSMITTER_READY | DUART_STATUS_TRANSMITTER_EMPTY);
                }
                else if (value & DUART_COMMAND_ENABLE_TX)
                {
                    channel.txEnabled = true;
                    channel.status |= (DUART_STATUS_TRANSMITTER_READY | DUART_STATUS_TRANSMITTER_EMPTY);
                }

                if (value & DUART_COMMAND_DISABLE_RX)
                    channel.rxEnabled = false;
                else if (value & DUART_COMMAND_ENABLE_RX)
                {
                    channel.rxEnabled = true;
                    channel.rxShiftBusy = false;
                    channel.rxBitsReceived = 0;
                }

                mustUpdateInterrupts = true;
                break;
            case DUART_WRITE_TX_HOLD_A:
            case DUART_WRITE_TX_HOLD_B:
                if (!channel.txEnabled)
                {
                    Logger::Log(DUART_LOG_PREFIX, std::format("DUART{} channel {}: THR write while transmitter disabled, ignoring", duartId, channelId).c_str(), LogChannels::Warning);
                    break;
                }

                channel.txHold = value;
                channel.txHoldFull = true;
                channel.status &= ~(DUART_STATUS_TRANSMITTER_READY | DUART_STATUS_TRANSMITTER_EMPTY);

                // if the shift register is idle, the new character can start moving out immediately - this
                // frees up the holding register again straight away (double buffering)
                if (!channel.txShiftBusy)
                {
                    channel.txData = channel.txHold;
                    channel.txHoldFull = false;
                    channel.txShiftBusy = true;
                    channel.txBitsTransmitted = 0;
                    channel.status |= DUART_STATUS_TRANSMITTER_READY;
                }

                mustUpdateInterrupts = true;
                break;
            case DUART_WRITE_AUX_CONTROL:
                duart.auxControl = value;
                // ACR[7] affects the baud rate set used by both channels on this chip
                RecomputeChannelClocks(duartId, 0);
                RecomputeChannelClocks(duartId, 1);
                break;
            case DUART_WRITE_INTERRUPT_MASK:
                duart.imr = value;
                mustUpdateInterrupts = true;
                break;
            case DUART_WRITE_COUNTER_UPPER:
                duart.counterPreset = (duart.counterPreset & 0x00FF) | ((uint16_t)value << 8);
                break;
            case DUART_WRITE_COUNTER_LOWER:
                duart.counterPreset = (duart.counterPreset & 0xFF00) | value;
                break;
            case DUART_WRITE_OUTPUT_PORT_CONF:
                duart.opcr = value;
                break;
            case DUART_WRITE_SET_OUTPUT_PORT_BITS_CMD:
                duart.opr |= value;
                break;
            case DUART_WRITE_RESET_OUTPUT_PORT_BITS_CMD:
                duart.opr &= (uint8_t)~value;
                break;
            case DUART_INTERRUPT_VECTOR:
                duart.ivr = value;
                break;
        }

        if (mustUpdateFrame)
            UpdateDataFrameState(duartId, channelId);

        if (mustUpdateInterrupts)
            UpdateInterruptState(duartId, channelId);

        if (logEnabled)
            Logger::Log(DUART_LOG_PREFIX, std::format("DUART{} write8 0x{:x} to addr 0x{:x}", duartId, value, addr).c_str(), DUART_LOG_CHANNEL_NAME);
    }

    void DUART68681::Write16(size_t addr, uint16_t value)
    {
        // big endian
        Write8(addr, (value) & 0xFF00);
        Write8(addr + 1, (value) & 0x00FF);
    }

    void DUART68681::Write32(size_t addr, uint32_t value)
    {
        // big endian
        Write8((addr), (value) & 0xFF000000);
        Write8((addr + 1), (value) & 0x00FF0000);
        Write8((addr + 2), (value) & 0x0000FF00);
        Write8((addr + 3), (value) & 0x000000FF);
    }

    void DUART68681::SetBaudRate(int32_t duart, int32_t channelId, bool isRx, uint8_t data)
    {
        UARTChannel& channel = duarts[duart].channels[channelId];

        int32_t baudRate = baudRateACR0[data & 0x0F];

        // 7th bit of acr switches baud rate
        if ((duarts[duart].auxControl) & 0x80) //bit0
            baudRate = baudRateACR1[data & 0x0F];

        // channel a
        if (!channelId)
        {
            // external from ip3, divide by 16
            if ((data & 0x0F) == 0x0E)
                baudRate = ip3clk >> 4;
            else if ((data & 0x0F) == 0x0F) // 0x0f
                baudRate = ip3clk;
        }
        else // channelb
        {
            // external from ip5, divide by 16
            if ((data & 0x0F) == 0x0E)
                baudRate = ip5clk >> 4;
            else if ((data & 0x0F) == 0x0F) // 0x0f
                baudRate = ip5clk;
        }

        if ((!baudRate) && ((data & 0xF) != 0xD))
            Logger::Log(DUART_LOG_PREFIX, std::format("Invalid DUART{} channel {} transmit clock configuration {}", duart, channelId, data).c_str(), LogChannels::Warning);

        if (!isRx)
        {
            if (logEnabled)
                Logger::Log(DUART_LOG_PREFIX, std::format("DUART{} channel {}: Transmit baud rate is now {}", duart, channelId, baudRate).c_str(), DUART_LOG_CHANNEL_NAME);
            
            channel.baudRateTX = baudRate;
        }
        else
        {
            if (logEnabled)
                Logger::Log(DUART_LOG_PREFIX, std::format("DUART{} channel {}: Receive baud rate is now {}", duart, channelId, baudRate).c_str(), DUART_LOG_CHANNEL_NAME);
            
            channel.baudRateRX = baudRate;
        }
    }

    void DUART68681::RecomputeChannelClocks(int32_t duartId, int32_t channelId)
    {
        UARTChannel& channel = duarts[duartId].channels[channelId];

        SetBaudRate(duartId, channelId, false, channel.clocksel & 0x0F);
        SetBaudRate(duartId, channelId, true, (channel.clocksel >> 4) & 0x0F);

        SetRxClock(duartId, channelId, channel.baudRateRX);
        SetTxClock(duartId, channelId, channel.baudRateTX);
    }

    void DUART68681::UpdateDataFrameState(int32_t duartId, int32_t channelId)
    {
        UARTChannel& channel = duarts[duartId].channels[channelId];

        // MR1[1:0] - bits per character (00=5 01=6 10=7 11=8)
        static const uint8_t bitsPerCharTable[4] = { 5, 6, 7, 8 };
        channel.dataBits = bitsPerCharTable[channel.mode1 & DUART_MODE_BITS_PER_CHAR];

        // MR1[4:3] - parity mode
        uint8_t parityMode = (channel.mode1 & DUART_MODE_PARITY_MODE) >> 3;
        channel.parityEnabled = (parityMode == DUART_MODE_PARITY_MODE_WITH || parityMode == DUART_MODE_PARITY_MODE_FORCE);

        if (parityMode == DUART_MODE_PARITY_MODE_MULTIDROP)
            Logger::Log(DUART_LOG_PREFIX, std::format("DUART{} channel {}: multidrop (wake-up) mode selected, not fully emulated", duartId, channelId).c_str(), LogChannels::Warning);

        // MR2[3:0] - stop bit length code, ranges 0.563 to 2.000 bits in 1/16 increments. Rounded to a whole
        // number of bit-times (1 or 2) since characters are moved as whole bytes rather than bit-by-bit here.
        uint8_t stopBitCode = channel.mode2 & 0x0F;
        channel.stopBitPeriods = (stopBitCode >= 0x08) ? 2 : 1;
    }

    uint32_t DUART68681::GetFrameBits(UARTChannel& channel)
    {
        // 1 start bit + data bits + optional parity bit + stop bit period(s)
        return 1 + channel.dataBits + (channel.parityEnabled ? 1 : 0) + channel.stopBitPeriods;
    }

    void DUART68681::UpdateInterruptState(int32_t duartId, int32_t channelId)
    {
        // ISR/IMR/IVR are chip-level (shared by both channels), so this recomputes the whole register.
        DUART& duart = duarts[duartId];

        UARTChannel& chanA = duart.channels[0];
        UARTChannel& chanB = duart.channels[1];

        uint8_t isr = duart.isr & (DUART_INT_INPUT_PORT_CHANGE | DUART_INT_DELTA_BREAK_A
            | DUART_INT_DELTA_BREAK_B | DUART_INT_COUNTER_READY);

        if (chanA.status & DUART_STATUS_TRANSMITTER_READY)
            isr |= DUART_INT_TXRDYA;

        bool rxIntSelectA = (chanA.mode1 & DUART_MODE_RX_INT_SELECT_BIT);
        if (rxIntSelectA ? (chanA.status & DUART_STATUS_FIFO_FULL) : (chanA.status & DUART_STATUS_RECEIVER_READY))
            isr |= DUART_INT_RXRDY_FFULLA;

        if (chanB.status & DUART_STATUS_TRANSMITTER_READY)
            isr |= DUART_INT_TXRDYB;

        bool rxIntSelectB = (chanB.mode1 & DUART_MODE_RX_INT_SELECT_BIT);
        if (rxIntSelectB ? (chanB.status & DUART_STATUS_FIFO_FULL) : (chanB.status & DUART_STATUS_RECEIVER_READY))
            isr |= DUART_INT_RXRDY_FFULLB;

        duart.isr = isr;

        /*
            INTRN is asserted whenever an unmasked source is pending. Both chips sit on interrupt
            level 6; which of them is asserting is what picks the vector, and DUART 1 is the one the
            kernel installs its Xclock handler for.
        */
        if (!interrupts)
            interrupts = Emulation::GetMachine()->FindComponentByType<IP2Interrupt>();

        if (interrupts)
            interrupts->SetLocalInterrupt(duartId ? IP2_LOCAL_DUART1 : IP2_LOCAL_DUART0,
                (duart.isr & duart.imr) != 0);
    }

    //
    // TICK method + CLOCK
    //

    uint64_t DUART68681::GetCounterTickNs(int32_t duartId)
    {
        // ACR[6:4]: 011 counter X1/16, 110 timer X1, 111 timer X1/16. The rest select pins nothing is connected to.
        switch (DUART_COUNTER_MODE(duarts[duartId].auxControl))
        {
            case 3:
            case 7:
                return (1000000000ull * 16) / DUART_X1_HZ;
            case 6:
                return 1000000000ull / DUART_X1_HZ;
            default:
                return 0;
        }
    }

    /*
        Rather than decrementing once per tick - which at 230kHz would mean either a very hot Tick or
        a counter that lies - work out where the counter must have got to from how long it has been
        since it was loaded. Reads latch the value first, so the host always sees an exact count.
    */
    void DUART68681::UpdateCounter(int32_t duartId)
    {
        DUART& duart = duarts[duartId];
        uint64_t tickNs = GetCounterTickNs(duartId);

        if (!tickNs)
            return;

        bool isTimer = DUART_COUNTER_MODE_IS_TIMER(duart.auxControl);

        // A timer free runs; a counter only runs between the start and stop commands.
        if (!isTimer && !duart.counterRunning)
            return;

        uint64_t now = Chrono_GetTicksNS(Chrono_GetTime());

        if (now < duart.counterStartNs)
            return;

        uint64_t elapsed = (now - duart.counterStartNs) / tickNs;

        // The counter rolls over rather than stopping at zero, so this is deliberately allowed to wrap.
        duart.counter = (uint16_t)(duart.counterPreset - elapsed);

        /*
            "Counter ready" is set the first time the count passes zero and stays set until the stop
            counter command clears it. In timer mode there is no stop, so the bit is set once a half
            period has gone by and the host clears it the same way.
        */
        if (elapsed >= duart.counterPreset)
            duart.isr |= DUART_INT_COUNTER_READY;
    }

    void DUART68681::Tick()
    {
        /*
            Keep the counter/timer current. This is what generates the scheduler clock, so a change
            here has to reach the interrupt logic rather than just sitting in ISR - run the interrupt
            recompute whenever the counter ready bit moves.
        */
        for (int32_t duart = 0; duart < 2; duart++)
        {
            uint8_t before = duarts[duart].isr;

            UpdateCounter(duart);

            if (duarts[duart].isr != before)
                UpdateInterruptState(duart, 0);   // chip level recompute, the channel argument is unused here
        }

        // Each of the 4 channels (2 chips x 2 channels) has its own independent baud rate, so each gets its own
        // pair of rx/tx bit clocks rather than one global clock for the whole component.
        auto ns = Chrono_GetTicksNS(Chrono_GetTime());

        for (int32_t duart = 0; duart < 2; duart++)
        {
            for (int32_t c = 0; c < DUART_NUM_CHANNELS; c++)
            {
                UARTChannel& channel = duarts[duart].channels[c];

                if (channel.rxClkNs && ((!channel.lastRxClkNs) 
                || (ns - channel.lastRxClkNs) > channel.rxClkNs))
                {
                    channel.lastRxClkNs = ns;
                    OnRxClock(duart, c);
                }

                if (channel.txClkNs && ((!channel.lastTxClkNs) 
                || (ns - channel.lastTxClkNs) > channel.txClkNs))
                {
                    channel.lastTxClkNs = ns;
                    OnTxClock(duart, c);
                }
            }
        }
    }

    void DUART68681::OnRxClock(int32_t duartId, int32_t channelId)
    {
        UARTChannel& channel = duarts[duartId].channels[channelId];

        if (!channel.rxEnabled)
            return;

        // no character in progress - see if the serial line has one waiting for us
        if (!channel.rxShiftBusy)
        {
            uint8_t data = 0;

            if (!GetLine(GetLineIndex(duartId, channelId)).TryReceiveByte(data))
                return;

            channel.rxData = data;
            channel.rxShiftBusy = true;
            channel.rxBitsReceived = 0;
            return;
        }

        // pace the arrival of the character over roughly the right number of bit transmission times
        channel.rxBitsReceived++;

        if (channel.rxBitsReceived < GetFrameBits(channel))
            return;

        channel.rxShiftBusy = false;
        channel.rxBitsReceived = 0;

        if (channel.rxFifoFree >= DUART_FIFO_SIZE)
        {
            channel.status |= DUART_STATUS_OVERRUN_ERROR;
            Logger::Log(DUART_LOG_PREFIX, std::format("DUART{} RX FIFO {} overrun!", duartId, channelId).c_str(), LogChannels::Warning);
            UpdateInterruptState(duartId, channelId);
            return;
        }

        channel.rxFifo[channel.rxFifoWritePtr] = channel.rxData;
        channel.rxFifoWritePtr++;
        channel.rxFifoWritePtr %= DUART_FIFO_SIZE;
        channel.rxFifoFree++;

        channel.status |= DUART_STATUS_RECEIVER_READY;

        if (channel.rxFifoFree == DUART_FIFO_SIZE)
            channel.status |= DUART_STATUS_FIFO_FULL;

        UpdateInterruptState(duartId, channelId);
    }

    void DUART68681::OnTxClock(int32_t duartId, int32_t channelId)
    {
        UARTChannel& channel = duarts[duartId].channels[channelId];

        if (!channel.txEnabled)
            return;

        // TxD held low for the whole character - nothing to actually put on the (byte-level) wire
        if (channel.txBreak)
            return;

        if (!channel.txShiftBusy)
            return;

        channel.txBitsTransmitted++;

        if (channel.txBitsTransmitted < GetFrameBits(channel))
            return;

        channel.txBitsTransmitted = 0;

        GetLine(GetLineIndex(duartId, channelId)).SendByte(channel.txData);

        if (channel.txHoldFull)
        {
            // another character was already waiting in the holding register - keep the shift register busy
            channel.txData = channel.txHold;
            channel.txHoldFull = false;
        }
        else
        {
            channel.txShiftBusy = false;
            channel.status |= DUART_STATUS_TRANSMITTER_EMPTY;
        }

        // reassert TxRDU
        channel.status |= DUART_STATUS_TRANSMITTER_READY;

        UpdateInterruptState(duartId, channelId);
    }

    void DUART68681::SetRxClock(int32_t duartId, int32_t channelId, uint32_t hz)
    {
        UARTChannel& channel = duarts[duartId].channels[channelId];

        if (!hz)
        {
            channel.rxClkNs = 0;
            return;
        }

        double intermediate = 1.0 / hz * 1000000000.0;
        channel.rxClkNs = (uint64_t)intermediate;
    }

    void DUART68681::SetTxClock(int32_t duartId, int32_t channelId, uint32_t hz)
    {
        UARTChannel& channel = duarts[duartId].channels[channelId];

        if (!hz)
        {
            channel.txClkNs = 0;
            return;
        }

        double intermediate = 1.0 / hz * 1000000000.0;
        channel.txClkNs = (uint64_t)intermediate;
    }
}
