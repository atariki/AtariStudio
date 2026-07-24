#include <AtariStudio/Disassembler/Disassembler.h>

#include <AtariStudio/Core/Memory.h>
#include <AtariStudio/Cpu6502/OpcodeTable.h>

#include <cstdio>

namespace atari
{

    using namespace cpu6502;

    namespace
    {

        template<typename... Args>
        std::string StringFormat(const char* format, Args... args)
        {
            char buffer[64];

            std::snprintf(buffer, sizeof(buffer), format, args...);

            return std::string(buffer);
        }

    }

    const char* Disassembler::InstructionToString(Instruction instruction)
    {
        switch (instruction)
        {
        case Instruction::ADC: return "ADC";
        case Instruction::AND: return "AND";
        case Instruction::ASL: return "ASL";

        case Instruction::BCC: return "BCC";
        case Instruction::BCS: return "BCS";
        case Instruction::BEQ: return "BEQ";
        case Instruction::BIT: return "BIT";
        case Instruction::BMI: return "BMI";
        case Instruction::BNE: return "BNE";
        case Instruction::BPL: return "BPL";
        case Instruction::BRK: return "BRK";
        case Instruction::BVC: return "BVC";
        case Instruction::BVS: return "BVS";

        case Instruction::CLC: return "CLC";
        case Instruction::CLD: return "CLD";
        case Instruction::CLI: return "CLI";
        case Instruction::CLV: return "CLV";

        case Instruction::CMP: return "CMP";
        case Instruction::CPX: return "CPX";
        case Instruction::CPY: return "CPY";

        case Instruction::DEC: return "DEC";
        case Instruction::DEX: return "DEX";
        case Instruction::DEY: return "DEY";

        case Instruction::EOR: return "EOR";

        case Instruction::INC: return "INC";
        case Instruction::INX: return "INX";
        case Instruction::INY: return "INY";

        case Instruction::JMP: return "JMP";
        case Instruction::JSR: return "JSR";

        case Instruction::LDA: return "LDA";
        case Instruction::LDX: return "LDX";
        case Instruction::LDY: return "LDY";

        case Instruction::LSR: return "LSR";

        case Instruction::NOP: return "NOP";

        case Instruction::ORA: return "ORA";

        case Instruction::PHA: return "PHA";
        case Instruction::PHP: return "PHP";
        case Instruction::PLA: return "PLA";
        case Instruction::PLP: return "PLP";

        case Instruction::ROL: return "ROL";
        case Instruction::ROR: return "ROR";
        case Instruction::RTI: return "RTI";
        case Instruction::RTS: return "RTS";

        case Instruction::SBC: return "SBC";

        case Instruction::SEC: return "SEC";
        case Instruction::SED: return "SED";
        case Instruction::SEI: return "SEI";

        case Instruction::STA: return "STA";
        case Instruction::STX: return "STX";
        case Instruction::STY: return "STY";

        case Instruction::TAX: return "TAX";
        case Instruction::TAY: return "TAY";
        case Instruction::TSX: return "TSX";
        case Instruction::TXA: return "TXA";
        case Instruction::TXS: return "TXS";
        case Instruction::TYA: return "TYA";

        default:
            return "???";
        }
    }

    std::string Disassembler::FormatOperand(
        AddressMode mode,
        uint16_t address,
        const uint8_t* bytes)
    {
        const uint8_t lo = bytes[1];
        const uint8_t hi = bytes[2];
        const uint16_t word = static_cast<uint16_t>(lo | (hi << 8));

        switch (mode)
        {
        case AddressMode::Implied:
            return "";

        case AddressMode::Accumulator:
            return "A";

        case AddressMode::Immediate:
            return StringFormat("#$%02X", lo);

        case AddressMode::ZeroPage:
            return StringFormat("$%02X", lo);

        case AddressMode::ZeroPageX:
            return StringFormat("$%02X,X", lo);

        case AddressMode::ZeroPageY:
            return StringFormat("$%02X,Y", lo);

        case AddressMode::Absolute:
            return StringFormat("$%04X", word);

        case AddressMode::AbsoluteX:
            return StringFormat("$%04X,X", word);

        case AddressMode::AbsoluteY:
            return StringFormat("$%04X,Y", word);

        case AddressMode::Indirect:
            return StringFormat("($%04X)", word);

        case AddressMode::IndexedIndirect:
            return StringFormat("($%02X,X)", lo);

        case AddressMode::IndirectIndexed:
            return StringFormat("($%02X),Y", lo);

        case AddressMode::Relative:
        {
            const int8_t offset = static_cast<int8_t>(lo);

            const uint16_t target =
                static_cast<uint16_t>(address + 2 + offset);

            return StringFormat("$%04X", target);
        }

        default:
            return "";
        }
    }

    DisassembledInstruction Disassembler::Decode(
        const Memory& memory,
        uint16_t address) const
    {
        DisassembledInstruction result;

        result.address = address;

        // Читаем опкод
        result.opcode = memory.Read(address);

        // Получаем информацию из таблицы опкодов
        const auto& info = OpcodeTable::Get(result.opcode);

        result.instruction = info.instruction;
        result.addressMode = info.addressMode;
        result.length = info.length;

        // Считываем байты инструкции
        result.bytes[0] = result.opcode;

        if (info.length > 1)
            result.bytes[1] = memory.Read(address + 1);

        if (info.length > 2)
            result.bytes[2] = memory.Read(address + 2);

        // Мнемоника
        if (info.instruction == Instruction::Illegal)
        {
            result.mnemonic = StringFormat(".DB $%02X", result.opcode);
        }
        else
        {
            result.mnemonic = InstructionToString(info.instruction);
        }

        // Операнд
        result.operand = FormatOperand(
            info.addressMode,
            address,
            result.bytes.data());

        // Полная строка
        // Полная строка
        if (info.instruction == Instruction::Illegal)
        {
            result.text = result.mnemonic;
        }
        else if (result.operand.empty())
        {
            result.text = result.mnemonic;
        }
        else
        {
            result.text = result.mnemonic + " " + result.operand;
        }
    }