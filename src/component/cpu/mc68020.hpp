#pragma once

#include <Motion.hpp>
#include <component/component.hpp>
#include <component/cpu/cpu.hpp>
#include <base/emulation.hpp>
#include <component/cpu/mc68020_moira_bridge.hpp>

namespace Motion
{
    #define MOIRA_DISASM_BUF_SIZE   512
    #define LOG_PREFIX_68020        "68020 CPU"

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
        void Reset() override;
        void Tick() override;
        void Shutdown() override; 

        // Reset vectors must be read after the memory and PROM mappings exist.
        bool IsEarlyStart() override { return false; };

        /// @brief get the name of this component. immutable const char*.
        const char* GetName() override { return "Motorola MC68020 CPU (Lisburn)"; };

        // not sure if this is right
        void SetIRQLine(int32_t irq) override { moiraCpu.setIPL(irq); };

        /// @brief returns a boolean indicating if this cpu is in privileged mode. most cpus only have two levels of privilege and x86 has 4, but 2 are almost never used.
        /// @return a boolean indicating if the cpu is in privileged mode
        bool IsPrivilegedMode() override { return (moiraCpu.getSR() & 0x2000); }; // bit 13 is sr

    private:
        MC68020DebuggerSystem* system; 
    };
}