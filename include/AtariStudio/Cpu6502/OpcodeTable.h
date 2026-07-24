#pragma once

#include <array>

#include <AtariStudio/Cpu6502/Instruction.h>
#include <AtariStudio/Cpu6502/Opcode.h>

namespace atari::cpu6502
{

    class OpcodeTable
    {
    public:

        /// Возвращает описание указанного опкода.
        static const InstructionInfo& Get(Opcode opcode);

    private:

        static const std::array<InstructionInfo, 256> m_table;
    };

} // namespace atari::cpu6502