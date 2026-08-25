/* motion - The SGI Emulator. Copyright (c)2026 danifunker. gf2_core.cpp: GF2 frame buffer controller and geometry pipe front end. */

#include <component/gpu/juniper/gf2/gf2.hpp>

namespace Motion
{
    Cvar* logGF2;
    Cvar* enableGF2;

    void GF2::Start()
    {
        enableGF2 = Cvar::Get("enableGF2", "0");

        gf2Channel = LogChannel(GF2_LOG_CHANNEL_NAME, ConsoleColor::BrightMagenta, ConsoleColor::White);
        Logger::AddChannel(gf2Channel);
        logGF2 = Cvar::Get("logGF2", "0");
        logEnabled = logGF2->GetValue();

        retraceHz = (uint32_t)Cvar::Get("gf2RetraceHz", GF2_RETRACE_HZ_DEFAULT)->GetValue();
        cursorTrace = Cvar::Get("logMouse", "0")->GetValue();

        if (logEnabled)
            Logger::SetChannelEnabled(GF2_LOG_CHANNEL_NAME);

        // Off by default while the geometry pipe is still a stub.
        if (!enableGF2->GetValue())
        {
            Logger::Log(LOG_PREFIX_GF2, "GF2 is disabled, the console will stay on the serial line. +set enableGF2 1 to fit the board.", LogChannels::Debug);
            return;
        }

        multibus = Emulation::GetMachine()->FindComponentByType<Multibus>();

        // The frame buffer the FBC draws into, and the only thing that knows how many planes exist.
        vram = Emulation::GetMachine()->FindComponentByType<BP3>();

        Multibus::SlotMapping slot = Multibus::SlotMapping(this);

        slot.ioStart = GF2_REG_START;
        slot.ioEnd = GF2_REG_END;
        slot.id = GF2_MULTIBUS_SLOT;

        multibus->AddSlotMapping(slot);

        // The geometry pipe is not on the backplane - segment 6 comes straight off the CPU.
        AddrSpaceMapping mapping = AddrSpaceMapping();

        mapping.startAddr = GF2_GE_SEGMENT_START;
        mapping.endAddr = GF2_GE_SEGMENT_END;
        mapping.component = this;

        AddrSpace::AddMapping(mapping);

        Logger::Log(LOG_PREFIX_GF2, std::format("GF2 fitted: registers at 0x50002000, geometry pipe at segment 6, "
            "retrace at {}Hz on Multibus IRQ {}.", retraceHz, GF2_MULTIBUS_IRQ).c_str(), LogChannels::Debug);
    }

    void GF2::Shutdown() { }

    // The vertical retrace, which is the only thing on this board that happens without the host asking for it.
    void GF2::Tick()
    {
        if (!multibus || !retraceHz)
            return;

        uint64_t now = Chrono_GetTicksNS(Chrono_GetTime());
        uint64_t periodNs = 1000000000ull / retraceHz;

        if (!lastRetraceNs)
        {
            lastRetraceNs = now;
            return;
        }

        if ((now - lastRetraceNs) < periodNs)
            return;

        lastRetraceNs = now;
        verticalPending = true;

        UpdateInterrupt();
    }

    // Drive the board's Multibus line.
    void GF2::UpdateInterrupt()
    {
        bool vertical = verticalPending && !(geFlagsWritten & GF2_GE_ENABVERTINT_BIT_);
        bool programmed = fbcInterruptPending && !(geFlagsWritten & GF2_GE_ENABFBCINT_BIT_);
        bool asserted = (vertical || programmed);

        if (asserted == irqAsserted)
            return;

        irqAsserted = asserted;
        multibus->SetMultibusIRQ(GF2_MULTIBUS_IRQ, asserted);
    }

    // Answer a HOSTFLAG. The host moves the cursor without stopping the pipe by writing the cursor's x to FBCdata, raising HOSTFLAG and waiting for.
    void GF2::ServiceCursorRequest()
    {
        if (!cursorRequested || fbcInterruptPending)
        {
            UpdateInterrupt();
            return;
        }

        cursorRequested = false;

        // No readback: fbc_progintr answers this one by writing, and dismisses it with FBCclrint.
        RaiseProgrammedInterrupt(GF2_FBC_INT_CURSOR, nullptr, 0);
    }

    // FBCdata is both a command port and a spy port.
    uint16_t GF2::ReadFBCData()
    {
        if (fbcFlagsWritten == GF2_FBC_READOUTRUN)
        {
            // READOUTRUN spies on the controller's output register.
            if (fbcInterruptPending)
                return fbcInterruptCode;

            switch (fbcCommand)
            {
                case GF2_FBC_CMD_SCRATCH_SIZE:
                    return GF2_FBC_SCRATCH_SIZE;
                case GF2_FBC_CMD_MICRO_VERSION:
                    return GF2_FBC_MICRO_VERSION;
                default:
                    return 0;
            }
        }

        // In any running mode FBCdata reads the head of the readback FIFO. AUTOCLEAR in GEflags means "clear FBC int after rd", so the host can drain a long.
        if (readbackCount <= 0)
            return 0;

        if (geFlagsWritten & GF2_GE_AUTOCLEAR_BIT)
            return PopReadback();

        return readback[readbackHead];
    }

    uint16_t GF2::Read16(size_t addr)
    {
        // The geometry pipe port is write only - it is the tail of a FIFO, and addrs.h says so.
        if (addr >= GF2_GE_SEGMENT_START)
            return 0;

        // The microcode window starts *at* FBCdata rather than after it - FBCmicrocode(State) is FBCucode((State & 0x1ff) << 1), so state 0 of every 512 state.
        if (addr >= GF2_REG_FBCDATA && addr < GF2_REG_GEFLAGS
        && fbcFlagsWritten == GF2_FBC_READMICRO && MicroAccessEnabled())
        {
            uint16_t word = microcode[MicroState(addr)][MicroSlice()];

            // The top slice is only eight bits wide, and Micro_Write masks its readback to match.
            return (MicroSlice() == 3) ? (word & GF2_MICRO_SLICE3_MASK) : word;
        }

        if (addr > GF2_REG_FBCDATA && addr < GF2_REG_GEFLAGS)
            return 0;

        switch (addr)
        {
            case GF2_REG_PIXEL:
                // Pixel readback. Nothing has been drawn, so every pixel is background.
                return 0;

            case GF2_REG_FBCFLAGS:
            {
                // INTERRUPT_BIT_ is the controller saying it has finished what it was asked to do, and like the rest of the read side it is active low.
                uint16_t flags = GF2_FBC_FBCACK_BIT | GF2_FBC_BPCACK_BIT;

                if (!fbcInterruptPending)
                    flags |= GF2_FBC_INTERRUPT_BIT_;

                // NEWVERT_BIT_ is active low too, and it is the first thing fbc_intr looks at: a clear bit sends it down the whole retrace path - colour map updates.
                if (!verticalPending)
                    flags |= GF2_FBC_NEWVERT_BIT_;

                return flags;
            }

            case GF2_REG_FBCDATA:
                return ReadFBCData();

            case GF2_REG_GEFLAGS:
                // fbc_reset() does "if (GEflags) { can't reset GE pipe }", so this has to read as a clean zero once the pipe is reset.
                return 0;

            default:
                if (logEnabled)
                    Logger::Log(LOG_PREFIX_GF2, std::format("Unhandled GF2 Read16 from 0x{:x}", addr).c_str(), LogChannels::Warning);
                return 0;
        }
    }

    void GF2::Write16(size_t addr, uint16_t value)
    {
        if (addr >= GF2_GE_SEGMENT_START)
        {
            WriteGE(addr, value, 16);
            return;
        }

        // Microcode window, as above - inclusive of FBCdata itself.
        if (addr >= GF2_REG_FBCDATA && addr < GF2_REG_GEFLAGS
        && fbcFlagsWritten == GF2_FBC_WRITEMICRO && MicroAccessEnabled())
        {
            microcode[MicroState(addr)][MicroSlice()] = value;
            return;
        }

        if (addr > GF2_REG_FBCDATA && addr < GF2_REG_GEFLAGS)
            return;

        switch (addr)
        {
            case GF2_REG_PIXEL:
                // FBCclrint is "FBCpixel = 1".
                PopReadback();
                break;

            case GF2_REG_FBCFLAGS:
            {
                fbcFlagsWritten = value;

                // READOUTRUN is a spy mode rather than a state: fbc_progintr flips into it to read the interrupt code out of the output register and straight back out.
                if (value == GF2_FBC_READOUTRUN)
                    break;

                bool hostFlag = (value & GF2_FBC_HOSTFLAG) != 0;

                // Only the transition is a request; dropping the flag cancels one not yet raised.
                if (hostFlag && !hostFlagSet)
                    cursorRequested = true;
                else if (!hostFlag)
                    cursorRequested = false;

                hostFlagSet = hostFlag;

                ServiceCursorRequest();
                break;
            }

            case GF2_REG_GEFLAGS:
                geFlagsWritten = value;

                // Disabling the vertical interrupt is also how it is dismissed - fbc_intr opens with a Disabvert/Enabvert pair whose only purpose is "reset vertical.
                if (value & GF2_GE_ENABVERTINT_BIT_)
                    verticalPending = false;

                UpdateInterrupt();
                break;

            case GF2_REG_FBCDATA:
                // The microcode case was taken above; here it is the command port - except during the cursor handshake, where the same register carries a coordinate.
                fbcCommand = value;

                if (fbcInterruptPending && fbcInterruptCode == GF2_FBC_INT_CURSOR)
                    MoveCursor(cursorPendingX, (int16_t)value);
                else
                    cursorPendingX = (int16_t)value;

                break;

            default:
                if (logEnabled)
                    Logger::Log(LOG_PREFIX_GF2, std::format("Unhandled GF2 Write16 of 0x{:x} to 0x{:x}", value, addr).c_str(), LogChannels::Warning);
                break;
        }
    }

    // The register block is a 16-bit port; the byte and long paths just fall through to it.
    uint8_t GF2::Read8(size_t addr) { return (uint8_t)Read16(addr & ~1); }
    uint32_t GF2::Read32(size_t addr) { return ((uint32_t)Read16(addr) << 16) | Read16(addr + 2); }

    void GF2::Write8(size_t addr, uint8_t value)
    {
        if (addr >= GF2_GE_SEGMENT_START)
        {
            WriteGE(addr, value, 8);
            return;
        }

        Write16(addr & ~1, value);
    }

    void GF2::Write32(size_t addr, uint32_t value)
    {
        if (addr >= GF2_GE_SEGMENT_START)
        {
            WriteGE(addr, value, 32);
            return;
        }

        Write16(addr, (uint16_t)(value >> 16));
        Write16(addr + 2, (uint16_t)value);
    }
};
