/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    multibus.cpp: The Intel Multibus bus implementation.
*/

#include <base/emulation.hpp>
#include <component/multibus/multibus.hpp>

namespace Motion
{
    void Multibus::Start()
    {
        // add the multibus state
        multibusExtension = new CoherentExtensionMultibus(this);
        Coherent::RegisterExtension(multibusExtension);

        // map ip2 segment 4 (multibus memory)

        AddrSpaceMapping mappingMultibus = AddrSpaceMapping();
        mappingMultibus.startAddr = MULTIBUS_MEMORY_START;
        mappingMultibus.endAddr = MULTIBUS_MEMORY_END;
        mappingMultibus.component = this;

        AddrSpace::AddMapping(mappingMultibus);

        // map ip2 segment 5 (multibus IO)

        AddrSpaceMapping mappingIo = AddrSpaceMapping();
        mappingIo.startAddr = MULTIBUS_IO_START;
        mappingIo.endAddr = MULTIBUS_IO_END;
        mappingIo.component = this; 

        AddrSpace::AddMapping(mappingIo);

        // There used to be a second mapping here that pointed the top megabyte of physical RAM at this component, so that bus masters writing "Multibus memory".

        // find the memory so we can use it
        if (!memory)
            memory = Emulation::GetMachine()->FindComponentByType<Memory>();

        // guaranteed, the CPU Initialises before this.
        if (!cpu)
            cpu = Emulation::GetMachine()->FindComponentByType<ComponentCPU>();
    }

    // The eight Multibus interrupt lines are shared, open collector, and they do not map one to one onto the CPU's seven levels - 0 and 1 both come out on.
    /* A 16-bit access at an even address carries the byte at that address in its low half, so pairs move twice as fast as singles;
       anything odd or trailing goes a byte at a time through the ^ 1 that the crossing amounts to. */
    void Multibus::ReadMBBlock(size_t addr, uint8_t* dst, size_t length)
    {
        size_t i = 0;

        if (!(addr & 1))
        {
            for (; (i + 1) < length; i += 2)
            {
                uint16_t dat = ReadMB16(addr + i);

                dst[i] = (uint8_t)(dat & 0xFF);
                dst[i + 1] = (uint8_t)(dat >> 8);
            }
        }

        for (; i < length; i++)
            dst[i] = ReadMB8((addr + i) ^ 1);
    }

    void Multibus::WriteMBBlock(size_t addr, const uint8_t* src, size_t length)
    {
        size_t i = 0;

        if (!(addr & 1))
        {
            for (; (i + 1) < length; i += 2)
                WriteMB16(addr + i, (uint16_t)((src[i + 1] << 8) | src[i]));
        }

        for (; i < length; i++)
            WriteMB8((addr + i) ^ 1, src[i]);
    }

    void Multibus::SetMultibusIRQ(int32_t number, bool asserted)
    {
        if (number < 0 || number >= MULTIBUS_NUM_IRQ)
        {
            Logger::Log(MULTIBUS_LOG_PREFIX, std::format("Tried to drive invalid IRQ #{}", number).c_str(), LogChannels::Warning);
            return;
        }

        if (!interrupts)
            interrupts = Emulation::GetMachine()->FindComponentByType<IP2Interrupt>();

        if (interrupts)
            interrupts->SetMultibusIRQ(number, asserted);
    }

    // is this stuff even faster 

    bool Multibus::UseCachedReadSlot(size_t addr)
    {
        if (!lastSlotRead)
            return false;

        return (addr >= lastSlotRead->ioStart
        && addr <= lastSlotRead->ioEnd);
    }

    bool Multibus::UseCachedWriteSlot(size_t addr)
    {
        if (!lastSlotWritten)
            return false;

        return ((addr >= lastSlotWritten->memStart && addr <= lastSlotWritten->memEnd) 
            || (addr >= lastSlotWritten->ioStart && addr <= lastSlotWritten->ioEnd));
    }

    bool Multibus::SetCachedReadMapping(size_t addr)
    {
        for (Multibus::SlotMapping& slot : slotMappings)
        {
            if (!slot.active)
                continue;

            if ((addr >= slot.memStart
            && addr <= slot.memEnd) 
            || (addr >= slot.ioStart
            && addr <= slot.ioEnd))
            {
                lastSlotRead = &slot;
                return true;
            }
        }

        // fail
        lastSlotRead = nullptr;
        return false; 
    }

    bool Multibus::SetCachedWriteMapping(size_t addr)
    {
        for (Multibus::SlotMapping& slot : slotMappings)
        {
            if (!slot.active)
                continue;

            if ((addr >= slot.memStart
            && addr <= slot.memEnd) 
            || (addr >= slot.ioStart
            && addr <= slot.ioEnd))
            {
                lastSlotWritten = &slot;
                return true;
            }
        }

        // fail
        lastSlotWritten = nullptr;
        return false; 
    }

    // this simulates the action of the user inserting a slot into the Multibus backplane.
    bool Multibus::AddSlotMapping(SlotMapping& slot)
    {
        slot.active = true; 

        // multibus is 24 bit
        if (slot.memStart
        && slot.memEnd)
        {
            // a card's memory window is a backplane address, which the CPU reaches through segment 4
            slot.memStart = MULTIBUS_MEMORY_START + (slot.memStart & MULTIBUS_ADDRESS_MASK);
            slot.memEnd = MULTIBUS_MEMORY_START + (slot.memEnd & MULTIBUS_ADDRESS_MASK);
        }

        if (!slot.memStart && !slot.memEnd && !slot.ioStart && !slot.ioEnd)
        {
            Logger::Log(MULTIBUS_LOG_PREFIX, "Multibus::AddSlotMapping: At least one of I/O and Memory address range must be set for a multibus mapping!",
            LogChannels::Error);
            return false;
        }

        Logger::Log(MULTIBUS_LOG_PREFIX, "Added new multibus slot:", LogChannels::Debug);

        if (slot.memStart || slot.memEnd)
            Logger::Log(MULTIBUS_LOG_PREFIX, std::format("Memory range is {:x}-{:x}", slot.memStart, slot.memEnd).c_str(), LogChannels::Debug);
        
        if (slot.ioStart || slot.ioEnd)
            Logger::Log(MULTIBUS_LOG_PREFIX, std::format("I/O range is {:x}-{:x}", slot.ioStart, slot.ioEnd).c_str(), LogChannels::Debug);

        Logger::Log(MULTIBUS_LOG_PREFIX, std::format("Slot is {}, component is {}", slot.id + 1, slot.component->GetName()).c_str(), LogChannels::Debug);

        slotMappings.push_back(slot);

        return true; 
    }   

    // Nothing on the backplane claimed this address, so the IP2 answers for it itself: the first megabyte of Multibus memory is a window onto system RAM.
    void Multibus::LogUnmapped(const char* what, size_t addr, bool isWrite, uint32_t value)
    {
        if (unmappedLogged >= MULTIBUS_MAX_UNMAPPED_LOGGED)
            return;

        unmappedLogged++;

        std::string tail = (unmappedLogged == MULTIBUS_MAX_UNMAPPED_LOGGED)
            ? " - further unmapped Multibus accesses will not be logged" : "";

        if (isWrite)
            Logger::Log(MULTIBUS_LOG_PREFIX, std::format("Multibus::{}: Unmapped Multibus write of 0x{:x} to 0x{:x}{}",
                what, value, addr, tail).c_str(), LogChannels::Warning);
        else
            Logger::Log(MULTIBUS_LOG_PREFIX, std::format("Multibus::{}: Unmapped Multibus read from 0x{:x}{}",
                what, addr, tail).c_str(), LogChannels::Warning);
    }

    Multibus::SlaveTarget Multibus::DecodeSlave(size_t addr, size_t* target)
    {
        if (addr < MULTIBUS_MEMORY_START || addr > MULTIBUS_MEMORY_END)
            return SlaveTarget::None;

        size_t busAddr = addr - MULTIBUS_MEMORY_START;

        if (busAddr <= MULTIBUS_SLAVE_WINDOW_END)
        {
            if (!memory)
                return SlaveTarget::None;


            size_t entry = busAddr >> MULTIBUS_SLAVE_PAGE_SHIFT;

            *target = ((size_t)(slaveMap[entry] & MULTIBUS_SLAVE_FRAME_MASK) << MULTIBUS_SLAVE_PAGE_SHIFT)
                | (busAddr & MULTIBUS_SLAVE_PAGE_MASK);

            return SlaveTarget::Ram;
        }

        if (busAddr <= MULTIBUS_SLAVE_MAP_END)
        {
            // every address inside a 4KB block selects the same entry
            *target = (busAddr - MULTIBUS_SLAVE_MAP_START) >> MULTIBUS_SLAVE_PAGE_SHIFT;
            return SlaveTarget::Map;
        }

        return SlaveTarget::None;
    }

    uint8_t Multibus::Read8(size_t addr) 
    {
        if (!UseCachedReadSlot(addr))
            if (!SetCachedReadMapping(addr))
            {
                size_t target = 0;

                switch (DecodeSlave(addr, &target))
                {
                    case SlaveTarget::Ram:
                        return memory->Read8(target);
                    case SlaveTarget::Map:
                        return (uint8_t)((addr & 1) ? (slaveMap[target] & 0xFF) : (slaveMap[target] >> 8));
                    default:
                        break;
                }

                AddrSpace::SignalFault(addr, false);

                LogUnmapped("Read8", addr, false, 0);
                return 0x00;
            }

            return lastSlotRead->component->Read8(addr);
    }

    uint16_t Multibus::Read16(size_t addr)
    {
        if (!UseCachedReadSlot(addr))
            if (!SetCachedReadMapping(addr))
            {
                size_t target = 0;

                switch (DecodeSlave(addr, &target))
                {
                    case SlaveTarget::Ram:
                        return memory->Read16(target);
                    case SlaveTarget::Map:
                        return slaveMap[target];
                    default:
                        break;
                }

                AddrSpace::SignalFault(addr, false);

                LogUnmapped("Read16", addr, false, 0);
                return 0x00;
            }

        return lastSlotRead->component->Read16(addr);
    }

    uint32_t Multibus::Read32(size_t addr) 
    {
        if (!UseCachedReadSlot(addr))
            if (!SetCachedReadMapping(addr))
            {
                size_t target = 0;

                switch (DecodeSlave(addr, &target))
                {
                    case SlaveTarget::Ram:
                        return memory->Read32(target);
                    case SlaveTarget::Map:
                        return (uint32_t)(slaveMap[target] << 16 | slaveMap[target]);
                    default:
                        break;
                }

                AddrSpace::SignalFault(addr, false);

                LogUnmapped("Read32", addr, false, 0);
                return 0x00;
            }

        return lastSlotRead->component->Read32(addr);
    }

    void Multibus::Write8(size_t addr, uint8_t value) 
    {
        if (!UseCachedWriteSlot(addr))
            if (!SetCachedWriteMapping(addr))
            {
                size_t target = 0;

                switch (DecodeSlave(addr, &target))
                {
                    case SlaveTarget::Ram:
                        memory->Write8(target, value);
                        return;
                    case SlaveTarget::Map:
                        slaveMap[target] = (addr & 1)
                            ? (uint16_t)((slaveMap[target] & 0xFF00) | value)
                            : (uint16_t)((slaveMap[target] & 0x00FF) | (value << 8));
                        return;
                    default:
                        break;
                }

                AddrSpace::SignalFault(addr, true);

                LogUnmapped("Write8", addr, true, value);
                return;
            }
            
        lastSlotWritten->component->Write8(addr, value);
    }

    void Multibus::Write16(size_t addr, uint16_t value)
    {
        if (!UseCachedWriteSlot(addr))
            if (!SetCachedWriteMapping(addr))
            {
                size_t target = 0;

                switch (DecodeSlave(addr, &target))
                {
                    case SlaveTarget::Ram:
                        memory->Write16(target, value);
                        return;
                    case SlaveTarget::Map:
                        slaveMap[target] = value;
                        return;
                    default:
                        break;
                }

                AddrSpace::SignalFault(addr, true);

                LogUnmapped("Write16", addr, true, value);
                return;
            }

        lastSlotWritten->component->Write16(addr, value);
    }
    
    void Multibus::Write32(size_t addr, uint32_t value)
    {
        if (!UseCachedWriteSlot(addr))
            if (!SetCachedWriteMapping(addr))
            {
                size_t target = 0;

                switch (DecodeSlave(addr, &target))
                {
                    case SlaveTarget::Ram:
                        memory->Write32(target, value);
                        return;
                    case SlaveTarget::Map:
                        slaveMap[target] = (uint16_t)value;
                        return;
                    default:
                        break;
                }

                AddrSpace::SignalFault(addr, true);

                LogUnmapped("Write32", addr, true, value);
                return;
            }

        lastSlotWritten->component->Write32(addr, value);       
    }

    void Multibus::Shutdown()
    {
        cpu = nullptr;
        lastSlotRead = lastSlotWritten = nullptr; 
        slotMappings.clear();
    }
}; 