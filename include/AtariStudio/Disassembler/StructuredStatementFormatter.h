#pragma once

#include <cctype>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>

#include <AtariStudio/Core/Types.h>
#include <AtariStudio/Cpu6502/AddressMode.h>
#include <AtariStudio/Cpu6502/Instruction.h>
#include <AtariStudio/Disassembler/DisassembledInstruction.h>
#include <AtariStudio/Disassembler/DisassemblyMetadata.h>

namespace atari
{

class StructuredStatementFormatter
{
public:

    [[nodiscard]]
    std::string FormatAccumulatorBitwise(
        const DisassembledInstruction& source,
        const DisassembledInstruction& operation) const
    {
        return
            FormatBitwiseExpression(
                "A",
                source,
                operation);
    }

    [[nodiscard]]
    std::string FormatBitwiseTransfer(
        const DisassembledInstruction& source,
        const DisassembledInstruction& operation,
        const DisassembledInstruction& store) const
    {
        if (store.instruction !=
            cpu6502::Instruction::STA)
        {
            return {};
        }

        return
            FormatBitwiseExpression(
                OperandExpression(store),
                source,
                operation);
    }

    [[nodiscard]]
    std::string FormatAccumulatorBitwiseTransfer(
        const DisassembledInstruction& operation,
        const DisassembledInstruction& store,
        const std::string& accumulatorSource =
            "A") const
    {
        if (store.instruction !=
            cpu6502::Instruction::STA)
        {
            return {};
        }

        return
            FormatBitwiseExpression(
                OperandExpression(store),
                accumulatorSource,
                operation);
    }

    [[nodiscard]]
    std::string FormatAccumulatorBitwiseOperation(
        const DisassembledInstruction& operation,
        const std::string& accumulatorSource) const
    {
        return
            FormatBitwiseExpression(
                "A",
                accumulatorSource,
                operation);
    }

    [[nodiscard]]
    std::string FormatMemoryChangeLoad(
        const DisassembledInstruction& change,
        std::size_t count,
        const DisassembledInstruction& load) const
    {
        using Instruction =
            cpu6502::Instruction;

        const bool increment =
            change.instruction ==
            Instruction::INC;

        const bool decrement =
            change.instruction ==
            Instruction::DEC;

        std::string destination;

        switch (load.instruction)
        {
        case Instruction::LDA:
            destination = "A";
            break;

        case Instruction::LDX:
            destination = "X";
            break;

        case Instruction::LDY:
            destination = "Y";
            break;

        default:
            return {};
        }

        if ((!increment && !decrement) ||
            count == 0 ||
            !SameMemoryOperand(
                change,
                load))
        {
            return {};
        }

        const std::string memory =
            OperandExpression(
                change);

        if (memory.empty())
        {
            return {};
        }

        if (IsHardwareMemory(change))
        {
            return
                std::string(
                    increment
                        ? "inc_load6502"
                        : "dec_load6502") +
                "(&" +
                memory +
                ", " +
                std::to_string(count) +
                ", &" +
                destination +
                ", &N, &Z)";
        }

        if (count == 1)
        {
            return
                destination +
                " = " +
                (increment
                     ? "++"
                     : "--") +
                memory;
        }

        return
            destination +
            " = (" +
            memory +
            (increment
                 ? " += "
                 : " -= ") +
            std::to_string(count) +
            ")";
    }

    [[nodiscard]]
    std::string FormatWideDecrement(
        const DisassembledInstruction& lowLoad,
        const DisassembledInstruction& highDecrement,
        const DisassembledInstruction& lowDecrement) const
    {
        using Instruction =
            cpu6502::Instruction;

        if (lowLoad.instruction != Instruction::LDA ||
            highDecrement.instruction != Instruction::DEC ||
            lowDecrement.instruction != Instruction::DEC ||
            !SameMemoryOperand(
                lowLoad,
                lowDecrement))
        {
            return {};
        }

        const std::string destination =
            WordMemoryExpression(
                lowDecrement,
                highDecrement);

        if (destination.empty())
        {
            return {};
        }

        return
            "dec16_6502(&" +
            destination +
            ", &A, &N, &Z)";
    }

    [[nodiscard]]
    std::string FormatWideIncrement(
        const DisassembledInstruction& low,
        const DisassembledInstruction& high) const
    {
        if (low.instruction !=
                cpu6502::Instruction::INC ||
            high.instruction !=
                cpu6502::Instruction::INC)
        {
            return {};
        }

        const std::string destination =
            WordMemoryExpression(
                low,
                high);

        if (destination.empty())
        {
            return {};
        }

        return
            "inc16_6502(&" +
            destination +
            ", &N, &Z)";
    }

    [[nodiscard]]
    std::string FormatRepeatedMemoryChange(
        const DisassembledInstruction& instruction,
        std::size_t count,
        bool preserveNegativeZero) const
    {
        using Instruction =
            cpu6502::Instruction;

        const bool increment =
            instruction.instruction ==
            Instruction::INC;

        const bool decrement =
            instruction.instruction ==
            Instruction::DEC;

        const std::string destination =
            OperandExpression(
                instruction);

        if ((!increment && !decrement) ||
            count < 2 ||
            destination.empty())
        {
            return {};
        }

        if (preserveNegativeZero ||
            IsHardwareMemory(
                instruction))
        {
            return
                std::string(
                    increment
                        ? "inc6502_n"
                        : "dec6502_n") +
                "(&" +
                destination +
                ", " +
                std::to_string(count) +
                ", &N, &Z)";
        }

        return
            destination +
            (increment
                 ? " += "
                 : " -= ") +
            std::to_string(count);
    }

    [[nodiscard]]
    std::string FormatWideArithmeticTransfer(
        const DisassembledInstruction& lowLoad,
        const DisassembledInstruction& lowOperation,
        const DisassembledInstruction& lowStore,
        const DisassembledInstruction& highLoad,
        const DisassembledInstruction& highOperation,
        const DisassembledInstruction& highStore,
        const std::string& carryInput) const
    {
        using Instruction =
            cpu6502::Instruction;

        const bool addition =
            lowOperation.instruction ==
                Instruction::ADC &&
            highOperation.instruction ==
                Instruction::ADC;

        const bool subtraction =
            lowOperation.instruction ==
                Instruction::SBC &&
            highOperation.instruction ==
                Instruction::SBC;

        if (lowLoad.instruction != Instruction::LDA ||
            highLoad.instruction != Instruction::LDA ||
            lowStore.instruction != Instruction::STA ||
            highStore.instruction != Instruction::STA ||
            (!addition && !subtraction))
        {
            return {};
        }

        const std::string source =
            WordMemoryExpression(
                lowLoad,
                highLoad);

        const std::string operand =
            WordMemoryExpression(
                lowOperation,
                highOperation);

        const std::string destination =
            WordMemoryExpression(
                lowStore,
                highStore);

        if (source.empty() ||
            operand.empty() ||
            destination.empty())
        {
            return {};
        }

        return
            std::string(
                addition
                    ? "adc16_6502"
                    : "sbc16_6502") +
            "(" +
            source +
            ", " +
            operand +
            ", " +
            carryInput +
            ", D, &" +
            destination +
            ", &A, &C, &V, &N, &Z)";
    }

    [[nodiscard]]
    std::string FormatArithmeticTransfer(
        const DisassembledInstruction& load,
        const DisassembledInstruction& operation,
        const DisassembledInstruction& store,
        const std::string& carryInput,
        bool preserveCarry,
        bool preserveOverflow,
        bool preserveNegativeZero,
        bool decimalClear) const
    {
        using Instruction =
            cpu6502::Instruction;

        if (load.instruction != Instruction::LDA ||
            store.instruction != Instruction::STA)
        {
            return {};
        }

        const bool subtract =
            operation.instruction ==
            Instruction::SBC;

        if (!subtract &&
            operation.instruction !=
                Instruction::ADC)
        {
            return {};
        }

        return
            FormatArithmeticIntrinsic(
                subtract
                    ? "sbc6502"
                    : "adc6502",
                OperandExpression(load),
                OperandExpression(operation),
                carryInput,
                preserveCarry ||
                    preserveOverflow ||
                    preserveNegativeZero,
                decimalClear,
                subtract,
                OperandExpression(store));
    }

    [[nodiscard]]
    std::string FormatCarryOperation(
        const DisassembledInstruction& instruction,
        const std::string& carryInput,
        bool preserveCarry,
        bool preserveOverflow,
        bool preserveNegativeZero,
        bool decimalClear,
        const std::string& accumulatorSource =
            "A") const
    {
        using Instruction =
            cpu6502::Instruction;

        const std::string operand =
            OperandExpression(
                instruction);

        const bool preserveFlags =
            preserveCarry ||
            preserveOverflow ||
            preserveNegativeZero;

        switch (instruction.instruction)
        {
        case Instruction::ASL:
        {
            const std::string destination =
                WritableOperand(instruction);

            return
                FormatShiftIntrinsic(
                    "asl6502",
                    instruction.addressMode ==
                            cpu6502::AddressMode::Accumulator
                        ? accumulatorSource
                        : destination,
                    destination,
                    preserveCarry ||
                        preserveNegativeZero);
        }

        case Instruction::LSR:
        {
            const std::string destination =
                WritableOperand(instruction);

            return
                FormatShiftIntrinsic(
                    "lsr6502",
                    instruction.addressMode ==
                            cpu6502::AddressMode::Accumulator
                        ? accumulatorSource
                        : destination,
                    destination,
                    preserveCarry ||
                        preserveNegativeZero);
        }

        case Instruction::ADC:
            return
                FormatArithmeticIntrinsic(
                    "adc6502",
                    accumulatorSource,
                    operand,
                    carryInput,
                    preserveFlags,
                    decimalClear,
                    false,
                    "A");

        case Instruction::SBC:
            return
                FormatArithmeticIntrinsic(
                    "sbc6502",
                    accumulatorSource,
                    operand,
                    carryInput,
                    preserveFlags,
                    decimalClear,
                    true,
                    "A");

        case Instruction::ROL:
        {
            const std::string destination =
                WritableOperand(instruction);

            return
                FormatRotateIntrinsic(
                    "rol6502",
                    instruction.addressMode ==
                            cpu6502::AddressMode::Accumulator
                        ? accumulatorSource
                        : destination,
                    destination,
                    carryInput,
                    preserveCarry ||
                    preserveNegativeZero);
        }

        case Instruction::ROR:
        {
            const std::string destination =
                WritableOperand(instruction);

            return
                FormatRotateIntrinsic(
                    "ror6502",
                    instruction.addressMode ==
                            cpu6502::AddressMode::Accumulator
                        ? accumulatorSource
                        : destination,
                    destination,
                    carryInput,
                    preserveCarry ||
                    preserveNegativeZero);
        }

        default:
            return {};
        }
    }

    [[nodiscard]]
    std::string FormatRegisterTransfer(
        const DisassembledInstruction& load,
        const DisassembledInstruction& store) const
    {
        using Instruction =
            cpu6502::Instruction;

        const bool accumulatorTransfer =
            load.instruction ==
                Instruction::LDA &&
            store.instruction ==
                Instruction::STA;

        const bool xTransfer =
            load.instruction ==
                Instruction::LDX &&
            store.instruction ==
                Instruction::STX;

        const bool yTransfer =
            load.instruction ==
                Instruction::LDY &&
            store.instruction ==
                Instruction::STY;

        if (!accumulatorTransfer &&
            !xTransfer &&
            !yTransfer)
        {
            return {};
        }

        return
            Assignment(
                OperandExpression(
                    store),
                OperandExpression(
                    load));
    }

    [[nodiscard]]
    std::string FormatAccumulatorStore(
        const DisassembledInstruction& store,
        const std::string& accumulatorSource) const
    {
        if (store.instruction !=
            cpu6502::Instruction::STA)
        {
            return {};
        }

        return
            Assignment(
                OperandExpression(store),
                accumulatorSource);
    }

    [[nodiscard]]
    std::string FormatAccumulatorConsumer(
        const DisassembledInstruction& instruction,
        const std::string& accumulatorSource,
        bool preserveCarry = false,
        bool preserveOverflow = false,
        bool preserveNegativeZero = false) const
    {
        using Instruction =
            cpu6502::Instruction;

        const std::string operand =
            OperandExpression(instruction);

        switch (instruction.instruction)
        {
        case Instruction::CMP:
            if (preserveCarry ||
                preserveNegativeZero)
            {
                return
                    "compare6502(" +
                    accumulatorSource +
                    ", " +
                    operand +
                    ", &C, &N, &Z)";
            }

            return
                Call(
                    "compare",
                    accumulatorSource +
                        ", " +
                        operand);

        case Instruction::CPX:
            if (preserveCarry ||
                preserveNegativeZero)
            {
                return
                    "compare6502(X, " +
                    operand +
                    ", &C, &N, &Z)";
            }

            return
                Call(
                    "compare",
                    "X, " +
                        operand);

        case Instruction::CPY:
            if (preserveCarry ||
                preserveNegativeZero)
            {
                return
                    "compare6502(Y, " +
                    operand +
                    ", &C, &N, &Z)";
            }

            return
                Call(
                    "compare",
                    "Y, " +
                        operand);

        case Instruction::BIT:
            if (preserveOverflow ||
                preserveNegativeZero)
            {
                return
                    "test_bits6502(" +
                    accumulatorSource +
                    ", " +
                    operand +
                    ", &V, &N, &Z)";
            }

            return
                Call(
                    "test_bits",
                    accumulatorSource +
                        ", " +
                        operand);

        case Instruction::PHA:
            return
                Call(
                    "push",
                    accumulatorSource);

        default:
            return {};
        }
    }

    [[nodiscard]]
    std::string FormatAccumulatorRegisterTransfer(
        const DisassembledInstruction& instruction,
        const std::string& accumulatorSource,
        bool preserveNegativeZero) const
    {
        using Instruction =
            cpu6502::Instruction;

        std::string destination;

        switch (instruction.instruction)
        {
        case Instruction::TAX:
            destination = "X";
            break;

        case Instruction::TAY:
            destination = "Y";
            break;

        default:
            return {};
        }

        if (preserveNegativeZero)
        {
            return
                "transfer6502(" +
                accumulatorSource +
                ", &" +
                destination +
                ", &N, &Z)";
        }

        return
            Assignment(
                destination,
                accumulatorSource);
    }

    [[nodiscard]]
    std::string FormatSourceRegisterTransfer(
        const DisassembledInstruction& instruction,
        bool preserveNegativeZero) const
    {
        using Instruction =
            cpu6502::Instruction;

        std::string source;
        std::string destination;

        switch (instruction.instruction)
        {
        case Instruction::TXA:
            source = "X";
            destination = "A";
            break;

        case Instruction::TYA:
            source = "Y";
            destination = "A";
            break;

        case Instruction::TSX:
            source = "SP";
            destination = "X";
            break;

        default:
            return {};
        }

        if (preserveNegativeZero)
        {
            return
                "transfer6502(" +
                source +
                ", &" +
                destination +
                ", &N, &Z)";
        }

        return
            Assignment(
                destination,
                source);
    }

    [[nodiscard]]
    std::string FormatLoad(
        const DisassembledInstruction& instruction,
        bool preserveNegativeZero) const
    {
        using Instruction =
            cpu6502::Instruction;

        std::string destination;

        switch (instruction.instruction)
        {
        case Instruction::LDA:
            destination = "A";
            break;

        case Instruction::LDX:
            destination = "X";
            break;

        case Instruction::LDY:
            destination = "Y";
            break;

        default:
            return {};
        }

        const std::string source =
            OperandExpression(instruction);

        if (preserveNegativeZero)
        {
            return
                "load6502(" +
                source +
                ", &" +
                destination +
                ", &N, &Z)";
        }

        return
            Assignment(
                destination,
                source);
    }

    [[nodiscard]]
    std::string FormatIncrementDecrement(
        const DisassembledInstruction& instruction,
        bool preserveNegativeZero) const
    {
        using Instruction =
            cpu6502::Instruction;

        std::string destination;
        bool increment = false;

        switch (instruction.instruction)
        {
        case Instruction::INC:
            destination =
                OperandExpression(instruction);
            increment = true;
            break;

        case Instruction::DEC:
            destination =
                OperandExpression(instruction);
            break;

        case Instruction::INX:
            destination = "X";
            increment = true;
            break;

        case Instruction::INY:
            destination = "Y";
            increment = true;
            break;

        case Instruction::DEX:
            destination = "X";
            break;

        case Instruction::DEY:
            destination = "Y";
            break;

        default:
            return {};
        }

        if (preserveNegativeZero)
        {
            return
                std::string(
                    increment
                        ? "inc6502_n"
                        : "dec6502_n") +
                "(&" +
                destination +
                ", 1, &N, &Z)";
        }

        return
            Prefix(
                increment
                    ? "++"
                    : "--",
                destination);
    }

    [[nodiscard]]
    std::string FormatStackPull(
        const DisassembledInstruction& instruction,
        bool preserveNegativeZero) const
    {
        if (instruction.instruction !=
            cpu6502::Instruction::PLA)
        {
            return {};
        }

        if (preserveNegativeZero)
        {
            return "pull6502(&A, &N, &Z)";
        }

        return "A = pop()";
    }

    [[nodiscard]]
    std::string Format(
        const DisassemblyMetadata& metadata,
        const DisassembledInstruction& instruction) const
    {
        using Instruction =
            cpu6502::Instruction;

        const std::string operand =
            OperandExpression(
                instruction);

        switch (instruction.instruction)
        {
        case Instruction::LDA:
            return Assignment("A", operand);

        case Instruction::LDX:
            return Assignment("X", operand);

        case Instruction::LDY:
            return Assignment("Y", operand);

        case Instruction::STA:
            return Assignment(operand, "A");

        case Instruction::STX:
            return Assignment(operand, "X");

        case Instruction::STY:
            return Assignment(operand, "Y");

        case Instruction::INC:
            return Prefix("++", operand);

        case Instruction::DEC:
            return Prefix("--", operand);

        case Instruction::INX:
            return "++X";

        case Instruction::INY:
            return "++Y";

        case Instruction::DEX:
            return "--X";

        case Instruction::DEY:
            return "--Y";

        case Instruction::TAX:
            return "X = A";

        case Instruction::TAY:
            return "Y = A";

        case Instruction::TSX:
            return "X = SP";

        case Instruction::TXA:
            return "A = X";

        case Instruction::TXS:
            return "SP = X";

        case Instruction::TYA:
            return "A = Y";

        case Instruction::AND:
            return CompoundAssignment("A", "&=", operand);

        case Instruction::ADC:
        case Instruction::SBC:
        case Instruction::ROL:
        case Instruction::ROR:
            return
                FormatCarryOperation(
                    instruction,
                    "C",
                    true,
                    true,
                    true,
                    false);

        case Instruction::EOR:
            return CompoundAssignment("A", "^=", operand);

        case Instruction::ORA:
            return CompoundAssignment("A", "|=", operand);

        case Instruction::ASL:
            return CompoundAssignment(
                WritableOperand(instruction),
                "<<=",
                "1");

        case Instruction::LSR:
            return CompoundAssignment(
                WritableOperand(instruction),
                ">>=",
                "1");

        case Instruction::CMP:
            return Call("compare", "A, " + operand);

        case Instruction::CPX:
            return Call("compare", "X, " + operand);

        case Instruction::CPY:
            return Call("compare", "Y, " + operand);

        case Instruction::BIT:
            return Call("test_bits", "A, " + operand);

        case Instruction::PHA:
            return "push(A)";

        case Instruction::PHP:
            return "push(P)";

        case Instruction::PLA:
            return "A = pop()";

        case Instruction::PLP:
            return "P = pop()";

        case Instruction::CLC:
            return "C = 0";

        case Instruction::SEC:
            return "C = 1";

        case Instruction::CLD:
            return "D = 0";

        case Instruction::SED:
            return "D = 1";

        case Instruction::CLI:
            return "I = 0";

        case Instruction::SEI:
            return "I = 1";

        case Instruction::CLV:
            return "V = 0";

        case Instruction::JSR:
            return Call(
                RoutineName(
                    metadata,
                    AbsoluteOperand(
                        instruction)),
                "");

        case Instruction::BRK:
            return
                "brk6502(" +
                Hex(
                    instruction.address,
                    4) +
                "); return /* BRK */";

        case Instruction::RTI:
            return
                "rti6502(); return /* RTI */";

        case Instruction::NOP:
            return "/* NOP */";

        default:
            return
                metadata.FormatInstruction(
                    instruction);
        }
    }

private:

    [[nodiscard]]
    static std::string AccumulatorSourceExpression(
        const DisassembledInstruction& source)
    {
        using Instruction =
            cpu6502::Instruction;

        switch (source.instruction)
        {
        case Instruction::LDA:
            return
                OperandExpression(source);

        case Instruction::TXA:
            return "X";

        case Instruction::TYA:
            return "Y";

        case Instruction::PLA:
            return "pop()";

        default:
            return {};
        }
    }

    [[nodiscard]]
    static std::string FormatBitwiseExpression(
        const std::string& destination,
        const DisassembledInstruction& source,
        const DisassembledInstruction& operation)
    {
        return
            FormatBitwiseExpression(
                destination,
                AccumulatorSourceExpression(
                    source),
                operation);
    }

    [[nodiscard]]
    static std::string FormatBitwiseExpression(
        const std::string& destination,
        const std::string& left,
        const DisassembledInstruction& operation)
    {
        using Instruction =
            cpu6502::Instruction;

        const char* symbol = nullptr;

        switch (operation.instruction)
        {
        case Instruction::AND:
            symbol = "&";
            break;

        case Instruction::EOR:
            symbol = "^";
            break;

        case Instruction::ORA:
            symbol = "|";
            break;

        default:
            return {};
        }

        const std::string right =
            OperandExpression(operation);

        if (destination.empty() ||
            left.empty() ||
            right.empty())
        {
            return {};
        }

        return
            destination +
            " = " +
            left +
            " " +
            symbol +
            " " +
            right;
    }

    [[nodiscard]]
    static bool SameMemoryOperand(
        const DisassembledInstruction& left,
        const DisassembledInstruction& right)
    {
        return
            left.addressMode ==
                right.addressMode &&
            left.length ==
                right.length &&
            left.bytes[1] ==
                right.bytes[1] &&
            left.bytes[2] ==
                right.bytes[2];
    }

    [[nodiscard]]
    static bool IsHardwareMemory(
        const DisassembledInstruction& instruction)
    {
        using AddressMode =
            cpu6502::AddressMode;

        u16 address = 0;

        switch (instruction.addressMode)
        {
        case AddressMode::Absolute:
        case AddressMode::AbsoluteX:
        case AddressMode::AbsoluteY:
            address =
                AbsoluteOperand(
                    instruction);
            break;

        case AddressMode::ZeroPage:
        case AddressMode::ZeroPageX:
        case AddressMode::ZeroPageY:
        case AddressMode::IndexedIndirect:
        case AddressMode::IndirectIndexed:
            return false;

        default:
            return true;
        }

        return
            address >= 0xD000 &&
            address <= 0xD7FF;
    }

    [[nodiscard]]
    static std::string WordMemoryExpression(
        const DisassembledInstruction& low,
        const DisassembledInstruction& high)
    {
        using AddressMode =
            cpu6502::AddressMode;

        if (low.addressMode != high.addressMode)
        {
            return {};
        }

        u16 lowAddress = 0;
        u16 highAddress = 0;
        int width = 0;

        switch (low.addressMode)
        {
        case AddressMode::ZeroPage:
            lowAddress =
                low.bytes[1];

            highAddress =
                high.bytes[1];

            width = 2;

            if (highAddress !=
                static_cast<u8>(
                    lowAddress + 1))
            {
                return {};
            }

            break;

        case AddressMode::Absolute:
            lowAddress =
                AbsoluteOperand(low);

            highAddress =
                AbsoluteOperand(high);

            width = 4;

            if (highAddress !=
                static_cast<u16>(
                    lowAddress + 1))
            {
                return {};
            }

            break;

        default:
            return {};
        }

        return
            "memory16[" +
            Hex(
                lowAddress,
                width) +
            "]";
    }

    [[nodiscard]]
    static std::string FormatArithmeticIntrinsic(
        const std::string& function,
        const std::string& source,
        const std::string& operand,
        const std::string& carryInput,
        bool preserveFlags,
        bool decimalClear,
        bool subtract,
        const std::string& destination)
    {
        if (preserveFlags)
        {
            return
                function +
                "(" +
                source +
                ", " +
                operand +
                ", " +
                carryInput +
                ", D, &" +
                destination +
                ", &C, &V, &N, &Z)";
        }

        if (decimalClear)
        {
            return
                FormatBinaryArithmetic(
                    source,
                    operand,
                    carryInput,
                    subtract,
                    destination);
        }

        return
            destination +
            " = " +
            function +
            "_value(" +
            source +
            ", " +
            operand +
            ", " +
            carryInput +
            ", D)";
    }

    [[nodiscard]]
    static std::string FormatBinaryArithmetic(
        const std::string& source,
        const std::string& operand,
        const std::string& carryInput,
        bool subtract,
        const std::string& destination)
    {
        std::string expression =
            destination +
            " = byte(" +
            source +
            " ";

        if (!subtract)
        {
            expression +=
                "+ " +
                operand;

            if (carryInput != "0")
            {
                expression +=
                    " + " +
                    carryInput;
            }
        }
        else
        {
            expression +=
                "- " +
                operand;

            if (carryInput == "0")
            {
                expression +=
                    " - 1";
            }
            else if (carryInput != "1")
            {
                expression +=
                    " - (1 - " +
                    carryInput +
                    ")";
            }
        }

        expression += ")";

        return expression;
    }

    [[nodiscard]]
    static std::string FormatRotateIntrinsic(
        const std::string& function,
        const std::string& source,
        const std::string& destination,
        const std::string& carryInput,
        bool preserveFlags)
    {
        if (preserveFlags)
        {
            return
                function +
                "(" +
                source +
                ", " +
                carryInput +
                ", &" +
                destination +
                ", &C, &N, &Z)";
        }

        return
            destination +
            " = " +
            function +
            "_value(" +
            source +
            ", " +
            carryInput +
            ")";
    }

    [[nodiscard]]
    static std::string FormatShiftIntrinsic(
        const std::string& function,
        const std::string& source,
        const std::string& destination,
        bool preserveFlags)
    {
        if (preserveFlags)
        {
            return
                function +
                "(" +
                source +
                ", &" +
                destination +
                ", &C, &N, &Z)";
        }

        return
            destination +
            " = " +
            function +
            "_value(" +
            source +
            ")";
    }

    [[nodiscard]]
    static std::string Assignment(
        const std::string& destination,
        const std::string& source)
    {
        if (destination.empty() ||
            source.empty())
        {
            return {};
        }

        return
            destination +
            " = " +
            source;
    }

    [[nodiscard]]
    static std::string CompoundAssignment(
        const std::string& destination,
        const std::string& operation,
        const std::string& source)
    {
        if (destination.empty() ||
            source.empty())
        {
            return {};
        }

        return
            destination +
            " " +
            operation +
            " " +
            source;
    }

    [[nodiscard]]
    static std::string Prefix(
        const std::string& operation,
        const std::string& operand)
    {
        if (operand.empty())
        {
            return {};
        }

        return
            operation +
            operand;
    }

    [[nodiscard]]
    static std::string Call(
        const std::string& function,
        const std::string& arguments)
    {
        if (function.empty())
        {
            return {};
        }

        return
            function +
            "(" +
            arguments +
            ")";
    }

    [[nodiscard]]
    static std::string WritableOperand(
        const DisassembledInstruction& instruction)
    {
        if (instruction.addressMode ==
            cpu6502::AddressMode::Accumulator)
        {
            return "A";
        }

        return
            OperandExpression(
                instruction);
    }

    [[nodiscard]]
    static std::string OperandExpression(
        const DisassembledInstruction& instruction)
    {
        using AddressMode =
            cpu6502::AddressMode;

        const std::string byteOperand =
            Hex(
                instruction.bytes[1],
                2);

        const std::string wordOperand =
            Hex(
                AbsoluteOperand(
                    instruction),
                4);

        switch (instruction.addressMode)
        {
        case AddressMode::Accumulator:
            return "A";

        case AddressMode::Immediate:
            return byteOperand;

        case AddressMode::ZeroPage:
            return Memory(byteOperand);

        case AddressMode::ZeroPageX:
            return
                Memory(
                    "byte(" +
                    byteOperand +
                    " + X)");

        case AddressMode::ZeroPageY:
            return
                Memory(
                    "byte(" +
                    byteOperand +
                    " + Y)");

        case AddressMode::Absolute:
            return Memory(wordOperand);

        case AddressMode::AbsoluteX:
            return
                Memory(
                    wordOperand +
                    " + X");

        case AddressMode::AbsoluteY:
            return
                Memory(
                    wordOperand +
                    " + Y");

        case AddressMode::IndexedIndirect:
            return
                Memory(
                    "word(byte(" +
                    byteOperand +
                    " + X))");

        case AddressMode::IndirectIndexed:
            return
                Memory(
                    "word(" +
                    byteOperand +
                    ") + Y");

        case AddressMode::Indirect:
            return
                Memory(
                    "word(" +
                    wordOperand +
                    ")");

        case AddressMode::Implied:
        case AddressMode::Relative:
        default:
            return {};
        }
    }

    [[nodiscard]]
    static std::string Memory(
        const std::string& address)
    {
        return
            "memory[" +
            address +
            "]";
    }

    [[nodiscard]]
    static u16 AbsoluteOperand(
        const DisassembledInstruction& instruction) noexcept
    {
        return static_cast<u16>(
            static_cast<u16>(
                instruction.bytes[1]) |
            (static_cast<u16>(
                instruction.bytes[2])
             << 8));
    }

    [[nodiscard]]
    static std::string RoutineName(
        const DisassemblyMetadata& metadata,
        u16 address)
    {
        const u16 sourceAddress =
            metadata.Relocation()
                .ResolveDestination(
                    address)
                .value_or(
                    address);

        const std::string* symbol =
            metadata.Symbols().Find(
                sourceAddress);

        if (symbol != nullptr)
        {
            return Identifier(
                *symbol);
        }

        return
            "sub_" +
            HexDigits(
                sourceAddress,
                4);
    }

    [[nodiscard]]
    static std::string Identifier(
        std::string value)
    {
        for (char& character : value)
        {
            const auto byte =
                static_cast<unsigned char>(
                    character);

            if (!std::isalnum(byte) &&
                character != '_')
            {
                character = '_';
            }
        }

        if (value.empty() ||
            std::isdigit(
                static_cast<unsigned char>(
                    value.front())) ||
            IsCppKeyword(value))
        {
            value =
                "sub_" +
                value;
        }

        return value;
    }

    [[nodiscard]]
    static bool IsCppKeyword(
        const std::string& value) noexcept
    {
        static constexpr const char*
            Keywords[] =
            {
                "alignas", "alignof", "and",
                "asm", "auto", "bitand",
                "bitor", "bool", "break",
                "case", "catch", "char",
                "char8_t", "char16_t",
                "char32_t", "class",
                "compl", "concept", "const",
                "consteval", "constexpr",
                "constinit", "const_cast",
                "continue", "co_await",
                "co_return", "co_yield",
                "decltype", "default",
                "delete", "do", "double",
                "dynamic_cast", "else",
                "enum", "explicit", "export",
                "extern", "false", "float",
                "for", "friend", "goto",
                "if", "inline", "int",
                "long", "mutable",
                "namespace", "new",
                "noexcept", "not", "not_eq",
                "nullptr", "operator", "or",
                "or_eq", "private",
                "protected", "public",
                "register",
                "reinterpret_cast",
                "requires", "return",
                "short", "signed", "sizeof",
                "static", "static_assert",
                "static_cast", "struct",
                "switch", "template", "this",
                "thread_local", "throw",
                "true", "try", "typedef",
                "typeid", "typename", "union",
                "unsigned", "using",
                "virtual", "void", "volatile",
                "wchar_t", "while", "xor",
                "xor_eq"
            };

        for (const char* keyword :
             Keywords)
        {
            if (value == keyword)
            {
                return true;
            }
        }

        return false;
    }

    [[nodiscard]]
    static std::string Hex(
        u16 value,
        int width)
    {
        return
            "$" +
            HexDigits(
                value,
                width);
    }

    [[nodiscard]]
    static std::string HexDigits(
        u16 value,
        int width)
    {
        std::ostringstream stream;

        stream
            << std::uppercase
            << std::hex
            << std::setw(width)
            << std::setfill('0')
            << value;

        return stream.str();
    }
};

} // namespace atari
