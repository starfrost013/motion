/*
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    ip2_mmu.cpp: Implements the MMU sitting on the IP2 board
*/

#include <component/ip2/ip2_mmu.hpp>

namespace Motion
{
    Cvar* logIP2MMU; 

    // all of this code should be macroised or turned into  utility functions in /platform/util

    uint8_t IP2MMU::Read8(size_t addr)
    {
        if (addr & 1)
            return Read16(addr) & 0xFF;
        else
            return Read16(addr) >> 8;
    }

    uint16_t IP2MMU::Read16(size_t addr)
    {
        uint32_t ret = 0xFF;

        switch (addr)
        {
        case REG_OS_BASE:
            ret = osBase >> 8;
            break;
        case REG_STATUS:
            ret = status;
            break;
        case REG_PARITY:
            ret = parity;
            break;
        case REG_MULTIBUS_PROTECT:
            ret = multibusProtect;
            break;
        case REG_PAGETABLE_BASE ... PAGETABLE_INDEX(PAGETABLE_MAX_PAGES) - 1:
            if (addr & 2)
                ret = pagetable[(addr - REG_PAGETABLE_BASE) >> 2] & 0x0000FFFF;
            else
                ret = (pagetable[(addr - REG_PAGETABLE_BASE) >> 2] & 0xFFFF0000) >> 16;
            break;
        case REG_TEXTDATA_BASE:
            ret = textdataBase;
            break;
        case REG_TEXTDATA_LIMIT:
            ret = textdataLimit;
            break;
        case REG_STACK_BASE:
            ret = stackBase;
            break;
        case REG_STACK_LIMIT:
            ret = stackLimit;
            break;
        default:
            Logger::Log(LOG_PREFIX_IP2MMU, std::format("Invalid IP2 MMU read at address 0x{:x}", addr).c_str(), LogChannels::Warning);
            ret = 0xFF;
            return ret;
        }

        if (logEnabled)
            Logger::Log(LOG_PREFIX_IP2MMU, std::format("IP2 MMU read 0x{:x} from address 0x{:x} (check debug window)", ret, addr).c_str(), MMU_LOG_CHANNEL_NAME);
        
        return ret;
    }

    uint32_t IP2MMU::Read32(size_t addr)
    {
        return (Read16(addr) << 16
        | Read16(addr + 2));
    }

    void IP2MMU::Write8(size_t addr, uint8_t value)
    {
        uint16_t val = Read16(addr);

        // big endian
        if (addr & 1)
            val = (val & 0xFF00) | value;
        else
            val = (val & 0x00FF) | (value << 8);

        Write16(addr, val);
    }

    void IP2MMU::Write16(size_t addr, uint16_t value)
    {
        size_t index = 0;

        switch (addr)
        {
            // this is an 8-bit register but it is easier becasue of how the MMU is designed to treat it as a 16 bit register.
            case REG_OS_BASE:
                osBase = (value >> 8) << 8;
                break;
            case REG_STATUS:
                status = value;

                // ST_ENABINT is the master interrupt enable; the register lives here but the logic it gates does not.
                if (!interrupts)
                    interrupts = Emulation::GetMachine()->FindComponentByType<IP2Interrupt>();

                if (interrupts)
                    interrupts->SetEnabled(value & MMU_STATUS_ENABLE_INTERRUPTS);

                break;
            case REG_PARITY:
                parity = value;
                break;
            case REG_MULTIBUS_PROTECT:
                multibusProtect = value;
                break;       
            case REG_PAGETABLE_BASE ... PAGETABLE_INDEX(PAGETABLE_MAX_PAGES) - 1:
                index = (addr - REG_PAGETABLE_BASE) >> 2;
        
                if (addr & 2)
                    pagetable[index] = (pagetable[index] & 0xFFFF0000) | (value);
                else
                    pagetable[index] = (pagetable[index] & 0x0000FFFF) | ((uint32_t)(value) << 16);

                break;
            case REG_TEXTDATA_BASE:
                textdataBase = value;
                break;
            case REG_TEXTDATA_LIMIT:
                textdataLimit = value;
                break;
            case REG_STACK_BASE:
                stackBase = value;
                break;
            case REG_STACK_LIMIT:
                stackLimit = value;
                break;
            default:
                Logger::Log(LOG_PREFIX_IP2MMU, std::format("Invalid IP2 MMU write at address 0x{:x}", addr).c_str(), LogChannels::Warning);
                return;
        }

        if (logEnabled)
            Logger::Log(LOG_PREFIX_IP2MMU, std::format("IP2 MMU write 0x{:x} to address 0x{:x} (check debug window)", value, addr).c_str(), MMU_LOG_CHANNEL_NAME);   
    }

    void IP2MMU::Write32(size_t addr, uint32_t value)
    {
        Write16(addr, value >> 16);
        Write16(addr + 2, value);
    }

    //
    // The actual MMU parts
    //

    bool IP2MMU::Translate(size_t addr, size_t* finalAddress, bool isWrite)
    {
        // IN order to minimise overhead, only get the CPU once (otherwise, everything would be massively slowed down)
        // ensure it irght here
        if (!cpu)
            cpu = Emulation::GetMachine()->FindComponentByType<ComponentCPU>();

        // don't map in reset
        if (cpu->GetIsInReset())
        {
            *finalAddress = addr;
            return true; 
        }
        
        uint8_t segment = MMU_SEGMENT_GET_ID(addr);
     
        // MAME uses templates for this, which is a bit simpler, but seems a bit silly
        // since it generates several verisons of each method and is not really how the h/w does it
        // this is most likely how the real SGI TTL stuff is doing it
        uint16_t limitValue = 0, baseValue = 0, finalPageNumber = 0; 

        if (segment == MMU_SEGMENT_GET_ID(MMU_SEGMENT_TEXTDATA))
        {
            baseValue = textdataBase;
            limitValue = textdataLimit;
        }
        else if (segment == MMU_SEGMENT_GET_ID(MMU_SEGMENT_STACK))
        {
            baseValue = stackBase;
            limitValue = stackLimit;
        }
        else if (segment == MMU_SEGMENT_GET_ID(MMU_SEGMENT_KERNEL))
        {
            baseValue = osBase;
            limitValue = 0;             // 0 means no limit
        }
        else
        {
            /*
                Only the three RAM segments go through the page map. MAME's mem_map sends segment 3
                straight to sys_map and segments 4 and 5 straight out to the Multibus, and the IP2
                schematic agrees - the map RAM sits between the CPU and DRAM, not between the CPU and
                the backplane. Running segment 4 through the map with a base of zero silently
                redirected the PROM's Multibus accesses into whatever page table entry happened to
                match, which is how the Multibus map programming was going missing.
            */
            *finalAddress = addr;
            return true; 
        }
        
        uint16_t pageNumber = 0;
        
        // stack is mapped in reverse
        // MAME bit: "packed into a right-aligned field in the output.""
        if (segment == MMU_SEGMENT_GET_ID(MMU_SEGMENT_STACK))
        {
            // bits 23-12
            pageNumber = ((addr >> 12) & PAGETABLE_PAGE_MASK) ^ PAGETABLE_PAGE_MASK;
            finalPageNumber = baseValue - pageNumber;
        }
        else
        {
            pageNumber = (addr >> 12) & PAGETABLE_PAGE_MASK;
            finalPageNumber = baseValue + pageNumber;
        }

        bool limitReached = limitValue && pageNumber > limitValue;

        // base + page can index past the table, and must fault rather than be masked back: 0x4000 & 0x3fff is entry 0, a silent alias.
        if (finalPageNumber >= PAGETABLE_MAX_PAGES)
        {
            if (faultsLogged < IP2MMU_MAX_FAULTS_LOGGED)
            {
                faultsLogged++;

                Logger::Log(LOG_PREFIX_IP2MMU,
                    std::format("Bus error: {} of 0x{:x} indexes page table entry 0x{:x}, past the end of the table",
                    isWrite ? "write" : "read", addr, finalPageNumber).c_str(), LogChannels::Warning);
            }

            return false;
        }

        uint32_t& page = pagetable[finalPageNumber];

        bool busError = false;

        // read
        if (!isWrite)
        {
            if (limitReached 
                || !(page & MMU_MASK_IS_PROTECTED)
                || ((page & MMU_MASK_IS_PROTECTED) == MMU_MASK_SUPERVISOR_ONLY) && !(cpu->IsPrivilegedMode()))
            {
                // MAME's sgi_ip2_device conditions, matching the BERR PAL on sheet 16: limit, the two protection bits, WRITE and FC2.
                busError = true; 
            }

            if (!busError)
                page |= MMU_MASK_REFERENCED;
        }
        else
        {
            if (limitReached 
                || !(page & MMU_MASK_IS_PROTECTED)
                || (page & MMU_MASK_IS_PROTECTED) == MMU_MASK_READ_ONLY // cannot write to readonly 
                || ((page & MMU_MASK_IS_PROTECTED) == MMU_MASK_SUPERVISOR_ONLY) && !(cpu->IsPrivilegedMode()))
            {
                // MAME's sgi_ip2_device conditions, matching the BERR PAL on sheet 16: limit, the two protection bits, WRITE and FC2.
                busError = true; 
            }

            if (!busError)
                page |= (MMU_MASK_REFERENCED | MMU_MASK_MODIFIED);
        }

        if (busError)
        {
            // Not fatal - the CPU turns it into vector 2 and the OS deals with it. Rate limited so a storm cannot bury the log.
            if (faultsLogged < IP2MMU_MAX_FAULTS_LOGGED)
            {
                faultsLogged++;

                Logger::Log(LOG_PREFIX_IP2MMU,
                    std::format("Bus error: {} of unmapped page 0x{:x} (segment {}, pte index 0x{:x}, pte 0x{:08x}){}",
                    isWrite ? "write" : "read", addr, segment, finalPageNumber, page,
                    (faultsLogged == IP2MMU_MAX_FAULTS_LOGGED) ? " - further faults will not be logged" : "").c_str(),
                    LogChannels::Warning);
            }

            return false; 
        }
        
        // calculate a real physical ram address with 13...0 page adn teh bottom1 0 bits of the real address
        // 4KB pages, so the offset kept is 12 bits: a 13-bit mask ORs VA bit 12 into the frame number and dies once the kernel maps for real.
        *finalAddress = (page & PAGETABLE_FRAME_MASK) << 12 | (addr & 0xFFF);
        //Logger::Log(LOG_PREFIX_IP2MMU, std::format("Translated virtual address {:x} to physical address {:x}", addr, *finalAddress).c_str(), LogChannels::Debug);
        return true; 
    }
}