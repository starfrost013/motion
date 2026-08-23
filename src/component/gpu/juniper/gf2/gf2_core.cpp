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

        Logger::Log(LOG_PREFIX_GF2, "GF2 fitted: registers at 0x50002000, geometry pipe at segment 6.", LogChannels::Debug);
    }

    void GF2::Shutdown() { }

    /*
        FBCdata is both a command port and a spy port. Written under a debug mode it poses a
        question to the controller; read back under READOUTRUN it answers the last one. fbc_reset()
        asks two and refuses to continue unless both come back right.
    */
    uint16_t GF2::ReadFBCData()
    {
        if (fbcFlagsWritten != GF2_FBC_READOUTRUN)
            return 0;

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

    uint16_t GF2::Read16(size_t addr)
    {
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
                    Work here completes the instant it is submitted, so the bit reads clear from the
                    moment anything is written to the pipe until FBCclrint acknowledges it.

                    TOKEN_BIT_ is PIPEISBUSY, also active low; the pipe is never busy for us.
                */
                uint16_t flags = GF2_FBC_TOKEN_BIT_ | GF2_FBC_FBCACK_BIT | GF2_FBC_BPCACK_BIT;

                if (!fbcInterruptPending)
                    flags |= GF2_FBC_INTERRUPT_BIT_;

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
                // FBCclrint is "FBCpixel = 1" - acknowledge the controller's interrupt.
                fbcInterruptPending = false;
                break;

            case GF2_REG_FBCFLAGS:
                fbcFlagsWritten = value;
                break;

            case GF2_REG_GEFLAGS:
                geFlagsWritten = value;
                break;

            case GF2_REG_FBCDATA:
                // The microcode case was taken above; here it is the command port.
                fbcCommand = value;
                break;

            default:
                if (logEnabled)
                    Logger::Log(LOG_PREFIX_GF2, std::format("Unhandled GF2 Write16 of 0x{:x} to 0x{:x}", value, addr).c_str(), LogChannels::Warning);
                break;
        }
    }

    /*
        Everything the textport draws arrives here: _tx_drawcursor and friends write 32-bit command
        words to _GEPORT_ followed by 16-bit data. Nothing executes them yet, so record the stream -
        it is the specification for whatever decodes it next, and it is much easier to read off a
        real boot than to reconstruct from the driver.
    */
    void GF2::WriteGE(size_t addr, uint32_t value, int32_t width)
    {
        // Nothing executes the stream yet, but the controller still has to report the work done.
        fbcInterruptPending = true;

        if (!logEnabled || geWordsLogged >= GF2_MAX_GE_LOGGED)
            return;

        geWordsLogged++;

        Logger::Log(LOG_PREFIX_GF2, std::format("GE {}-bit write of 0x{:x} to {}{}", width, value,
            (addr == GF2_GE_PORT) ? "GEPORT" : ((addr == GF2_GE_TOKEN) ? "GETOKEN" : "0x"),
            (addr == GF2_GE_PORT || addr == GF2_GE_TOKEN) ? std::string() : std::format("{:x}", addr)).c_str(),
            LogChannels::Warning);

        if (geWordsLogged == GF2_MAX_GE_LOGGED)
            Logger::Log(LOG_PREFIX_GF2, "further geometry pipe writes will not be logged", LogChannels::Debug);
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
