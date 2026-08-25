#include <component/addrspace.hpp>
#include <base/emulation.hpp>

namespace Motion
{
    // Hot: every access goes through here. The cache is checked before its own bounds, so a stale one is harmless.
    AddrSpaceMapping* AddrSpace::GetMapping(size_t addr)
    {
        if (lastMapping
        && addr >= lastMapping->startAddr
        && addr <= lastMapping->endAddr)
            return lastMapping;

        for (auto& entry : mappings)
        {
            if (addr >= entry.second.startAddr
            && addr <= entry.second.endAddr)
                return lastMapping = &entry.second;
        }

        return nullptr;
    }

    void AddrSpace::LogUnmapped(const char* what, size_t addr, bool isWrite, uint32_t value)
    {
        if (peekDepth || unmappedLogged >= ADDRSPACE_MAX_UNMAPPED_LOGGED)
            return;

        unmappedLogged++;

        std::string tail = (unmappedLogged == ADDRSPACE_MAX_UNMAPPED_LOGGED)
            ? " - further unmapped accesses will not be logged" : "";

        if (isWrite)
            Logger::Log(LOG_PREFIX_MAPPING, std::format("AddrSpace::{} - Unmapped write of 0x{:x} to 0x{:x}!{}",
                what, value, addr, tail).c_str(), LogChannels::Warning);
        else
            Logger::Log(LOG_PREFIX_MAPPING, std::format("AddrSpace::{} - Unmapped read from 0x{:x}!{}",
                what, addr, tail).c_str(), LogChannels::Warning);
    }

    uint8_t AddrSpace::ReadU8(size_t addr)
    {
        size_t physAddr = addr;

        if (mmu)
        {
            if (!mmu->Translate(addr, &physAddr, false))
            {
                SignalFault(addr, false);
                return 0xFF;
            }
        }

        AddrSpaceMapping* mapping = GetMapping(physAddr);

        if (mapping)
        {
            return mapping->component->Read8(physAddr);
        }
        else
        {
            SignalFaultIfDeviceSpace(physAddr, false);
            NotifyUnmapped(physAddr, false, 8);

            LogUnmapped("ReadU8", physAddr, false, 0);
            return 0;
        }
    }
    
    uint16_t AddrSpace::ReadU16(size_t addr)
    {
        size_t physAddr = addr;

        if (mmu)
        {
            if (!mmu->Translate(addr, &physAddr, false))
            {
                SignalFault(addr, false);
                return 0xFF;
            }
        }

        AddrSpaceMapping* mapping = GetMapping(physAddr);

        if (mapping)
        {
            auto value = mapping->component->Read16(physAddr);
            // IRIS is a big-endian system

            return value;
        }
        else
        {
            SignalFaultIfDeviceSpace(physAddr, false);
            NotifyUnmapped(physAddr, false, 16);

            LogUnmapped("ReadU16", physAddr, false, 0);
            return 0;
        }
    }
    
    uint32_t AddrSpace::ReadU32(size_t addr)
    {
        size_t physAddr = addr;

        if (mmu)
        {
            if (!mmu->Translate(addr, &physAddr, false))
            {
                SignalFault(addr, false);
                return 0xFF;
            }
        }

        AddrSpaceMapping* mapping = GetMapping(physAddr);

        if (mapping)
        {
            // IRIS is a big-endian system
            auto value = mapping->component->Read32(physAddr);

            return value;
        }
        else
        {
            SignalFaultIfDeviceSpace(physAddr, false);
            NotifyUnmapped(physAddr, false, 32);

            LogUnmapped("ReadU32", physAddr, false, 0);
            return 0;
        }
    }
    
    int8_t AddrSpace::ReadS8(size_t addr)
    {
        return (int8_t)ReadU8(addr);
    }
    
    int16_t AddrSpace::ReadS16(size_t addr)
    {
        return (int16_t)ReadU16(addr);
    }
    
    int32_t AddrSpace::ReadS32(size_t addr)
    {
        return (int32_t)ReadU32(addr);
    }

    void AddrSpace::AddMapping(AddrSpaceMapping mapping)
    {
        // this seems like a good place to put this
        // paranoid assumption of the mmu location
        // BAD CODE HACK

        if (mapping.startAddr > mapping.endAddr)
        {
            Logger::Log(LOG_PREFIX_MAPPING, "AddrSpace::AddMapping - mapping.StartAddr > mapping.endAddr", LogChannels::Error);
            return;
        }

        auto mappingCount = mappings.count(mapping.startAddr);

        if (mappingCount > 1)
        {
            Logger::Log(LOG_PREFIX_MAPPING, "AddrSpace::AddMapping - mapping already exists", LogChannels::Error);
            return;
        }

        if (!mapping.component)
        {
            Logger::Log(LOG_PREFIX_MAPPING, "AddrSpace::AddMapping - mapping doesn't have an attached component!", LogChannels::Error);
            return; 
        }

        Logger::Log(LOG_PREFIX_MAPPING, std::format("Added address mapping from 0x{:x} to 0x{:x} (size 0x{:x}) for component {}",
            mapping.startAddr, mapping.endAddr, (mapping.endAddr - mapping.startAddr), mapping.component->GetName()).c_str(), LogChannels::Debug);
        
        mappings[mapping.startAddr] = mapping;
    }

    void AddrSpace::WriteU8(size_t addr, uint8_t value)
    {
        size_t physAddr = addr;

        if (mmu)
        {
            if (!mmu->Translate(addr, &physAddr, true))
            {
                SignalFault(addr, true);
                return;
            }
        }

        AddrSpaceMapping* mapping = GetMapping(physAddr);

        if (mapping)
        {
            return mapping->component->Write8(physAddr, value);
        }
        else
        {
            SignalFaultIfDeviceSpace(physAddr, true);
            NotifyUnmapped(physAddr, true, 8);

            LogUnmapped("WriteU8", physAddr, true, value);
        }
    }

    void AddrSpace::WriteU16(size_t addr, uint16_t value)
    {
        size_t physAddr = addr;

        if (mmu)
        {
            if (!mmu->Translate(addr, &physAddr, true))
            {
                SignalFault(addr, true);
                return;
            }
        }

        AddrSpaceMapping* mapping = GetMapping(physAddr);

        if (mapping)
        {
            // IRIS is a big-endian system
            return mapping->component->Write16(physAddr, value);
        }
        else
        {
            SignalFaultIfDeviceSpace(physAddr, true);
            NotifyUnmapped(physAddr, true, 16);

            LogUnmapped("WriteU16", physAddr, true, value);
        }
    }

    void AddrSpace::WriteU32(size_t addr, uint32_t value)
    {
        size_t physAddr = addr;

        if (mmu)
        {
            if (!mmu->Translate(addr, &physAddr, true))
            {
                SignalFault(addr, true);
                return;
            }
        }

        AddrSpaceMapping* mapping = GetMapping(physAddr);

        if (mapping)
        {
            return mapping->component->Write32(physAddr, value);
        }
        else
        {
            SignalFaultIfDeviceSpace(physAddr, true);
            NotifyUnmapped(physAddr, true, 32);

            LogUnmapped("WriteU32", physAddr, true, value);
        }
    }

    void AddrSpace::WriteS8(size_t addr, int8_t value)
    {
        WriteU8(addr, (uint8_t)value);
    }

    void AddrSpace::WriteS16(size_t addr, int16_t value)
    {
        WriteU16(addr, (uint16_t)value);
    }

    void AddrSpace::WriteS32(size_t addr, int32_t value)
    {
        WriteU32(addr, (uint32_t)value);
    }

    // A hole in device space means nothing drove DSACK, the cycle times out and BERR is asserted.
    void AddrSpace::SignalFaultIfDeviceSpace(size_t addr, bool isWrite)
    {
        if (addr < ADDRSPACE_DEVICE_SPACE_START)
            return;

        SignalFault(addr, isWrite);
    }

    AddrSpacePeek::AddrSpacePeek() { AddrSpace::PushPeek(); }
    AddrSpacePeek::~AddrSpacePeek() { AddrSpace::PopPeek(); }

    void AddrSpace::SignalFault(size_t addr, bool isWrite)
    {
        if (!faultsEnabled || peekDepth)
            return;

        faultPending = true;
        faultAddress = addr;
        faultWasWrite = isWrite;
    }

    bool AddrSpace::TakeFault(size_t* addr, bool* isWrite)
    {
        if (!faultPending)
            return false;

        if (addr)
            *addr = faultAddress;

        if (isWrite)
            *isWrite = faultWasWrite;

        faultPending = false;
        return true;
    }

    void AddrSpace::RegisterMMU(ComponentMMU* mmu)
    {
        if (mmu)
            Logger::Log(LOG_PREFIX_MAPPING, std::format("Addressing system registered an MMU: {}", mmu->GetName()).c_str(), LogChannels::Debug);
        AddrSpace::mmu = mmu;
    }

    void AddrSpace::Shutdown()
    {
        // in case the bozo user forgot to actually shut down the mmu. it will be deleted anywya but then we will have a stale pointer.
        mmu = nullptr; 
        mappings.clear();
    }
}