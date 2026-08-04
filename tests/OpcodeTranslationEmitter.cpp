#include <AtariStudio/Core/Memory.h>
#include <AtariStudio/Cpu6502/Instruction.h>
#include <AtariStudio/Disassembler/Disassembler.h>
#include <AtariStudio/Disassembler/DisassemblyMetadata.h>
#include <AtariStudio/Disassembler/StructuredExpressionBuilder.h>
#include <AtariStudio/Disassembler/StructuredStatementFormatter.h>
#include <AtariStudio/Disassembler/StructuredTranslationUnitGenerator.h>

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>

int main(
    int argc,
    char* argv[])
{
    if (argc != 2)
    {
        std::cerr
            << "Usage: OpcodeTranslationEmitter <output.cpp>\n";

        return 1;
    }

    atari::Memory memory;
    atari::Disassembler disassembler;
    atari::DisassemblyMetadata metadata;
    atari::StructuredStatementFormatter
        formatter;

    atari::StructuredExpression root;

    root.kind =
        atari::StructuredExpressionKind::Block;

    root.address = 0x2000;
    root.statement = "OpcodeMatrix";

    std::size_t officialOpcodeCount = 0;

    for (std::uint16_t opcode = 0;
         opcode <= 0xFF;
         ++opcode)
    {
        memory.Write8(
            0x2000,
            static_cast<atari::u8>(
                opcode));

        memory.Write8(0x2001, 0x34);
        memory.Write8(0x2002, 0x12);

        const auto instruction =
            disassembler.Decode(
                memory,
                0x2000);

        if (instruction.instruction ==
            atari::cpu6502::Instruction::Illegal)
        {
            continue;
        }

        ++officialOpcodeCount;

        std::string statement;

        if (instruction.instruction ==
            atari::cpu6502::Instruction::RTS)
        {
            statement = "return";
        }
        else
        {
            statement =
                formatter.Format(
                    metadata,
                    instruction);
        }

        if (statement.empty())
        {
            std::cerr
                << "Empty formatter output for opcode $"
                << std::hex
                << opcode
                << '\n';

            return 1;
        }

        atari::StructuredExpression expression;

        expression.kind =
            atari::StructuredExpressionKind::Statement;

        expression.address =
            static_cast<atari::u16>(
                0x2000 + opcode);

        expression.statement =
            std::move(
                statement);

        root.children.push_back(
            std::move(
                expression));
    }

    if (officialOpcodeCount != 151)
    {
        std::cerr
            << "Expected 151 official opcodes, got "
            << officialOpcodeCount
            << '\n';

        return 1;
    }

    atari::StructuredExpressionResult result;

    result.roots.push_back(
        std::move(
            root));

    const std::string translationUnit =
        atari::StructuredTranslationUnitGenerator{}
            .Generate(result);

    std::ofstream output(
        argv[1],
        std::ios::binary |
            std::ios::trunc);

    if (!output)
    {
        std::cerr
            << "Cannot create output file: "
            << argv[1]
            << '\n';

        return 1;
    }

    output << translationUnit;

    if (!output)
    {
        std::cerr
            << "Cannot write output file: "
            << argv[1]
            << '\n';

        return 1;
    }

    return 0;
}
