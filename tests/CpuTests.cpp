#include <AtariStudio/Core/Memory.h>
#include <AtariStudio/Cpu6502/Instruction.h>
#include <AtariStudio/Cpu6502/OpcodeTable.h>
#include <AtariStudio/Disassembler/Disassembler.h>
#include <AtariStudio/Disassembler/Listing.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>

namespace
{

bool Expect(
    bool condition,
    const char* message)
{
    if (!condition)
    {
        std::cerr
            << "FAILED: "
            << message
            << '\n';
    }

    return condition;
}

std::uint8_t ExpectedLength(
    std::string_view mode)
{
    if (mode == "IMP" ||
        mode == "ACC")
    {
        return 1;
    }

    if (mode == "ABS" ||
        mode == "ABX" ||
        mode == "ABY" ||
        mode == "IND")
    {
        return 3;
    }

    return 2;
}

std::string_view ExpectedOperand(
    std::string_view mode)
{
    if (mode == "IMP")
    {
        return "";
    }

    if (mode == "ACC")
    {
        return "A";
    }

    if (mode == "IMM")
    {
        return "#$34";
    }

    if (mode == "ZPG")
    {
        return "$34";
    }

    if (mode == "ZPX")
    {
        return "$34,X";
    }

    if (mode == "ZPY")
    {
        return "$34,Y";
    }

    if (mode == "REL")
    {
        return "$2036";
    }

    if (mode == "ABS")
    {
        return "$1234";
    }

    if (mode == "ABX")
    {
        return "$1234,X";
    }

    if (mode == "ABY")
    {
        return "$1234,Y";
    }

    if (mode == "IND")
    {
        return "($1234)";
    }

    if (mode == "IDX")
    {
        return "($34,X)";
    }

    return "($34),Y";
}

} // namespace

int main()
{
    bool passed = true;
    std::size_t officialOpcodes = 0;
    const atari::Disassembler disassembler;

    static constexpr std::array<
        std::string_view,
        16> ExpectedInstructions =
        {
            "BRK ORA ILL ILL ILL ORA ASL ILL PHP ORA ASL ILL ILL ORA ASL ILL",
            "BPL ORA ILL ILL ILL ORA ASL ILL CLC ORA ILL ILL ILL ORA ASL ILL",
            "JSR AND ILL ILL BIT AND ROL ILL PLP AND ROL ILL BIT AND ROL ILL",
            "BMI AND ILL ILL ILL AND ROL ILL SEC AND ILL ILL ILL AND ROL ILL",
            "RTI EOR ILL ILL ILL EOR LSR ILL PHA EOR LSR ILL JMP EOR LSR ILL",
            "BVC EOR ILL ILL ILL EOR LSR ILL CLI EOR ILL ILL ILL EOR LSR ILL",
            "RTS ADC ILL ILL ILL ADC ROR ILL PLA ADC ROR ILL JMP ADC ROR ILL",
            "BVS ADC ILL ILL ILL ADC ROR ILL SEI ADC ILL ILL ILL ADC ROR ILL",
            "ILL STA ILL ILL STY STA STX ILL DEY ILL TXA ILL STY STA STX ILL",
            "BCC STA ILL ILL STY STA STX ILL TYA STA TXS ILL ILL STA ILL ILL",
            "LDY LDA LDX ILL LDY LDA LDX ILL TAY LDA TAX ILL LDY LDA LDX ILL",
            "BCS LDA ILL ILL LDY LDA LDX ILL CLV LDA TSX ILL LDY LDA LDX ILL",
            "CPY CMP ILL ILL CPY CMP DEC ILL INY CMP DEX ILL CPY CMP DEC ILL",
            "BNE CMP ILL ILL ILL CMP DEC ILL CLD CMP ILL ILL ILL CMP DEC ILL",
            "CPX SBC ILL ILL CPX SBC INC ILL INX SBC NOP ILL CPX SBC INC ILL",
            "BEQ SBC ILL ILL ILL SBC INC ILL SED SBC ILL ILL ILL SBC INC ILL"
        };

    static constexpr std::array<
        std::string_view,
        16> ExpectedModes =
        {
            "IMP IDX ILL ILL ILL ZPG ZPG ILL IMP IMM ACC ILL ILL ABS ABS ILL",
            "REL IDY ILL ILL ILL ZPX ZPX ILL IMP ABY ILL ILL ILL ABX ABX ILL",
            "ABS IDX ILL ILL ZPG ZPG ZPG ILL IMP IMM ACC ILL ABS ABS ABS ILL",
            "REL IDY ILL ILL ILL ZPX ZPX ILL IMP ABY ILL ILL ILL ABX ABX ILL",
            "IMP IDX ILL ILL ILL ZPG ZPG ILL IMP IMM ACC ILL ABS ABS ABS ILL",
            "REL IDY ILL ILL ILL ZPX ZPX ILL IMP ABY ILL ILL ILL ABX ABX ILL",
            "IMP IDX ILL ILL ILL ZPG ZPG ILL IMP IMM ACC ILL IND ABS ABS ILL",
            "REL IDY ILL ILL ILL ZPX ZPX ILL IMP ABY ILL ILL ILL ABX ABX ILL",
            "ILL IDX ILL ILL ZPG ZPG ZPG ILL IMP ILL IMP ILL ABS ABS ABS ILL",
            "REL IDY ILL ILL ZPX ZPX ZPY ILL IMP ABY IMP ILL ILL ABX ILL ILL",
            "IMM IDX IMM ILL ZPG ZPG ZPG ILL IMP IMM IMP ILL ABS ABS ABS ILL",
            "REL IDY ILL ILL ZPX ZPX ZPY ILL IMP ABY IMP ILL ABX ABX ABY ILL",
            "IMM IDX ILL ILL ZPG ZPG ZPG ILL IMP IMM IMP ILL ABS ABS ABS ILL",
            "REL IDY ILL ILL ILL ZPX ZPX ILL IMP ABY ILL ILL ILL ABX ABX ILL",
            "IMM IDX ILL ILL ZPG ZPG ZPG ILL IMP IMM IMP ILL ABS ABS ABS ILL",
            "REL IDY ILL ILL ILL ZPX ZPX ILL IMP ABY ILL ILL ILL ABX ABX ILL"
        };

    auto opcodeMemory =
        std::make_unique<atari::Memory>();

    opcodeMemory->Write8(0x2001, 0x34);
    opcodeMemory->Write8(0x2002, 0x12);

    for (std::size_t row = 0;
         row < ExpectedInstructions.size();
         ++row)
    {
        std::istringstream instructionStream{
            std::string(
                ExpectedInstructions[row])};

        std::istringstream modeStream{
            std::string(
                ExpectedModes[row])};

        for (std::size_t column = 0;
             column < 16;
             ++column)
        {
            std::string expectedInstruction;
            std::string expectedMode;

            instructionStream >>
                expectedInstruction;
            modeStream >>
                expectedMode;

            const auto opcodeValue =
                static_cast<std::uint8_t>(
                    row * 16 +
                    column);

            opcodeMemory->Write8(
                0x2000,
                opcodeValue);

            const auto decoded =
                disassembler.Decode(
                    *opcodeMemory,
                    0x2000);

            const auto& opcode =
                atari::cpu6502::OpcodeTable::Get(
                    opcodeValue);

            if (expectedInstruction == "ILL")
            {
                passed &=
                    Expect(
                        opcode.instruction ==
                            atari::cpu6502::Instruction::Illegal &&
                        !opcode.official &&
                        opcode.addressMode ==
                            atari::cpu6502::AddressMode::Implied &&
                        opcode.length == 1 &&
                        decoded.text.find(
                            ".DB $") == 0,
                        "illegal opcode metadata must match the NMOS table");

                continue;
            }

            ++officialOpcodes;

            passed &=
                Expect(
                    opcode.instruction !=
                        atari::cpu6502::Instruction::Illegal &&
                    opcode.official &&
                    decoded.mnemonic ==
                        expectedInstruction &&
                    decoded.operand ==
                        ExpectedOperand(
                            expectedMode) &&
                    opcode.length ==
                        ExpectedLength(
                            expectedMode) &&
                    opcode.cycles >= 2 &&
                    opcode.cycles <= 7,
                    "official opcode must match the NMOS instruction matrix");
        }
    }

    passed &=
        Expect(
            officialOpcodes == 151,
            "NMOS 6502 table must contain 151 official opcodes");

    atari::Memory memory;

    memory.Write8(0xFFFE, 0x4C);
    memory.Write8(0xFFFF, 0x34);
    memory.Write8(0x0000, 0x12);

    const auto truncated =
        disassembler.Decode(
            memory,
            0xFFFE);

    passed &=
        Expect(
            truncated.length == 3 &&
            truncated.bytes[0] == 0x4C &&
            truncated.bytes[1] == 0x34 &&
            truncated.bytes[2] == 0x12 &&
            truncated.text == "JMP $1234",
            "6502 decode at end of address space must wrap operands");

    memory.Write8(0xFFFE, 0xD0);
    memory.Write8(0xFFFF, 0x01);

    const auto forwardBranchWrap =
        disassembler.Decode(
            memory,
            0xFFFE);

    passed &=
        Expect(
            forwardBranchWrap.length == 2 &&
            forwardBranchWrap.bytes[1] == 0x01 &&
            forwardBranchWrap.text == "BNE $0001",
            "positive relative branch must wrap past $FFFF");

    memory.Write8(0x0000, 0xD0);
    memory.Write8(0x0001, 0x80);

    const auto backwardBranchWrap =
        disassembler.Decode(
            memory,
            0x0000);

    passed &=
        Expect(
            backwardBranchWrap.length == 2 &&
            backwardBranchWrap.bytes[1] == 0x80 &&
            backwardBranchWrap.text == "BNE $FF82",
            "negative relative branch must wrap below $0000");

    memory.Write8(0x2000, 0x02);

    const auto illegal =
        disassembler.Decode(
            memory,
            0x2000);

    passed &=
        Expect(
            illegal.instruction ==
                atari::cpu6502::Instruction::Illegal &&
            illegal.length == 1 &&
            illegal.text == ".DB $02",
            "illegal opcode must decode as one data byte");

    atari::DisassembledInstruction malformed;
    malformed.address = 0x3456;
    malformed.bytes = {0xAA, 0xBB, 0xCC};
    malformed.length = 0xFF;
    malformed.text = "MALFORMED";

    const std::string formattedMalformed =
        atari::Listing{}.
            FormatLine(
                malformed);

    passed &=
        Expect(
            formattedMalformed.find(
                "3456: AA BB CC") == 0 &&
            formattedMalformed.ends_with(
                "MALFORMED"),
            "listing formatter must clamp malformed byte counts");

    return passed ? 0 : 1;
}
