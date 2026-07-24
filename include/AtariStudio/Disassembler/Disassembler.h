#pragma once

#include <cstdint>

#include <AtariStudio/Disassembler/DisassembledInstruction.h>

namespace atari
{

    class Memory;

    ///
    /// Дизассемблер процессора MOS 6502.
    ///
    class Disassembler
    {
    public:

        Disassembler() = default;

        [[nodiscard]]
        DisassembledInstruction Decode(
            const Memory& memory,
            uint16_t address) const;

    private:

        [[nodiscard]]
        static const char* InstructionToString(
            cpu6502::Instruction instruction);

        [[nodiscard]]
        static std::string FormatOperand(
            cpu6502::AddressMode mode,
            uint16_t address,
            const uint8_t* bytes);
    };

} // namespace atari