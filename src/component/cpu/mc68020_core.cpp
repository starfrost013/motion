#include <iostream>
#include <component/cpu/mc68020.hpp>

namespace Motion
{
    Cvar* logCpuTrace;

    // defined below Start, which installs them
    static MC68020* tracedCpu;
    static void TraceUnmapped(size_t addr, bool isWrite, int32_t width);
    static void TraceFatalUserFault(size_t addr, bool isWrite);

    void MC68020::Start()
    {
        // ensure we are in reset so e.g. the MMUs don't try and map everything
        isInReset = true;
        AddrSpace::SetFaultsEnabled(false);

        Logger::Log(LOG_PREFIX_68020, "*yawn* I'm a Motorola 68020!", LogChannels::Debug);

        moiraCpu.setModel(Motion::Lisburn::Model::M68020);
        moiraCpu.UseVectoredInterrupts();

        system = new MC68020DebuggerSystem(&moiraCpu);
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.d[0], "d0"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.d[1], "d1"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.d[2], "d2"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.d[3], "d3"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.d[4], "d4"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.d[5], "d5"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.d[6], "d6"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.d[7], "d7"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.a[0], "a0"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.a[1], "a1"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.a[2], "a2"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.a[3], "a3"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.a[4], "a4"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.a[5], "a5"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.a[6], "a6"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.a[7], "sp [a7]"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.pc, "pc"));
        // NEED TO BE some changes to MOIRA for these ones
        //system->AddRegister(new CoherentSystem::Register<uint8_t>(&this->moiraCpu.reg), "ccr"); // flags (technically lower byte of status)
        //system->AddRegister(new CoherentSystem::Register<uint8_t>(&this->moiraCpu.reg.sr.), "status"); 
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.usp, "usp"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.msp, "msp"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.isp, "isp"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.sfc, "sfc"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.dfc, "dfc"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.vbr, "vbr"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.cacr, "cacr"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.caar, "caar"));
        //system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.sr), "sr");

        // set the word size
        system->SetWordSize(CoherentSystem::WordSize::WordSize32);

        Coherent::SetSystem(system);
        // convert to nanoseconds

        Logger::Log(LOG_PREFIX_68020, "Resetting CPU...");
        moiraCpu.reset();

        // moira has a didReset delegate, but due to various design reasons (mostly include cycles) we can't use it 
        isInReset = false;
        AddrSpace::SetFaultsEnabled(true);

        // Off by default because recording a PC per instruction is not free.
        logCpuTrace = Cvar::Get("logCpuTrace", "0");
        traceEnabled = logCpuTrace->GetValue();
        MC68020MoiraBridge::traceExceptions = traceEnabled;

        if (traceEnabled)
        {
            tracedCpu = this;
            AddrSpace::unmappedHook = TraceUnmapped;
            MC68020MoiraBridge::fatalUserFaultHook = TraceFatalUserFault;
        }
    }

    // Bring-up instrumentation for the kernel boot.
    static uint32_t tracePcs[PC_TRACE_SIZE] = {0};
    static uint64_t traceCount = 0;
    static int32_t traceDumps = 0;
    static bool traceKernelSeen = false;

    // Registers, the top of the stack and the control flow that led here.
    static void TraceFaultState(int32_t rawPcs)
    {
        auto& cpu = tracedCpu->moiraCpu;

        for (int32_t i = 0; i < 8; i++)
        {
            Logger::Log(LOG_PREFIX_68020, std::format("  d{} 0x{:08x}   a{} 0x{:08x}",
                i, cpu.reg.d[i], i, cpu.reg.a[i]).c_str(), LogChannels::Warning);
        }

        // Both sides of sp - arguments above, the new frame below - behind a peek, so reporting a fault cannot raise one.
        {
            AddrSpacePeek peek;
            uint32_t sp = cpu.reg.a[7];

            for (int32_t i = -2; i < 10; i++)
                Logger::Log(LOG_PREFIX_68020, std::format("  [sp{}0x{:02x}] 0x{:08x}", i < 0 ? "-" : "+",
                    (i < 0 ? -i : i) * 4, AddrSpace::ReadU32(sp + (i * 4))).c_str(), LogChannels::Warning);
        }

        // Printing every PC is useless - a copy loop fills the whole window.
        uint64_t total = (traceCount < PC_TRACE_SIZE) ? traceCount : PC_TRACE_SIZE;
        std::string line;
        uint32_t lastFrom = 0, lastTo = 0;
        int32_t run = 0;

        auto flush = [&]()
        {
            if (!run)
                return;

            line += std::format("{:x}>{:x}", lastFrom, lastTo);

            if (run > 1)
                line += std::format("(x{})", run);

            line += " ";

            if (line.length() > 190)
            {
                Logger::Log(LOG_PREFIX_68020, std::format("  flow: {}", line).c_str(), LogChannels::Warning);
                line.clear();
            }
        };

        for (uint64_t i = 1; i < total; i++)
        {
            uint32_t prev = tracePcs[(traceCount - total + i - 1) % PC_TRACE_SIZE];
            uint32_t cur = tracePcs[(traceCount - total + i) % PC_TRACE_SIZE];

            // a straight-line 68020 instruction is at most 10ish bytes, so further than that, or backwards, is a taken branch
            if (cur >= prev && cur - prev <= 12)
                continue;

            if (prev == lastFrom && cur == lastTo)
            {
                run++;
                continue;
            }

            flush();
            lastFrom = prev;
            lastTo = cur;
            run = 1;
        }

        flush();

        if (!line.empty())
            Logger::Log(LOG_PREFIX_68020, std::format("  flow: {}", line).c_str(), LogChannels::Warning);

        if (rawPcs > 0)
        {
            std::string pcs;
            int32_t want = (int32_t)((total < (uint64_t)rawPcs) ? total : (uint64_t)rawPcs);

            for (int32_t i = want; i > 0; i--)
                pcs += std::format("{:x} ", tracePcs[(traceCount - i) % PC_TRACE_SIZE]);

            Logger::Log(LOG_PREFIX_68020, std::format("  last {} pcs: {}", want, pcs).c_str(), LogChannels::Warning);
        }
    }

    static void TraceUnmapped(size_t addr, bool isWrite, int32_t width)
    {
        if (!tracedCpu || traceDumps >= PC_TRACE_MAX_DUMPS)
            return;

        uint32_t pc = tracedCpu->moiraCpu.getPC();

        // PROM memory sizing and the absent FPA are both expected; everything else is worth a look.
        if (addr >= MMU_SEGMENT_FPA)
            return;

        if (pc >= MMU_SEGMENT_SYSTEM && pc < MMU_SEGMENT_MULTIBUS_MEMORY && addr < ADDRSPACE_DEVICE_SPACE_START)
            return;

        traceDumps++;

        Logger::Log(LOG_PREFIX_68020, std::format("--- unmapped {}{} of 0x{:x} at pc 0x{:x} (sr 0x{:04x}, vbr 0x{:x}) ---",
            isWrite ? "write" : "read", width, addr, pc, tracedCpu->moiraCpu.getSR(),
            tracedCpu->moiraCpu.reg.vbr).c_str(), LogChannels::Warning);

        TraceFaultState(0);
    }

    // A user mode access to the null guard page is the fault that is about to become SIGSEGV - every other user fault is demand paging doing its job.
    static void TraceFatalUserFault(size_t addr, bool isWrite)
    {
        if (!tracedCpu || traceDumps >= PC_TRACE_MAX_DUMPS)
            return;

        traceDumps++;

        auto& cpu = tracedCpu->moiraCpu;

        Logger::Log(LOG_PREFIX_68020, std::format("--- fatal user fault: {} of 0x{:x} at pc 0x{:x} (sr 0x{:04x}) ---",
            isWrite ? "write" : "read", addr, cpu.getPC0(), cpu.getSR()).c_str(), LogChannels::Warning);

        TraceFaultState(PC_TRACE_FATAL_RAW_PCS);
    }

    // One-shot: the first time the CPU executes out of the kernel segment, show what the kernel segment actually resolves to.
    static void TraceKernelEntry(uint32_t pc)
    {
        IP2MMU* mmu = Emulation::GetMachine()->FindComponentByType<IP2MMU>();

        if (!mmu)
            return;

        Logger::Log(LOG_PREFIX_68020, std::format("--- first kernel-segment execution at 0x{:x} ---", pc).c_str(), LogChannels::Warning);
        Logger::Log(LOG_PREFIX_68020, std::format("  osBase 0x{:x}  textdata base 0x{:x} limit 0x{:x}  stack base 0x{:x} limit 0x{:x}",
            mmu->osBase, mmu->textdataBase, mmu->textdataLimit, mmu->stackBase, mmu->stackLimit).c_str(), LogChannels::Warning);

        for (uint32_t va = 0x20000000; va <= 0x20008000; va += 0x1000)
        {
            size_t phys = 0;
            uint32_t pte = mmu->pagetable[(mmu->osBase + ((va >> 12) & PAGETABLE_PAGE_MASK)) & (PAGETABLE_MAX_PAGES - 1)];

            if (!mmu->Translate(va, &phys, false))
            {
                Logger::Log(LOG_PREFIX_68020, std::format("  0x{:08x} -> FAULT (pte 0x{:08x})", va, pte).c_str(), LogChannels::Warning);
                continue;
            }

            Logger::Log(LOG_PREFIX_68020, std::format("  0x{:08x} -> phys 0x{:06x} (pte 0x{:08x})  first words {:08x} {:08x}",
                va, phys, pte, AddrSpace::ReadU32(va), AddrSpace::ReadU32(va + 4)).c_str(), LogChannels::Warning);
        }
    }

    void MC68020::Tick()
    {
        if (traceEnabled)
        {
            uint32_t pc = moiraCpu.getPC();

            if (!traceKernelSeen && pc >= MMU_SEGMENT_KERNEL && pc < MMU_SEGMENT_SYSTEM)
            {
                traceKernelSeen = true;
                TraceKernelEntry(pc);
            }

            tracePcs[traceCount++ % PC_TRACE_SIZE] = pc;

            // A periodic sample is the cheapest way to tell a quiet but progressing kernel from a hung one.
            if (traceKernelSeen && !(traceCount % PC_TRACE_SAMPLE_EVERY))
                Logger::Log(LOG_PREFIX_68020, std::format("sample: {}M instructions, pc 0x{:x}, sp 0x{:x}, a0 0x{:x} a1 0x{:x} a2 0x{:x} a3 0x{:x} a4 0x{:x} a5 0x{:x} d0 0x{:x}",
                    traceCount / 1000000, pc, moiraCpu.reg.a[7], moiraCpu.reg.a[0], moiraCpu.reg.a[1],
                    moiraCpu.reg.a[2], moiraCpu.reg.a[3], moiraCpu.reg.a[4], moiraCpu.reg.a[5], moiraCpu.reg.d[0]).c_str(), LogChannels::Warning);
        }

        try
        {
            moiraCpu.execute();
        }
        catch (const std::exception& exc)
        {
            // Moira's processException rethrows anything that is not an address error or a bus error, and a double fault escapes it too because it is thrown as a.
            if (escapedExceptions < MC68020_MAX_ESCAPED_EXCEPTIONS)
            {
                escapedExceptions++;

                Logger::Log(LOG_PREFIX_68020, std::format("CPU exception escaped the core at pc 0x{:x}: {}{}",
                    moiraCpu.getPC(), exc.what(),
                    (escapedExceptions == MC68020_MAX_ESCAPED_EXCEPTIONS) ? " - further ones will not be logged" : "").c_str(),
                    LogChannels::Error);
            }
        }
    }
    
    void MC68020::Shutdown()
    {
        delete system;
        Coherent::SetSystem(nullptr);
    }
}