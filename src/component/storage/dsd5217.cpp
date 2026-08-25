/* motion - The SGI Emulator. Copyright (c)2026 starfrost. dsd5217.cpp: The Qualogy (previously known as Data Systems Design) DSD 5217 Multibus Disk & Tape Controller Technically. */

#include <component/storage/dsd5217.hpp>

namespace Motion
{
    void DSD5217::Start()
    {
        if (Profile::GetDiskController() != DiskControllerType::DSD5217)
        {
            Logger::Log(DSD5217_LOG_PREFIX, "DSD 5217 is not the fitted disk controller. +set diskController dsd fits it.");
            return;
        }

        multibus = Emulation::GetMachine()->FindComponentByType<Multibus>();

        // The only thing this board decodes is its single programmed I/O port.
        Multibus::SlotMapping slot = Multibus::SlotMapping(this);
        slot.ioStart = DSD5217_MBIO_START;
        slot.ioEnd = DSD5217_MBIO_END;
        slot.id = DSD5217_MULTIBUS_SLOTNUM;

        multibus->AddSlotMapping(slot);

        // The two winchesters the controller supports, md0 and md1.
        for (int32_t i = 0; i < DSD5217_MAX_DISK_DRIVES; i++)
            drives[i].image = Profile::OpenDisk(i);

        dsdExtension = new CoherentExtensionDSD5217(this);
        Coherent::RegisterExtension(dsdExtension);
    }

    // Multibus byte N is the byte the host wrote at N ^ 1, and multi-byte fields are little endian as the 8085 sees them.

    uint8_t DSD5217::MBRead8(size_t mbAddr)
    {
        return multibus->ReadMB8(mbAddr ^ 1);
    }

    void DSD5217::MBWrite8(size_t mbAddr, uint8_t value)
    {
        multibus->WriteMB8(mbAddr ^ 1, value);
    }

    uint16_t DSD5217::MBRead16(size_t mbAddr)
    {
        return (uint16_t)(MBRead8(mbAddr) | (MBRead8(mbAddr + 1) << 8));
    }

    void DSD5217::MBWrite16(size_t mbAddr, uint16_t value)
    {
        MBWrite8(mbAddr, value & 0xFF);
        MBWrite8(mbAddr + 1, (value >> 8) & 0xFF);
    }

    uint32_t DSD5217::MBRead32(size_t mbAddr)
    {
        return (uint32_t)MBRead16(mbAddr) | ((uint32_t)MBRead16(mbAddr + 2) << 16);
    }

    void DSD5217::MBWrite32(size_t mbAddr, uint32_t value)
    {
        MBWrite16(mbAddr, value & 0xFFFF);
        MBWrite16(mbAddr + 2, (value >> 16) & 0xFFFF);
    }

    // Chaining through the control blocks, exactly as 4.6.3 - 4.6.7 of the 5215 guide describe it

    bool DSD5217::FetchWakeUpBlock()
    {
        wub.extension = MBRead8(DSD5217_WUB_ADDRESS + DSD5217_WUB_OFF_EXTENSION);
        wub.ccbPtr = MBRead32(DSD5217_WUB_ADDRESS + DSD5217_WUB_OFF_CCB_PTR);

        if (wub.extension != DSD5217_24BIT_ADDRESSING)
        {
            Logger::Log(DSD5217_LOG_PREFIX, std::format("Wake-up block asked for addressing mode {} - only 24-bit linear (7) is implemented",
                wub.extension).c_str(), LogChannels::Warning);
            return false;
        }

        if (!wub.ccbPtr)
        {
            Logger::Log(DSD5217_LOG_PREFIX, "Wake-up block has a null CCB pointer", LogChannels::Warning);
            return false;
        }

        Logger::Log(DSD5217_LOG_PREFIX, std::format("Woke up: CCB is at multibus 0x{:x}",
            wub.ccbPtr & DSD5217_BLOCK_PTR_MASK).c_str(), LogChannels::Debug);

        return true;
    }

    bool DSD5217::FetchChannelBlocks()
    {
        if (!wub.ccbPtr)
            return false;

        size_t ccbAddr = wub.ccbPtr & DSD5217_BLOCK_PTR_MASK;

        ccb.ccw1 = MBRead8(ccbAddr + 0x00);
        ccb.busy = MBRead8(ccbAddr + 0x01);
        ccb.cibPtr = MBRead32(ccbAddr + 0x02);
        ccb.ccw2 = MBRead8(ccbAddr + 0x08);
        ccb.busy2 = MBRead8(ccbAddr + 0x09);
        ccb.cpPtr = MBRead32(ccbAddr + 0x0A);
        ccb.controlPtr = MBRead16(ccbAddr + 0x0E);

        if (!ccb.cibPtr)
        {
            Logger::Log(DSD5217_LOG_PREFIX, "Channel control block has a null CIB pointer", LogChannels::Warning);
            return false;
        }

        size_t cibAddr = CIBAddress();

        cib.opStatus = MBRead8(cibAddr + 0x01);
        cib.commandSemaphore = MBRead8(cibAddr + 0x02);
        cib.statusSemaphore = MBRead8(cibAddr + 0x03);
        cib.iopbPtr = MBRead32(cibAddr + 0x08);

        return true;
    }

    void DSD5217::FetchIOPB()
    {
        size_t iopbAddr = cib.iopbPtr & DSD5217_BLOCK_PTR_MASK;

        iopb.actualTransfers = MBRead32(iopbAddr + 0x04);
        iopb.deviceCode = MBRead16(iopbAddr + 0x08);
        iopb.unit = MBRead8(iopbAddr + 0x0A);
        iopb.function = MBRead8(iopbAddr + 0x0B);
        iopb.modifier = MBRead16(iopbAddr + 0x0C);
        iopb.cylinder = MBRead16(iopbAddr + 0x0E);
        iopb.head = MBRead8(iopbAddr + 0x10);
        iopb.sector = MBRead8(iopbAddr + 0x11);
        iopb.dba = MBRead32(iopbAddr + 0x12);
        iopb.rbc = MBRead32(iopbAddr + 0x16);
        iopb.generalPtr = MBRead32(iopbAddr + 0x1A);
    }

    void DSD5217::SetControllerBusy(bool busy)
    {
        ccb.busy = busy ? 0xFF : 0x00;

        // "posted when the controller is busy processing a command, and cleared after" - the I/O port must never touch it.
        if (wub.ccbPtr)
            MBWrite8((wub.ccbPtr & DSD5217_BLOCK_PTR_MASK) + 0x01, ccb.busy);
    }

    void DSD5217::PostStatus(uint8_t opStatus)
    {
        cib.opStatus = opStatus;

        if (ccb.cibPtr)
        {
            size_t cibAddr = CIBAddress();

            // "if it is zero, the controller assumes that previous status information has been accepted by the host"
            if (MBRead8(cibAddr + 0x03))
                Logger::Log(DSD5217_LOG_PREFIX, "Host hasn't picked up the previous status yet, overwriting it", LogChannels::Debug);

            MBWrite8(cibAddr + 0x01, opStatus);
            MBWrite8(cibAddr + 0x03, 0xFF);

            cib.statusSemaphore = 0xFF;
        }

        SetControllerBusy(false);

        if (!(iopb.modifier & DSD5217_MODIFIER_NO_INT))
            AssertIRQLine();
    }

    // Programmed I/O. Only writes are recognised, and only the bottom two bits of them.

    uint8_t DSD5217::Read8(size_t addr)
    {
        // "Only I/O write operations are recognized" - nothing drives the bus on a read
        return 0xFF;
    }

    void DSD5217::Write8(size_t addr, uint8_t value)
    {
        // With no drive fitted the board does not answer its port at all, which is what makes dsd0 probe as absent.
        if (!drives[0].image
        && !drives[1].image)
            return;

        addr &= 0xFFFFF;

        if (addr != DSD5217_MBIO_COMMAND)
            return;

        state = value;

        switch (value & DSD5217_IO_COMMAND_MASK)
        {
        case DSD5217_IO_CLEAR:
            // Drops the interrupt and the reset, and deliberately NOT the busy flag - SGI issues one after every command.
            ClearIRQLine();
            inReset = false;
            break;
        case DSD5217_IO_RESET:
            ClearIRQLine();
            inReset = true;
            tablesFetched = false;
            break;
        case DSD5217_IO_START:
            if (inReset)
            {
                Logger::Log(DSD5217_LOG_PREFIX, "Start command issued while the controller is held in reset", LogChannels::Warning);
                break;
            }

            if (!tablesFetched)
            {
                // "The first programmed I/O start command is treated in a special way when the controller has been reset.
                if (FetchWakeUpBlock() && FetchChannelBlocks())
                {
                    tablesFetched = true;
                    SetControllerBusy(false);
                }

                break;
            }

            ExecuteCommand();
            break;
        default:
            // 03h isn't a documented command
            break;
        }
    }

    uint16_t DSD5217::Read16(size_t addr)
    {
        return (Read8(addr) | Read8(addr + 1) << 8);
    }

    void DSD5217::Write16(size_t addr, uint16_t value)
    {
        // Strangely enough, this is a little endian peripheral. Huh!
        Write8(addr, (value & 0x00FF));
        Write8(addr + 1, ((value & 0xFF00) >> 8));
    }

    //
    // command execution
    //
    void DSD5217::ExecuteCommand()
    {
        // we only emualte the hard drive right now
        if (!drives[0].image
        && !drives[1].image)
            return; 

        SetControllerBusy(true);

        // the host may move the CIB and the IOPB between commands, so re-chain every time
        if (!FetchChannelBlocks()
        || !cib.iopbPtr)
        {
            Logger::Log(DSD5217_LOG_PREFIX, "Start command issued but the control block chain is broken", LogChannels::Warning);
            SetControllerBusy(false);
            return;
        }

        FetchIOPB();

        // Which of the two winchesters this command is for. Everything below works through it.
        currentDrive = CurrentDrive();

        bool commandIsImplemented = true;
        bool ok = true;

        if (iopb.deviceCode == DSD5217_DEVICE_CODE_HDD
        && !currentDrive)
        {
            // A command for a drive that is not fitted.
            inist.sb[DSD5217_SB_HARD_ERROR1] |= DSD5217_HARDERR1_UNIT_NOT_READY;
            ok = false;
        }
        else if (iopb.deviceCode != DSD5217_DEVICE_CODE_HDD)
        {
            Logger::Log(DSD5217_LOG_PREFIX, "Only HDD commands are currently supported! (QIC, Floppy not implemented!)", LogChannels::Warning);
            commandIsImplemented = false; 
        }
        else
        {
            switch (iopb.function)
            {
                case DSD5217_FUNC_INIT:
                    ok = ReadInitBlock();
                    break;
                case DSD5217_FUNC_XFER_STATUS:
                    ok = WriteStatusBlock();
                    break;
                case DSD5217_FUNC_READ_DATA:
                    ok = ReadSector();
                    break;
                case DSD5217_FUNC_WRITE:
                    ok = WriteSector();
                    break; 
                default:
                    commandIsImplemented = false; 
                    break;
            }
        }

        if (!commandIsImplemented)
            Logger::Log(DSD5217_LOG_PREFIX, std::format("It's time to implement command 0x{:x} (device 0x{:x})",
                iopb.function, iopb.deviceCode).c_str(), LogChannels::Debug);
        else
            Logger::Log(DSD5217_LOG_PREFIX, std::format("Executed command 0x{:x}", iopb.function).c_str(), LogChannels::Debug);

        // Winchester "immediate function complete", with the unit that ran it in bits 5:4.
        uint8_t opStatus = DSD5217_OPERATION_STATUS_COMPLETE
            | ((iopb.unit << DSD5217_OPERATION_UNIT_SHIFT) & DSD5217_OPERATION_UNIT_BITS);

        if (!ok)
            opStatus |= (DSD5217_OPERATION_SUMMARY_ERROR | DSD5217_OPERATION_HARD_ERROR);

        PostStatus(opStatus);
    }

    DSD5217::Drive* DSD5217::CurrentDrive()
    {
        if (iopb.unit >= DSD5217_MAX_DISK_DRIVES
        || !drives[iopb.unit].image)
            return nullptr;

        return &drives[iopb.unit];
    }

    size_t DSD5217::GetBytesPerSector()
    {
        if (!currentDrive)
            return 0;

        return (size_t)((currentDrive->inib.bytesPerSectorHigh << 8) | currentDrive->inib.bytesPerSectorLow);
    }

    // Initialize (00h): the data buffer holds the geometry of the drive being initialised
    bool DSD5217::ReadInitBlock()
    {
        size_t dba = iopb.dba;

        INIB& inib = currentDrive->inib;

        inib.nrCylinders = MBRead16(dba + 0x00);
        inib.fixedHeads = MBRead8(dba + 0x02);
        inib.removableHeads = MBRead8(dba + 0x03);
        inib.sectorsPerTrack = MBRead8(dba + 0x04);
        inib.bytesPerSectorLow = MBRead8(dba + 0x05);
        inib.bytesPerSectorHigh = MBRead8(dba + 0x06);
        inib.numberOfAlternateCylinders = MBRead8(dba + 0x07);

        size_t bytesPerSector = GetBytesPerSector();

        Logger::Log(DSD5217_LOG_PREFIX, std::format("Initialise unit {}: {} cylinders ({} alternate), {} fixed / {} removable heads, "
            "{} sectors per track, {} bytes per sector", iopb.unit, inib.nrCylinders, inib.numberOfAlternateCylinders,
            inib.fixedHeads, inib.removableHeads, inib.sectorsPerTrack, bytesPerSector).c_str());

        if (!bytesPerSector
        || bytesPerSector > DSD5217_MAXIMUM_BUFFER_SIZE
        || !inib.sectorsPerTrack)
        {
            Logger::Log(DSD5217_LOG_PREFIX, "Initialise specified a disk format this controller can't do", LogChannels::Warning);
            inist.sb[DSD5217_SB_HARD_ERROR0] |= DSD5217_HARDERR0_ILLEGAL_FORMAT;
            return false;
        }

        currentDrive->initialised = true;
        return true;
    }

    // Transfer Status (01h): hand the error status buffer back to the host, then clear it
    bool DSD5217::WriteStatusBlock()
    {
        for (int32_t i = 0; i < DSD5217_SB_SIZE; i++)
        {
            MBWrite8(iopb.dba + i, inist.sb[i]);
            inist.sb[i] = 0;
        }

        return true;
    }

    // Write Data (05h), the mirror of ReadSector below. A partial last sector is written short rather than read-modify-written.
    bool DSD5217::WriteSector()
    {
        size_t bytesPerSector = GetBytesPerSector();

        if (!bytesPerSector
        || bytesPerSector > DSD5217_MAXIMUM_BUFFER_SIZE)
        {
            Logger::Log(DSD5217_LOG_PREFIX, "Write issued before a valid Initialize command, refusing to transfer", LogChannels::Warning);
            inist.sb[DSD5217_SB_HARD_ERROR0] |= DSD5217_HARDERR0_ILLEGAL_FORMAT;
            return false;
        }

        size_t diskLinear = CHSToLinear();

        if (iopb.rbc == 0)
            iopb.rbc = (uint32_t)bytesPerSector;

        if ((iopb.dba + iopb.rbc) > 0xFFFFF)
            Logger::Log(DSD5217_LOG_PREFIX, std::format("Transfer from 0x{:x}..0x{:x} leaves the emulated 1MB multibus window and will wrap",
                iopb.dba, iopb.dba + iopb.rbc).c_str(), LogChannels::Warning);

        // Refuse to write past the end of the image rather than letting the stream extend it.
        size_t imageSize = currentDrive->image->GetSize();

        if (diskLinear >= imageSize)
        {
            Logger::Log(DSD5217_LOG_PREFIX, std::format("Refusing to write at 0x{:x}, past the end of a {} byte image",
                diskLinear, imageSize).c_str(), LogChannels::Warning);

            inist.sb[DSD5217_SB_HARD_ERROR0] |= DSD5217_HARDERR0_END_OF_MEDIA;
            return false;
        }

        uint32_t transferred = 0;
        bool endOfMedia = false;

        while (transferred < iopb.rbc
        && !endOfMedia)
        {
            uint32_t remaining = iopb.rbc - transferred;
            uint32_t bytesToTransfer = (remaining < bytesPerSector) ? remaining : (uint32_t)bytesPerSector;

            if (diskLinear + transferred + bytesToTransfer > imageSize)
            {
                bytesToTransfer = (uint32_t)(imageSize - (diskLinear + transferred));
                endOfMedia = true;

                if (!bytesToTransfer)
                    break;
            }

            multibus->ReadMBBlock(iopb.dba + transferred, sectorBuffer, bytesToTransfer);

            // Straight to the file or into the copy-on-write overlay, depending on the mode. See disk_image.hpp.
            if (!currentDrive->image->Write(diskLinear + transferred, sectorBuffer, bytesToTransfer))
            {
                Logger::Log(DSD5217_LOG_PREFIX, std::format("Write of {} bytes at 0x{:x} failed",
                    bytesToTransfer, diskLinear + transferred).c_str(), LogChannels::Error);

                inist.sb[DSD5217_SB_HARD_ERROR0] |= DSD5217_HARDERR0_END_OF_MEDIA;
                return false;
            }

            transferred += bytesToTransfer;
        }

        iopb.actualTransfers = transferred;

        MBWrite32((cib.iopbPtr & DSD5217_BLOCK_PTR_MASK) + 0x04, iopb.actualTransfers);

        Logger::Log(DSD5217_LOG_PREFIX, std::format("Write Data Command: Wrote {} of {} bytes from multibus memory 0x{:x} to 0x{:x} to disk position 0x{:x} to 0x{:x}",
            iopb.actualTransfers, iopb.rbc, iopb.dba, iopb.dba + iopb.actualTransfers, diskLinear, diskLinear + iopb.actualTransfers).c_str());

        if (transferred < iopb.rbc)
        {
            Logger::Log(DSD5217_LOG_PREFIX, std::format("Ran off the end of the disk image writing at 0x{:x}. Is the image big enough?",
                diskLinear + transferred).c_str(), LogChannels::Warning);

            inist.sb[DSD5217_SB_HARD_ERROR0] |= DSD5217_HARDERR0_END_OF_MEDIA;
            return false;
        }

        return true;
    }

    bool DSD5217::ReadSector()
    {
        size_t bytesPerSector = GetBytesPerSector();

        if (!bytesPerSector
        || bytesPerSector > DSD5217_MAXIMUM_BUFFER_SIZE)
        {
            Logger::Log(DSD5217_LOG_PREFIX, "Read Data issued before a valid Initialize command, refusing to transfer", LogChannels::Warning);
            inist.sb[DSD5217_SB_HARD_ERROR0] |= DSD5217_HARDERR0_ILLEGAL_FORMAT;
            return false;
        }

        size_t diskLinear = CHSToLinear();

        // sgi why did you not program the requested transfer count
        if (iopb.rbc == 0)
            iopb.rbc = (uint32_t)bytesPerSector;

        // The 5217 drives 24 address bits but only the IP2's one megabyte window is modelled, so anything above it wraps.
        if ((iopb.dba + iopb.rbc) > 0xFFFFF)
            Logger::Log(DSD5217_LOG_PREFIX, std::format("Transfer to 0x{:x}..0x{:x} leaves the emulated 1MB multibus window and will wrap",
                iopb.dba, iopb.dba + iopb.rbc).c_str(), LogChannels::Warning);

        uint32_t transferred = 0;
        bool endOfMedia = false;

        while (transferred < iopb.rbc
        && !endOfMedia)
        {
            // "If the requested transfer count does not specify an integral number of sectors the last sector containing part of the data is read into the on-board.
            uint32_t remaining = iopb.rbc - transferred;
            uint32_t bytesToTransfer = (remaining < bytesPerSector) ? remaining : (uint32_t)bytesPerSector;

            // Clamp against what is left of the image: DiskImage refuses a read that runs off the end outright.
            size_t imageSize = currentDrive->image->GetSize();
            size_t remainingOnDisk = (diskLinear + transferred < imageSize)
                ? (imageSize - (diskLinear + transferred)) : 0;

            if (!remainingOnDisk)
            {
                endOfMedia = true;
                break;
            }

            if (remainingOnDisk < bytesToTransfer)
            {
                bytesToTransfer = (uint32_t)remainingOnDisk;
                endOfMedia = true;
            }

            if (!currentDrive->image->Read(diskLinear + transferred, sectorBuffer, bytesToTransfer))
            {
                endOfMedia = true;
                break;
            }

            multibus->WriteMBBlock(iopb.dba + transferred, sectorBuffer, bytesToTransfer);

            transferred += bytesToTransfer;
        }

        iopb.actualTransfers = transferred;

        // "Actual transfer count (returned at end of operation)"
        MBWrite32((cib.iopbPtr & DSD5217_BLOCK_PTR_MASK) + 0x04, iopb.actualTransfers);

        Logger::Log(DSD5217_LOG_PREFIX, std::format("Read Data Command: Read {} of {} bytes to multibus memory 0x{:x} to 0x{:x} from disk position 0x{:x} to 0x{:x}",
            iopb.actualTransfers, iopb.rbc, iopb.dba, iopb.dba + iopb.actualTransfers, diskLinear, diskLinear + iopb.actualTransfers).c_str());

        if (transferred < iopb.rbc)
        {
            Logger::Log(DSD5217_LOG_PREFIX, std::format("Ran off the end of the disk image reading from 0x{:x}. Is the image big enough?",
                diskLinear + transferred).c_str(), LogChannels::Warning);

            inist.sb[DSD5217_SB_HARD_ERROR0] |= DSD5217_HARDERR0_END_OF_MEDIA;
            return false;
        }

        return true;
    }

    // Convert CHS to linear address
    // Probably should be in a "ComponentHDD" generic class
    size_t DSD5217::CHSToLinear()
    {
        size_t cylinderWeWant = iopb.cylinder;
        size_t headWeWant = iopb.head;
        size_t sectorWeWant = iopb.sector;

        // figure out the disk information
        size_t sectorsPerTrack = currentDrive->inib.sectorsPerTrack;
        size_t bytesPerSector = GetBytesPerSector();
        size_t numCyls = currentDrive->inib.nrCylinders;

        size_t nrHeads = (iopb.deviceCode == DSD5217_DEVICE_CODE_FLOPPY)
            ? currentDrive->inib.removableHeads : currentDrive->inib.fixedHeads;
       
        // floppy - cyl's start at 1, otherwise 0
        if (iopb.deviceCode == DSD5217_DEVICE_CODE_FLOPPY)
            cylinderWeWant--;

        MOTION_ASSERT(cylinderWeWant >= numCyls, "****** INVALID DISK CYLINDER REQUEST!!! ******");

        size_t final = (((cylinderWeWant * nrHeads) + headWeWant) * sectorsPerTrack + sectorWeWant) * bytesPerSector;
        return final;
    }

    void DSD5217::AssertIRQLine()
    {
        irqAsserted = true;
        multibus->SetMultibusIRQ(DSD5217_MULTIBUS_IRQ_LEVEL, true);
    }

    void DSD5217::ClearIRQLine()
    {
        // Only drop a line this board raised - the Multibus IRQs are shared and clearing blind would eat somebody else's.
        if (!irqAsserted)
            return;

        irqAsserted = false;
        multibus->SetMultibusIRQ(DSD5217_MULTIBUS_IRQ_LEVEL, false);
    }

    void DSD5217::Shutdown()
    {
        // Both are null when the board was never fitted.
        delete dsdExtension;

        for (Drive& drive : drives)
        {
            if (!drive.image)
                continue;

            Profile::CloseDisk(drive.image);
            drive.image = nullptr;
        }
    }
};
