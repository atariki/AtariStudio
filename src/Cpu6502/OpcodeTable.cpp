#include <AtariStudio/Cpu6502/OpcodeTable.h>

namespace atari::cpu6502
{

    namespace
    {

        constexpr InstructionInfo Illegal =
        {
            Instruction::Illegal,
            AddressMode::Implied,
            1,
            2,
            false,
            false
        };

    }

    const std::array<InstructionInfo, 256> OpcodeTable::m_table =
    {
        /* 00 */
        InstructionInfo{ Instruction::BRK, AddressMode::Implied,     1, 7, false, true },

        /* 01-FF */
        Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal,
        Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal,

        Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal,
        Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal,

        Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal,
        Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal,

        Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal,
        Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal,

        Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal,
        Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal,

        Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal,
        Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal,

        Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal,
        Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal,

        Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal,
        Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal,

        Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal,
        Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal,

        Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal,
        Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal,

        Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal,
        Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal,

        Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal,
        Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal,

        Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal,
        Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal,

        Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal,
        Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal,

        Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal,
        Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal,

        Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal,
        Illegal, Illegal, Illegal, Illegal, Illegal, Illegal, Illegal
    };

    const InstructionInfo& OpcodeTable::Get(Opcode opcode)
    {
        return m_table[opcode];
    }

} // namespace atari::cpu6502