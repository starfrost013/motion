#include <component/cpu/m68k/mc68020.hpp>

namespace Motion
{
    char* MC68020DebuggerSystem::DisasmInstruction(size_t start)
    {
        nextInstructionSize = moiraCpu->disassemble(disasmBuf, (uint32_t)start);
        return disasmBuf;
    }

    size_t MC68020DebuggerSystem::GetPC()
    {
        return moiraCpu->getPC();
    }    
}