/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    gf2_core.cpp: GF2 frame buffer controller and geometry pipe front end.

    The behaviour here is taken from the IRIX 3.7 kernel sources rather than from another emulator:
    fbcreset() in sys/gl1/fbc.c and gereset()/gefind() in sys/gl1/ge.c are the only two routines that
    have to be satisfied before IRIX will call the graphics console usable, and both of them are
    handshakes with exit conditions that a plausible-looking constant does not meet.
*/

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

        /*
            Off by default while the geometry pipe is still a stub. con_init() only switches the
            console to the screen if resetting the graphics succeeds, so a half finished board is
            worse than no board at all: with nothing here the reset bus errors, the nofault longjmp
            fires and the console falls back to the serial line, which is a working machine.
        */
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

    /*
        The vertical retrace, which is the only thing on this board that happens without the host
        asking for it. It matters far beyond drawing: fbc_intr services the *programmed* interrupt at
        its tail as well, so on a real machine every FBC result is picked up at the next retrace even
        if nothing else prompts one. That is why a board that never retraces looks like a board whose
        FBC never finishes anything - gl_WaitForEOF spins on a count only fbc_progintr decrements.

        Wall clock rather than emulated cycles, the same as the RTC's periodic interrupt, and one
        field behind at most: the guest runs slower than the host, and catching up field for field
        after a slow patch would hand retrace_softintr a burst it counts as elapsed time.
    */
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

    /*
        Drive the board's Multibus line. Both sources are gated at the board by their own enable in
        GEflags, both active low, and the difference between the enable and the status bit is load
        bearing: gl_getplaneinfo disables the FBC interrupt and then polls INTERRUPT_BIT_ itself, so
        "is the controller finished" and "is it allowed to say so out loud" are separate questions.
    */
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

    /*
        Answer a HOSTFLAG. The host moves the cursor without stopping the pipe by writing the cursor's
        x to FBCdata, raising HOSTFLAG and waiting for _INTCURSOR; fbc_progintr then hands over the y,
        dismisses the interrupt and drops the flag again. saveeverything() spins on its own copy of
        that flag, which is only cleared on the far side of the handshake, so a HOSTFLAG that is never
        answered is a hang rather than a missing cursor.

        A programmed interrupt already up owns the readback FIFO, so the cursor waits for it - which
        is what fbc_intr means by "there may be a programmed interrupt in progress, so we must service
        until we find the cursor interrupt". PopReadback comes back here once the FIFO drains.
    */
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

    /*
        FBCdata is both a command port and a spy port. Written under a debug mode it poses a
        question to the controller; read back under READOUTRUN it answers the last one. fbc_reset()
        asks two and refuses to continue unless both come back right.
    */
    uint16_t GF2::ReadFBCData()
    {
        if (fbcFlagsWritten == GF2_FBC_READOUTRUN)
        {
            /*
                READOUTRUN spies on the controller's output register. While a programmed interrupt is
                up that register holds the interrupt code, which is how the host finds out what kind
                of answer is waiting - gl_getplaneinfo panics outright if it is not _INTPIXEL32, and
                gr.c does the same for _INTFEEDBACK. With nothing outstanding it is still the debug
                answer port that fbc_reset questions.
            */
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

        /*
            In any running mode FBCdata reads the head of the readback FIFO. AUTOCLEAR in GEflags
            means "clear FBC int after rd", so the host can drain a long result with a plain read
            loop instead of a read and an FBCclrint per word - gr_savestate does exactly that.
        */
        if (readbackCount <= 0)
            return 0;

        if (geFlagsWritten & GF2_GE_AUTOCLEAR_BIT)
            return PopReadback();

        return readback[readbackHead];
    }

    uint16_t GF2::Read16(size_t addr)
    {
        /*
            The geometry pipe port is write only - it is the tail of a FIFO, and addrs.h says so. A
            read of it is not the guest drawing, it is the debugger looking at the address space, so
            it answers zero rather than warning about an access the machine never really made.
        */
        if (addr >= GF2_GE_SEGMENT_START)
            return 0;

        /*
            The microcode window starts *at* FBCdata rather than after it - FBCmicrocode(State) is
            FBCucode((State & 0x1ff) << 1), so state 0 of every 512 state block addresses 0x2800
            exactly. Which meaning applies is decided by the mode, not by the address.
        */
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
                /*
                    INTERRUPT_BIT_ is the controller saying it has finished what it was asked to do,
                    and like the rest of the read side it is active low. gl_getplaneinfo pushes a
                    handful of commands down the pipe and then sits in

                        move.w $50002400,d0 ; andi.l #$10,d0 ; bne <back>

                    waiting for it to go clear, so reporting it permanently set wedges the boot.
                    It is raised only when the microcode actually has a result waiting - a readback,
                    a feedback buffer, a hit - and drops again once the host has emptied the readback
                    FIFO. Asserting it for any submitted work instead would be simpler and wrong: the
                    host would read the code back and find nothing that explains the interrupt.

                    TOKEN_BIT_ has to read *clear*. The trailing underscore says the signal is
                    active low, but gf2.h defines PIPEISBUSY as (FBCflags & TOKEN_BIT_) with no
                    inversion, so a set bit means the pipe is busy. Commands here complete as they
                    are submitted, so it is never busy - and reporting otherwise is not harmless:
                    tx_repaint() gives up and reschedules whenever PIPEISBUSY, so the textport
                    silently never draws and the console appears dead while the machine runs on.
                */
                uint16_t flags = GF2_FBC_FBCACK_BIT | GF2_FBC_BPCACK_BIT;

                if (!fbcInterruptPending)
                    flags |= GF2_FBC_INTERRUPT_BIT_;

                /*
                    NEWVERT_BIT_ is active low too, and it is the first thing fbc_intr looks at: a
                    clear bit sends it down the whole retrace path - colour map updates, blink
                    events, the mouse, the cursor - and a set one straight to fbc_progintr. It is a
                    latch, set again only when the host writes GEflags with ENABVERTINT_BIT_ set.
                */
                if (!verticalPending)
                    flags |= GF2_FBC_NEWVERT_BIT_;

                return flags;
            }

            case GF2_REG_FBCDATA:
                return ReadFBCData();

            case GF2_REG_GEFLAGS:
                /*
                    fbc_reset() does "if (GEflags) { can't reset GE pipe }", so this has to read as a
                    clean zero once the pipe is reset. That also keeps HIWATER_BIT clear, which is
                    what gewait() spins on.
                */
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
                /*
                    FBCclrint is "FBCpixel = 1". It takes one word off the readback FIFO rather than
                    clearing the interrupt outright: the host pops its way through a result and the
                    interrupt drops when there is nothing left. gl_getplaneinfo relies on this - it
                    pops the command and the count, reads the two halves of the pixel, and pops after
                    each one.
                */
                PopReadback();
                break;

            case GF2_REG_FBCFLAGS:
            {
                fbcFlagsWritten = value;

                /*
                    READOUTRUN is a spy mode rather than a state: fbc_progintr flips into it to read
                    the interrupt code out of the output register and straight back out again, and
                    both of those writes carry a HOSTFLAG the host is not touching. Counting either
                    as an edge raises a second cursor interrupt for every real one - which is exactly
                    what the log shows, two per retrace instead of one, and the pipe filling with
                    GEnoops that only exist to force the FBC to dispatch again.
                */
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

                /*
                    Disabling the vertical interrupt is also how it is dismissed - fbc_intr opens
                    with a Disabvert/Enabvert pair whose only purpose is "reset vertical interrupt",
                    and nothing else in the driver disables it. Do this before updating the line, so
                    a handler that dismisses and re-enables in two writes never sees it reasserted.
                */
                if (value & GF2_GE_ENABVERTINT_BIT_)
                    verticalPending = false;

                UpdateInterrupt();
                break;

            case GF2_REG_FBCDATA:
                /*
                    The microcode case was taken above; here it is the command port - except during
                    the cursor handshake, where the same register carries a coordinate. fbc_intr
                    writes the cursor's x and then raises HOSTFLAG; fbc_progintr answers the
                    interrupt by writing the y. So a write while a cursor interrupt is outstanding is
                    the y that completes a move, and any other write is either a debug question or
                    the x that is about to be used by one.
                */
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
