/*
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    storager2.cpp: The Interphase Storager 2 Multibus ESDI disk controller - SGI's "sii".

    The whole of this file works in MULTIBUS byte offsets. The IP2 crosses the byte lanes, so Multibus
    byte N is the byte the 68020 wrote at N ^ 1; Read8 and Write8 uncross that once on the way in and
    MBRead8/MBWrite8 do the same for the memory the board DMAs through. Every offset named in
    storager2.hpp is therefore the offset the controller's own firmware would use, not the one the
    driver writes - which is why R0 is at 0x1F8 here and ST_R0 is 0x1F9 in siireg.h.

    Commands are executed synchronously, inside the store that starts them. A real board takes a
    revolution or two; nothing in the driver depends on that, and pretending to be instant removes a
    whole class of races that would otherwise need a scheduler.
*/

#include <component/storage/storager2.hpp>

namespace Motion
{
    Cvar* logStorager;

    void Storager2::Start()
    {
        logStorager = Cvar::Get("logStorager", "0");
        logEnabled = (logStorager->GetValue() != 0);

        if (Profile::GetDiskController() != DiskControllerType::Storager)
        {
            Logger::Log(STORAGER2_LOG_PREFIX, "Storager 2 is not the fitted disk controller. "
                "+set diskController storager fits it.");
            return;
        }

        multibus = Emulation::GetMachine()->FindComponentByType<Multibus>();

        Multibus::SlotMapping slot = Multibus::SlotMapping(this);

        slot.ioStart = STORAGER2_MBIO_START;
        slot.ioEnd = STORAGER2_MBIO_END;
        slot.id = STORAGER2_MULTIBUS_SLOTNUM;

        multibus->AddSlotMapping(slot);

        /*
            si0 and si1, the machine's two physical drives - profileDisk0Path and profileDisk1Path.
            The controller also carries a floppy on unit 2 and a QIC tape as siq0, neither of which is
            emulated, so those units stay empty and report not installed.
        */
        for (int32_t i = 0; i < STORAGER2_MAX_WINCHESTERS; i++)
        {
            drives[i].image = Profile::OpenDisk(i);

            if (!drives[i].image)
                continue;

            drives[i].imageSize = drives[i].image->GetSize();

            Logger::Log(STORAGER2_LOG_PREFIX, std::format("si{}: {} byte image ({} blocks of {})",
                i, drives[i].imageSize, drives[i].imageSize / STORAGER2_BLOCK_SIZE, STORAGER2_BLOCK_SIZE).c_str());
        }

        if (!drives[0].image)
            Logger::Log(STORAGER2_LOG_PREFIX, "Storager 2 fitted with no drive on unit 0 - si0 will report not "
                "installed. Point profileDisk0Path at a disk image to attach one.");

        Logger::Log(STORAGER2_LOG_PREFIX, std::format("Storager 2 fitted: registers at 0x{:x}, Multibus IRQ {}.",
            STORAGER2_MBIO_START & 0xFFFF, STORAGER2_MULTIBUS_IRQ_LEVEL).c_str());
    }

    void Storager2::Shutdown()
    {
        for (Drive& drive : drives)
        {
            if (!drive.image)
                continue;

            Profile::CloseDisk(drive.image);
            drive.image = nullptr;
        }
    }

    void Storager2::Trace(const std::string& message)
    {
        if (!logEnabled)
            return;

        // Not LogChannels::Debug: logging.hpp drops the whole Debug channel when RELEASE is
        // defined, and RelWithDebInfo defines it. The cvar is the gate instead.
        Logger::Log(STORAGER2_LOG_PREFIX, message.c_str());
    }

    //
    // The window. Everything below this line is in Multibus byte order.
    //

    uint8_t Storager2::Read8(size_t addr)
    {
        size_t mbOffset = (addr & STORAGER2_MBIO_MASK) ^ 1;

        if (mbOffset >= STORAGER2_REG_R0)
            return ReadRegister(mbOffset);

        return io[mbOffset];
    }

    void Storager2::Write8(size_t addr, uint8_t value)
    {
        size_t mbOffset = (addr & STORAGER2_MBIO_MASK) ^ 1;

        if (mbOffset >= STORAGER2_REG_R0)
        {
            WriteRegister(mbOffset, value);
            return;
        }

        io[mbOffset] = value;

        /*
            The control words. One byte per IOPB at even offsets 0x00 to 0x1A, and writing one with
            SC_ENIOPB set is how siistart() hands that IOPB to the board. 0x1C-0x1F are the overlapped
            seek enable and the IOPB count, which are recorded and otherwise ignored - the count only
            matters to a board that is walking the ring on its own, and this one is told about every
            IOPB explicitly.
        */
        if (mbOffset < (STORAGER2_NUM_IOPBS * STORAGER2_CW_STRIDE)
        && !(mbOffset & 1)
        && (value & STORAGER2_SC_ENIOPB))
        {
            size_t index = mbOffset / STORAGER2_CW_STRIDE;
            size_t base = STORAGER2_IOPB_BASE + (index * STORAGER2_IOPB_STRIDE);
            uint8_t unit = io[base + STORAGER2_IOPB_OFF_UNIT];

            bool ok = ExecuteIOPB(true, base);

            /*
                Completions queue. The driver runs siistart() under spl6 and can hand over several
                IOPBs before the level 5 handler gets a chance to run, so a single "which unit
                interrupted" register would lose all but the last of them - and a lost completion is a
                buffer that is never iodone()d, which is a wedged filesystem. R0 and R1 report the head
                of this queue and ST_CLEAR pops it, exactly like a real fourteen deep board.
            */
            if (completionCount < (int32_t)(sizeof(completions) / sizeof(completions[0])))
            {
                int32_t slot = (completionHead + completionCount) % (int32_t)(sizeof(completions) / sizeof(completions[0]));

                completions[slot].iopb = (uint8_t)index;
                completions[slot].unit = unit;
                completions[slot].error = !ok;
                completionCount++;
            }
            else
            {
                Logger::Log(STORAGER2_LOG_PREFIX, std::format("Completion queue overflowed launching IOPB {} - a transfer has been lost",
                    index).c_str(), LogChannels::Error);
            }

            AssertIRQLine();
        }
    }

    uint16_t Storager2::Read16(size_t addr)
    {
        return (uint16_t)((Read8(addr) << 8) | Read8(addr + 1));
    }

    void Storager2::Write16(size_t addr, uint16_t value)
    {
        Write8(addr, (uint8_t)(value >> 8));
        Write8(addr + 1, (uint8_t)(value & 0xFF));
    }

    uint32_t Storager2::Read32(size_t addr)
    {
        return (uint32_t)((Read16(addr) << 16) | Read16(addr + 2));
    }

    void Storager2::Write32(size_t addr, uint32_t value)
    {
        Write16(addr, (uint16_t)(value >> 16));
        Write16(addr + 2, (uint16_t)(value & 0xFFFF));
    }

    //
    // R0-R3. R4-R7 are the tape half of the board and are not decoded at all; see storager2.hpp.
    //

    uint8_t Storager2::ReadRegister(size_t mbOffset)
    {
        switch (mbOffset - STORAGER2_REG_R0)
        {
        case 0:
            return ReadStatusRegister();
        case 1:
            /*
                RDINTR - which unit and which IOPB caused the interrupt. siiintr() takes the IOPB index
                from bits 3-6 and the unit from bits 0-2, and bails out without touching anything if
                the unit is above 3, which is its way of ignoring the tape's interrupts.
            */
            if (completionCount > 0)
                return (uint8_t)((completions[completionHead].iopb << 3) | (completions[completionHead].unit & 0x07));

            return 0;
        case 2:
            // TAPESTATUS. There is no tape; siiintr() only ever compares this against itself.
            return 0;
        case 3:
            return (uint8_t)(iopbAddress & 0xFF);
        default:
            return 0xFF;
        }
    }

    void Storager2::WriteRegister(size_t mbOffset, uint8_t value)
    {
        switch (mbOffset - STORAGER2_REG_R0)
        {
        case 0:
            WriteCommandRegister(value);
            return;
        case 1:
            iopbAddress = (iopbAddress & 0x00FFFF) | ((uint32_t)value << 16);
            return;
        case 2:
            iopbAddress = (iopbAddress & 0xFF00FF) | ((uint32_t)value << 8);
            return;
        case 3:
            iopbAddress = (iopbAddress & 0xFFFF00) | value;
            return;
        default:
            return;
        }
    }

    uint8_t Storager2::ReadStatusRegister()
    {
        /*
            The reset self test. siiprobe() writes ST_RESET, waits for DONE to go *away*, then waits
            for it to come *true*, and treats a timeout on either as CONF_DEAD - so a board that
            answers DONE the instant it is reset probes as dead just as surely as one that never
            answers at all. The wait is counted in status reads rather than in emulated microseconds
            because the driver's timeouts are spin loops: tying this to the clock means the answer
            depends on how fast the host is running the 68020 that day, and counting reads cannot
            deadlock however fast or slow that is.
        */
        // Not on a debugger read - the memory editor drawing this register must not be what advances
        // the self test, or the probe sees DONE come true early and depends on where the debugger is
        // pointed. See AddrSpacePeek.
        if (resetSettleReads > 0
        && !AddrSpace::IsPeeking())
        {
            resetSettleReads--;

            if (resetSettleReads == 0)
                done = true;
        }

        uint8_t value = 0;

        /*
            ST_BUSY never comes back set: a command finishes inside the store that starts it, so there
            is no window in which the host could see the board working. siicmd() spins on this before
            every command and is happy to find it clear.
        */
        if (completionCount > 0)
        {
            value |= STORAGER2_ST_DONE;

            if (completions[completionHead].error)
                value |= STORAGER2_ST_ERROR;
        }

        if (done)
            value |= STORAGER2_ST_DONE;

        if (error)
            value |= STORAGER2_ST_ERROR;

        if (queueMode)
            value |= STORAGER2_ST_QUEUEMODE;

        /*
            ST_IOPBMASK is 0x78 and overlaps ST_ERROR at bit 3, so the two cannot both be reported
            here. The driver reads the IOPB index out of R1 and only ever tests ST_ERROR in R0, so the
            error bit wins.
        */
        return value;
    }

    void Storager2::WriteCommandRegister(uint8_t value)
    {
        // Order matters: STARTWO() is ST_START | ST_NOINTERRUPT, so the action bits have to be tested
        // rather than the whole byte compared.
        if (value & STORAGER2_CMD_RESET)
        {
            ResetController();
            return;
        }

        if (value & STORAGER2_CMD_ABORT)
        {
            // siiattach()'s error path aborts and then waits for DONE to go away.
            done = false;
            error = false;
            queueMode = false;
            completionHead = 0;
            completionCount = 0;
            ClearIRQLine();

            Trace("Abort");
            return;
        }

        if (value & STORAGER2_CMD_CLEAR)
        {
            ClearCompletion();
            return;
        }

        if (!(value & STORAGER2_CMD_START))
            return;

        /*
            All ones in R1/R2/R3 is not an IOPB address, it is siistart() asking to go into queued
            mode: from here on the IOPBs are the board's own RAM and each one is launched by its
            control word rather than by a start command.
        */
        if (iopbAddress == MULTIBUS_ADDRESS_MASK)
        {
            if (!queueMode)
            {
                queueMode = true;
                done = false;
                error = false;
                completionHead = 0;
                completionCount = 0;
                ClearIRQLine();

                Trace("Entered queued mode");
            }

            return;
        }

        done = false;
        error = false;

        bool ok = ExecuteIOPB(false, iopbAddress);

        done = true;
        error = !ok;

        /*
            A non queued command never raises the interrupt here even when the host forgets
            ST_NOINTERRUPT. siiintr() answers an interrupt by reading an IOPB index out of R1 and
            dereferencing the struct buf * the driver left in that IOPB - and a non queued command has
            no IOPB in the ring and no buf, so servicing one would find a null pointer and panic. Every
            caller in sii.c uses STARTWO(), so this only guards against a path that does not exist.
        */
        if (!(value & STORAGER2_CMD_NOINTERRUPT))
            Logger::Log(STORAGER2_LOG_PREFIX, "A non-queued command asked for an interrupt; there is no IOPB for the "
                "handler to find, so it has been suppressed", LogChannels::Warning);
    }

    void Storager2::ResetController()
    {
        done = false;
        error = false;
        queueMode = false;
        iopbAddress = 0;
        completionHead = 0;
        completionCount = 0;
        resetSettleReads = STORAGER2_RESET_SETTLE_READS;

        for (Drive& drive : drives)
            drive.initialised = false;

        ClearIRQLine();

        Trace("Reset");
    }

    void Storager2::ClearCompletion()
    {
        /*
            ST_CLEAR acknowledges one completion. Anything left in the queue keeps DONE set and the
            interrupt asserted, so the handler is re-entered and collects the next one - which is the
            only way several IOPBs finishing inside one spl6 region all get reported.
        */
        done = false;
        error = false;

        if (completionCount > 0)
        {
            completionHead = (completionHead + 1) % (int32_t)(sizeof(completions) / sizeof(completions[0]));
            completionCount--;
        }

        if (completionCount == 0)
            ClearIRQLine();
    }

    //
    // IOPB execution
    //

    uint8_t Storager2::IOPBRead8(bool queued, size_t base, size_t offset)
    {
        if (queued)
            return io[(base + offset) & STORAGER2_MBIO_MASK];

        return MBRead8(base + offset);
    }

    void Storager2::IOPBWrite8(bool queued, size_t base, size_t offset, uint8_t value)
    {
        if (queued)
        {
            io[(base + offset) & STORAGER2_MBIO_MASK] = value;
            return;
        }

        MBWrite8(base + offset, value);
    }

    void Storager2::FetchIOPB(bool queued, size_t base, IOPB& out)
    {
        out.command = IOPBRead8(queued, base, STORAGER2_IOPB_OFF_COMMAND);
        out.options = IOPBRead8(queued, base, STORAGER2_IOPB_OFF_OPTIONS);
        out.unit = IOPBRead8(queued, base, STORAGER2_IOPB_OFF_UNIT);
        out.head = IOPBRead8(queued, base, STORAGER2_IOPB_OFF_HEAD);

        out.cylinder = (uint16_t)((IOPBRead8(queued, base, STORAGER2_IOPB_OFF_CYLINDER) << 8)
            | IOPBRead8(queued, base, STORAGER2_IOPB_OFF_CYLINDER + 1));

        out.sector = (uint16_t)((IOPBRead8(queued, base, STORAGER2_IOPB_OFF_SECTOR) << 8)
            | IOPBRead8(queued, base, STORAGER2_IOPB_OFF_SECTOR + 1));

        out.sectorCount = (uint16_t)((IOPBRead8(queued, base, STORAGER2_IOPB_OFF_COUNT) << 8)
            | IOPBRead8(queued, base, STORAGER2_IOPB_OFF_COUNT + 1));

        out.bufferAddress = ((uint32_t)IOPBRead8(queued, base, STORAGER2_IOPB_OFF_BUFFER) << 16)
            | ((uint32_t)IOPBRead8(queued, base, STORAGER2_IOPB_OFF_BUFFER + 1) << 8)
            | IOPBRead8(queued, base, STORAGER2_IOPB_OFF_BUFFER + 2);
    }

    bool Storager2::ExecuteIOPB(bool queued, size_t base)
    {
        IOPB iopb = {0};
        FetchIOPB(queued, base, iopb);

        uint8_t errorCode = 0;
        bool ok = false;

        switch (iopb.command)
        {
        case STORAGER2_C_INIT:
            ok = Initialise(iopb, errorCode);
            break;

        /*
            Read absolute skips the bad block map, and read-no-cache is what attach uses to be sure it
            is looking at the platter rather than at whatever the last command left in the buffer.
            There is no cache and no bad block map here, so all three are the same read.
        */
        case STORAGER2_C_READ:
        case STORAGER2_C_READNOCACHE:
        case STORAGER2_C_RDABSOLUTE:
            ok = Transfer(iopb, false, errorCode);
            break;

        case STORAGER2_C_WRITE:
            ok = Transfer(iopb, true, errorCode);
            break;

        case STORAGER2_C_VERIFY:
        {
            // Nothing to compare against - just answer whether the blocks are addressable.
            size_t linear = 0;

            if (iopb.unit >= STORAGER2_MAX_UNITS)
                errorCode = STORAGER2_ERR_BADUNIT;
            else
                ok = CheckAddress(iopb, drives[iopb.unit], linear, errorCode);

            break;
        }

        /*
            C_REPORT is a controller command, not a drive one: stdprobe() issues it with unit 0 before
            it has looked for a drive at all, and treats a failure as CONF_DEAD. It is the whole
            reason the PROM could not boot off this board - it got as far as asking and got an
            "invalid command code" back.
        */
        case STORAGER2_C_REPORT:
            IOPBWrite8(queued, base, STORAGER2_IOPB_OFF_ERROR, STORAGER2_REPORT_FIRMWARE);
            IOPBWrite8(queued, base, STORAGER2_IOPB_OFF_UNIT, STORAGER2_REPORT_EXTENSION);
            IOPBWrite8(queued, base, STORAGER2_IOPB_OFF_HEAD, STORAGER2_REPORT_PRODUCT);
            IOPBWrite8(queued, base, STORAGER2_IOPB_OFF_CYLINDER, (STORAGER2_REPORT_OPTIONS >> 8) & 0xFF);
            IOPBWrite8(queued, base, STORAGER2_IOPB_OFF_CYLINDER + 1, STORAGER2_REPORT_OPTIONS & 0xFF);

            Trace("Report: firmware 2.1");
            ok = true;
            break;

        case STORAGER2_C_SEEK:
        case STORAGER2_C_RESTORE:
        case STORAGER2_C_RESET:
            if (iopb.unit >= STORAGER2_MAX_UNITS)
                errorCode = STORAGER2_ERR_BADUNIT;
            else if (!drives[iopb.unit].image)
                errorCode = STORAGER2_ERR_NOTREADY;
            else
                ok = true;

            break;

        default:
            errorCode = STORAGER2_ERR_BADCOMMAND;

            Logger::Log(STORAGER2_LOG_PREFIX, std::format("Unimplemented command 0x{:02x} for unit {}",
                iopb.command, iopb.unit).c_str(), LogChannels::Warning);
            break;
        }

        IOPBWrite8(queued, base, STORAGER2_IOPB_OFF_STATUS, ok ? STORAGER2_S_OK : STORAGER2_S_ERROR);

        // Only touch the error byte when there is an error to report. Both drivers zero it before
        // every command, and C_REPORT has already put its firmware revision there.
        if (!ok)
            IOPBWrite8(queued, base, STORAGER2_IOPB_OFF_ERROR, errorCode);

        return ok;
    }

    /*
        C_INIT - take a Unit Initialization Block and believe what it says about the drive. A real
        controller checks the geometry against the drive it can actually see, which is how siiattach()
        can walk sii_uibs[] trying a hard sectored ESDI, then a soft sectored one, then an ST506 until
        one takes. There is only ever one kind of drive here, so every UIB is accepted and the loop
        stops at the first - and then attach reads the label and re-initialises with the real geometry
        out of it, which is the one that matters.
    */
    bool Storager2::Initialise(const IOPB& iopb, uint8_t& errorOut)
    {
        if (iopb.unit >= STORAGER2_MAX_UNITS)
        {
            errorOut = STORAGER2_ERR_BADUNIT;
            return false;
        }

        Drive& drive = drives[iopb.unit];

        if (!drive.image)
        {
            errorOut = STORAGER2_ERR_NOTREADY;
            return false;
        }

        uint8_t uib[STORAGER2_UIB_SIZE] = {0};

        for (size_t i = 0; i < STORAGER2_UIB_SIZE; i++)
            uib[i] = MBRead8(iopb.bufferAddress + i);

        uint8_t heads = uib[STORAGER2_UIB_OFF_HEADS];
        uint8_t sectorsPerTrack = uib[STORAGER2_UIB_OFF_SPT];
        uint16_t bytesPerSector = (uint16_t)(uib[STORAGER2_UIB_OFF_BPS_LOW] | (uib[STORAGER2_UIB_OFF_BPS_HIGH] << 8));
        uint16_t cylinders = (uint16_t)(uib[STORAGER2_UIB_OFF_CYL_LOW] | (uib[STORAGER2_UIB_OFF_CYL_HIGH] << 8));

        if (bytesPerSector != STORAGER2_BLOCK_SIZE)
        {
            errorOut = STORAGER2_ERR_BADSECTORSIZE;
            return false;
        }

        if (!sectorsPerTrack)
        {
            errorOut = STORAGER2_ERR_BADSPT;
            return false;
        }

        if (!heads)
        {
            errorOut = STORAGER2_ERR_BADHEADCOUNT;
            return false;
        }

        if (!cylinders)
        {
            errorOut = STORAGER2_ERR_BADCYLCOUNT;
            return false;
        }

        drive.heads = heads;
        drive.sectorsPerTrack = sectorsPerTrack;
        drive.bytesPerSector = bytesPerSector;
        drive.cylinders = cylinders;
        drive.initialised = true;

        Trace(std::format("Initialize: unit {} is {}/{}/{}, drive descriptor 0x{:02x}",
            iopb.unit, cylinders, heads, sectorsPerTrack, uib[STORAGER2_UIB_OFF_DDB]));

        return true;
    }

    bool Storager2::CheckAddress(const IOPB& iopb, const Drive& drive, size_t& linearOut, uint8_t& errorOut)
    {
        if (!drive.initialised)
        {
            errorOut = STORAGER2_ERR_NOTINITIALISED;
            return false;
        }

        if (iopb.cylinder >= drive.cylinders)
        {
            errorOut = STORAGER2_ERR_BADCYLINDER;
            return false;
        }

        if (iopb.head >= drive.heads)
        {
            errorOut = STORAGER2_ERR_BADHEAD;
            return false;
        }

        if (iopb.sector >= drive.sectorsPerTrack)
        {
            errorOut = STORAGER2_ERR_BADSECTOR;
            return false;
        }

        // dklabel.h: lba = cyl * (heads * sectors) + head * sectors + sec
        linearOut = ((((size_t)iopb.cylinder * drive.heads) + iopb.head) * drive.sectorsPerTrack + iopb.sector)
            * drive.bytesPerSector;

        if (linearOut >= drive.imageSize)
        {
            errorOut = STORAGER2_ERR_SECTORNOTFOUND;
            return false;
        }

        return true;
    }

    bool Storager2::Transfer(const IOPB& iopb, bool write, uint8_t& errorOut)
    {
        if (iopb.unit >= STORAGER2_MAX_UNITS)
        {
            errorOut = STORAGER2_ERR_BADUNIT;
            return false;
        }

        Drive& drive = drives[iopb.unit];

        if (!drive.image)
        {
            errorOut = STORAGER2_ERR_NOTREADY;
            return false;
        }

        size_t linear = 0;

        if (!CheckAddress(iopb, drive, linear, errorOut))
            return false;

        if (!iopb.sectorCount)
            return true;

        size_t length = (size_t)iopb.sectorCount * drive.bytesPerSector;

        if ((linear + length) > drive.imageSize)
        {
            Logger::Log(STORAGER2_LOG_PREFIX, std::format("{} of {} bytes at 0x{:x} runs off the end of a {} byte image",
                write ? "Write" : "Read", length, linear, drive.imageSize).c_str(), LogChannels::Warning);

            errorOut = STORAGER2_ERR_SECTORNOTFOUND;
            return false;
        }

        // Only the first megabyte of the backplane is a window onto system RAM, so anything past it
        // wraps and lands somewhere silly rather than where the host meant.
        if ((iopb.bufferAddress + length) > MULTIBUS_SLAVE_WINDOW_END)
            Logger::Log(STORAGER2_LOG_PREFIX, std::format("Transfer to 0x{:x}..0x{:x} leaves the emulated 1MB multibus window and will wrap",
                iopb.bufferAddress, iopb.bufferAddress + length).c_str(), LogChannels::Warning);

        uint8_t sector[STORAGER2_BLOCK_SIZE] = {0};
        size_t transferred = 0;

        while (transferred < length)
        {
            size_t chunk = length - transferred;

            if (chunk > drive.bytesPerSector)
                chunk = drive.bytesPerSector;

            size_t buffer = iopb.bufferAddress + transferred;

            if (write)
            {
                size_t i = 0;

                /*
                    The byte lanes are crossed, so a 16-bit read at an even address gives the byte at
                    that address in the low half. Anything odd, or a trailing byte, goes one at a time
                    through MBRead8, which does the crossing itself.
                */
                if (!(buffer & 1))
                {
                    for (; (i + 1) < chunk; i += 2)
                    {
                        uint16_t dat = multibus->ReadMB16(buffer + i);

                        sector[i] = (uint8_t)(dat & 0xFF);
                        sector[i + 1] = (uint8_t)(dat >> 8);
                    }
                }

                for (; i < chunk; i++)
                    sector[i] = MBRead8(buffer + i);

                // Where this lands depends on the mode - straight to the file, or into the
                // copy-on-write overlay. See disk_image.hpp.
                if (!drive.image->Write(linear + transferred, sector, chunk))
                {
                    Logger::Log(STORAGER2_LOG_PREFIX, std::format("Write of {} bytes at 0x{:x} failed",
                        chunk, linear + transferred).c_str(), LogChannels::Error);

                    errorOut = STORAGER2_ERR_BUSTIMEOUT;
                    return false;
                }
            }
            else
            {
                if (!drive.image->Read(linear + transferred, sector, chunk))
                {
                    Logger::Log(STORAGER2_LOG_PREFIX, std::format("Read of {} bytes at 0x{:x} came up short",
                        chunk, linear + transferred).c_str(), LogChannels::Error);

                    errorOut = STORAGER2_ERR_SECTORNOTFOUND;
                    return false;
                }

                size_t i = 0;

                // The other way round: writing (sector[i + 1] << 8) | sector[i] as a 16-bit word puts
                // sector[i] at multibus address buffer + i and sector[i + 1] at buffer + i + 1.
                if (!(buffer & 1))
                {
                    for (; (i + 1) < chunk; i += 2)
                        multibus->WriteMB16(buffer + i, (uint16_t)((sector[i + 1] << 8) | sector[i]));
                }

                for (; i < chunk; i++)
                    MBWrite8(buffer + i, sector[i]);
            }

            transferred += chunk;
        }

        Trace(std::format("{}: unit {} {}/{}/{} x{} -> disk 0x{:x}, multibus 0x{:x}, {} bytes",
            write ? "Write" : "Read", iopb.unit, iopb.cylinder, iopb.head, iopb.sector,
            iopb.sectorCount, linear, iopb.bufferAddress, transferred));

        return true;
    }

    void Storager2::AssertIRQLine()
    {
        if (irqAsserted)
            return;

        irqAsserted = true;
        multibus->SetMultibusIRQ(STORAGER2_MULTIBUS_IRQ_LEVEL, true);
    }

    void Storager2::ClearIRQLine()
    {
        // The Multibus interrupt lines are shared and there is no arbitration in here, so only drop
        // one this board actually raised.
        if (!irqAsserted)
            return;

        irqAsserted = false;
        multibus->SetMultibusIRQ(STORAGER2_MULTIBUS_IRQ_LEVEL, false);
    }
};
