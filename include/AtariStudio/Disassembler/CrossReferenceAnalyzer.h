#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

#include <AtariStudio/Core/Memory.h>
#include <AtariStudio/Core/Types.h>
#include <AtariStudio/Cpu6502/Instruction.h>
#include <AtariStudio/Disassembler/ControlFlowAnalyzer.h>
#include <AtariStudio/Disassembler/Disassembler.h>

namespace atari
{

enum class CrossReferenceType
{
    Branch,
    Call,
    Jump
};

struct CrossReference
{
    //
    // Где находится инструкция,
    // создающая ссылку.
    //
    u16 sourceAddress = 0;

    //
    // Адрес цели в нашем XEX listing.
    //
    u16 targetAddress = 0;

    //
    // Адрес, реально находящийся
    // в машинной инструкции.
    //
    u16 encodedTarget = 0;

    CrossReferenceType type =
        CrossReferenceType::Branch;

    //
    // true:
    //
    // encodedTarget является runtime-адресом,
    // а targetAddress был преобразован обратно
    // в source-адрес XEX.
    //
    bool relocated = false;
};

struct CrossReferenceAnalysisResult
{
    std::vector<CrossReference> references;
};

class CrossReferenceAnalyzer
{
public:

    [[nodiscard]]
    CrossReferenceAnalysisResult Analyze(
        const Memory& memory,
        const ControlFlowAnalysisResult& analysis) const
    {
        CrossReferenceAnalysisResult result;

        Disassembler disassembler;

        for (const u16 sourceAddress :
             analysis.instructionAddresses)
        {
            const auto instruction =
                disassembler.Decode(
                    memory,
                    sourceAddress);

            CrossReference reference;

            reference.sourceAddress =
                sourceAddress;

            bool found = false;

            switch (instruction.instruction)
            {
            //
            // Relative branches.
            //
            case cpu6502::Instruction::BCC:
            case cpu6502::Instruction::BCS:
            case cpu6502::Instruction::BEQ:
            case cpu6502::Instruction::BMI:
            case cpu6502::Instruction::BNE:
            case cpu6502::Instruction::BPL:
            case cpu6502::Instruction::BVC:
            case cpu6502::Instruction::BVS:
            {
                reference.type =
                    CrossReferenceType::Branch;

                reference.encodedTarget =
                    RelativeTarget(
                        instruction);

                reference.targetAddress =
                    reference.encodedTarget;

                found = true;

                break;
            }

            //
            // JSR
            //
            case cpu6502::Instruction::JSR:
            {
                reference.type =
                    CrossReferenceType::Call;

                reference.encodedTarget =
                    AbsoluteTarget(
                        instruction);

                reference.targetAddress =
                    ResolveTarget(
                        memory,
                        analysis,
                        reference.encodedTarget,
                        reference.relocated);

                found = true;

                break;
            }

            //
            // JMP absolute
            //
            case cpu6502::Instruction::JMP:
            {
                if (instruction.addressMode !=
                    cpu6502::AddressMode::Absolute)
                {
                    break;
                }

                reference.type =
                    CrossReferenceType::Jump;

                reference.encodedTarget =
                    AbsoluteTarget(
                        instruction);

                reference.targetAddress =
                    ResolveTarget(
                        memory,
                        analysis,
                        reference.encodedTarget,
                        reference.relocated);

                found = true;

                break;
            }

            default:
                break;
            }

            if (found)
            {
                result.references.push_back(
                    reference);
            }
        }

        std::sort(
            result.references.begin(),
            result.references.end(),
            [](const CrossReference& left,
               const CrossReference& right)
            {
                if (left.targetAddress !=
                    right.targetAddress)
                {
                    return
                        left.targetAddress <
                        right.targetAddress;
                }

                if (left.sourceAddress !=
                    right.sourceAddress)
                {
                    return
                        left.sourceAddress <
                        right.sourceAddress;
                }

                return
                    static_cast<int>(left.type) <
                    static_cast<int>(right.type);
            });

        result.references.erase(
            std::unique(
                result.references.begin(),
                result.references.end(),
                [](const CrossReference& left,
                   const CrossReference& right)
                {
                    return
                        left.sourceAddress ==
                            right.sourceAddress &&
                        left.targetAddress ==
                            right.targetAddress &&
                        left.encodedTarget ==
                            right.encodedTarget &&
                        left.type ==
                            right.type;
                }),
            result.references.end());

        return result;
    }

private:

    [[nodiscard]]
    static u16 AbsoluteTarget(
        const DisassembledInstruction& instruction)
    {
        return static_cast<u16>(
            static_cast<u16>(
                instruction.bytes[1]) |
            (static_cast<u16>(
                instruction.bytes[2]) << 8));
    }

    [[nodiscard]]
    static u16 RelativeTarget(
        const DisassembledInstruction& instruction)
    {
        const auto offset =
            static_cast<std::int8_t>(
                instruction.bytes[1]);

        const std::int32_t target =
            static_cast<std::int32_t>(
                instruction.address) +
            static_cast<std::int32_t>(
                instruction.length) +
            static_cast<std::int32_t>(
                offset);

        return static_cast<u16>(
            target);
    }

    [[nodiscard]]
    static u16 ResolveTarget(
        const Memory& memory,
        const ControlFlowAnalysisResult& analysis,
        u16 encodedTarget,
        bool& relocated)
    {
        relocated = false;

        const auto source =
            analysis.relocation.ResolveDestination(
                encodedTarget);

        if (!source.has_value())
        {
            return encodedTarget;
        }

        if (!memory.Cell(
                source.value()).initialized)
        {
            return encodedTarget;
        }

        relocated = true;

        return source.value();
    }
};

} // namespace atari