#pragma once

#include <array>
#include <cstdint>
#include <string>

#include <AtariStudio/Cpu6502/Instruction.h>
#include <AtariStudio/Cpu6502/Opcode.h>

namespace atari
{

    struct DisassembledInstruction
    {
        uint16_t address = 0;

        cpu6502::Opcode opcode = 0;

        cpu6502::Instruction instruction =
            cpu6502::Instruction::Illegal;

        cpu6502::AddressMode addressMode =
            cpu6502::AddressMode::Implied;

        std::array<uint8_t, 3> bytes{};

        uint8_t length = 0;

        std::string mnemonic;

        std::string operand;

        std::string text;
    };

}