#pragma once

#include <Motion.hpp>
#include <component/component.hpp>
#include <component/cpu/cpu.hpp>
#include <base/emulation.hpp>
#include <component/cpu/mc68020_moira_bridge.hpp>
#include <component/ip2/ip2_mmu.hpp>

namespace Motion
{
    extern Cvar* logCpuTrace;

    #define MOIRA_DISASM_BUF_SIZE   512
    #define LOG_PREFIX_68020        "68020 CPU"

    // Bring-up instrumentation, see TraceUnmapped in mc68020_core.cpp.
    #define PC_TRACE_SIZE           (1 << 18)
    #define PC_TRACE_MAX_DUMPS      4
    #define PC_TRACE_SAMPLE_EVERY   5000000
    #define PC_TRACE_FATAL_RAW_PCS  48

    // A fault storm would otherwise fill the log.
    #define MC68020_MAX_ESCAPED_EXCEPTIONS  16

    /// @brief Debugger extensions for Lisburn
    /// if it deals with the debugger it goes in here. if it doesn't it doesn't 
    class MC68020DebuggerSystem : public CoherentSystem
    {

    public:
        char* DisasmInstruction(size_t start) override;
        size_t GetPC() override;

        /// @brief Initialise the MC68020 Debugger System
        MC68020DebuggerSystem(MC68020MoiraBridge* bridge)
        {
            this->moiraCpu = bridge;
        }

        /// @brief for the coherent stack window
        uint32_t GetStack32(uint32_t offset) override
        {
            AddrSpacePeek peek;
            return AddrSpace::ReadU32(moiraCpu->reg.sp + (offset << 2));
        };

    private: 
        char disasmBuf[MOIRA_DISASM_BUF_SIZE] = {0};
        MC68020MoiraBridge* moiraCpu;
    };

    class MC68020 : public ComponentCPU
    {
    public: 

        MC68020MoiraBridge moiraCpu;
        // MMU & FPU are off-chip. 68881 + 68882.
        
        //
        // METHODS
        // 

        // rc16 uses this clock speed. sdhould be switchable eventually
        uint32_t GetClockSpeed() override { return 16666670; }; 

        void Start() override;
        void Tick() override;
        void Shutdown() override; 

        // other stuff may be dependent on the CPU, so start it first
        bool IsEarlyStart() override { return true; };

        /// @brief get the name of this component. immutable const char*.
        const char* GetName() override { return "Motorola MC68020 CPU (Lisburn)"; };

        // not sure if this is right
        void SetIRQLine(int32_t irq) override { moiraCpu.setIPL(irq); };

        /// @brief returns a boolean indicating if this cpu is in privileged mode. most cpus only have two levels of privilege and x86 has 4, but 2 are almost never used.
        /// @return a boolean indicating if the cpu is in privileged mode
        bool IsPrivilegedMode() override { return (moiraCpu.getSR() & 0x2000); }; // bit 13 is sr

        uint32_t GetProgramCounter() override { return moiraCpu.getPC(); };

    private:
        MC68020DebuggerSystem* system; 
        bool traceEnabled = false;
        int32_t escapedExceptions = 0;
    };
}