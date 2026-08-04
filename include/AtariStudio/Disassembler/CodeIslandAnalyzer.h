#pragma once

#include <algorithm>
#include <cstdint>
#include <deque>
#include <utility>
#include <vector>

#include <AtariStudio/Core/Project.h>
#include <AtariStudio/Core/Types.h>
#include <AtariStudio/Cpu6502/Instruction.h>
#include <AtariStudio/Disassembler/ControlFlowAnalyzer.h>
#include <AtariStudio/Disassembler/Disassembler.h>

namespace atari
{

class CodeIslandAnalyzer
{
public:

    void Analyze(
        Project& project,
        ControlFlowAnalysisResult& analysis) const
    {
        auto& memory =
            project.GetMemory();

        for (const auto& segment :
             project.Segments())
        {
            if (segment.type ==
                SegmentType::System)
            {
                continue;
            }

            if (segment.end < segment.begin)
            {
                continue;
            }

            //
            // Code islands ищем только в сегментах,
            // в которых уже найден хотя бы какой-то код.
            //
            if (!ContainsKnownCode(
                    segment,
                    analysis))
            {
                continue;
            }

            for (std::uint32_t address =
                     segment.begin;
                 address <= segment.end;
                 ++address)
            {
                const auto candidateAddress =
                    static_cast<u16>(address);

                //
                // Уже известный код пропускаем.
                //
                if (memory.Cell(
                        candidateAddress).executable)
                {
                    continue;
                }

                //
                // Нельзя начинать код с байта,
                // которого нет в XEX.
                //
                if (!memory.Cell(
                        candidateAddress).initialized)
                {
                    continue;
                }

                const auto candidate =
                    AnalyzeCandidate(
                        project,
                        segment,
                        candidateAddress,
                        analysis);

                if (!candidate.valid)
                {
                    continue;
                }

                //
                // Начало найденного code island
                // получает автоматическую метку.
                //
                analysis.targetAddresses.push_back(
                    candidateAddress);

                for (const auto instructionAddress :
                     candidate.instructions)
                {
                    const auto instruction =
                        m_disassembler.Decode(
                            memory,
                            instructionAddress);

                    //
                    // Помечаем все байты инструкции
                    // как исполняемые.
                    //
                    for (u16 i = 0;
                         i < instruction.length;
                         ++i)
                    {
                        memory.Cell(
                            static_cast<u16>(
                                instructionAddress + i))
                            .executable = true;
                    }

                    analysis.instructionAddresses.push_back(
                        instructionAddress);
                }

                for (const auto target :
                     candidate.targets)
                {
                    analysis.targetAddresses.push_back(
                        target);
                }
            }
        }

        SortUnique(
            analysis.instructionAddresses);

        SortUnique(
            analysis.targetAddresses);
    }

private:

    struct CandidateResult
    {
        bool valid = false;

        std::vector<u16> instructions;
        std::vector<u16> targets;
    };

    [[nodiscard]]
    static bool ContainsKnownCode(
        const Segment& segment,
        const ControlFlowAnalysisResult& analysis)
    {
        const auto iterator =
            std::lower_bound(
                analysis.instructionAddresses.begin(),
                analysis.instructionAddresses.end(),
                segment.begin);

        return
            iterator !=
                analysis.instructionAddresses.end() &&
            *iterator <= segment.end;
    }

    [[nodiscard]]
    static bool AddressInside(
        const Segment& segment,
        u16 address) noexcept
    {
        return
            address >= segment.begin &&
            address <= segment.end;
    }

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
    static u16 ResolveRuntimeTarget(
        const Memory& memory,
        const ControlFlowAnalysisResult& analysis,
        u16 target)
    {
        const auto source =
            analysis.relocation.ResolveDestination(
                target);

        if (source.has_value() &&
            memory.Cell(source.value()).initialized)
        {
            return source.value();
        }

        return target;
    }

    [[nodiscard]]
    CandidateResult AnalyzeCandidate(
        const Project& project,
        const Segment& segment,
        u16 startAddress,
        const ControlFlowAnalysisResult& analysis) const
    {
        CandidateResult result;

        const auto& memory =
            project.GetMemory();

        //
        // Слишком короткие последовательности
        // легко возникают в обычных данных.
        //
        constexpr std::size_t minimumInstructions = 6;

        //
        // Защита от ложного огромного блока.
        //
        constexpr std::size_t maximumInstructions = 256;

        std::vector<bool> visited(
            MemorySize,
            false);

        std::deque<u16> workList;

        //
        // Для проверки пересечения инструкций.
        //
        std::vector<std::pair<u16, u16>>
            instructionRanges;

        bool hasStrongTerminal = false;
        std::size_t controlFlowInstructions = 0;

        workList.push_back(
            startAddress);

        while (!workList.empty())
        {
            const u16 address =
                workList.front();

            workList.pop_front();

            //
            // Вышли за анализируемый XEX-сегмент.
            //
            if (!AddressInside(
                    segment,
                    address))
            {
                return result;
            }

            //
            // Ветка попала в уже известный код.
            // Это допустимый выход из island.
            //
            if (memory.Cell(address).executable)
            {
                hasStrongTerminal = true;
                continue;
            }

            if (visited[address])
            {
                continue;
            }

            visited[address] = true;

            if (!memory.Cell(address).initialized)
            {
                return result;
            }

            const auto instruction =
                m_disassembler.Decode(
                    memory,
                    address);

            //
            // Illegal opcode — очень сильный
            // признак того, что это не код.
            //
            if (instruction.instruction ==
                cpu6502::Instruction::Illegal)
            {
                return result;
            }

            if (instruction.length == 0 ||
                instruction.length > 3)
            {
                return result;
            }

            const std::uint32_t instructionEnd32 =
                static_cast<std::uint32_t>(
                    address) +
                instruction.length - 1;

            if (instructionEnd32 > 0xFFFF)
            {
                return result;
            }

            const u16 instructionEnd =
                static_cast<u16>(
                    instructionEnd32);

            //
            // Вся инструкция должна находиться
            // внутри текущего XEX-сегмента.
            //
            if (!AddressInside(
                    segment,
                    instructionEnd))
            {
                return result;
            }

            //
            // Все байты должны реально существовать.
            //
            for (u16 i = 0;
                 i < instruction.length;
                 ++i)
            {
                const u16 byteAddress =
                    static_cast<u16>(
                        address + i);

                if (!memory.Cell(
                        byteAddress).initialized)
                {
                    return result;
                }
            }

            //
            // Ветка не должна попадать внутрь
            // уже декодированной инструкции.
            //
            for (const auto& range :
                 instructionRanges)
            {
                const bool overlaps =
                    !(instructionEnd < range.first ||
                      address > range.second);

                if (overlaps &&
                    address != range.first)
                {
                    return result;
                }
            }

            instructionRanges.emplace_back(
                address,
                instructionEnd);

            result.instructions.push_back(
                address);

            if (result.instructions.size() >
                maximumInstructions)
            {
                return CandidateResult{};
            }

            const std::uint32_t nextAddress32 =
                static_cast<std::uint32_t>(
                    address) +
                instruction.length;

            const auto enqueueNext =
                [&]() -> bool
                {
                    if (nextAddress32 > 0xFFFF)
                    {
                        return false;
                    }

                    const u16 nextAddress =
                        static_cast<u16>(
                            nextAddress32);

                    if (!AddressInside(
                            segment,
                            nextAddress))
                    {
                        return false;
                    }

                    workList.push_back(
                        nextAddress);

                    return true;
                };

            const auto enqueueTarget =
                [&](u16 target) -> bool
                {
                    //
                    // Неизвестная ветка назад за начало
                    // candidate обычно означает, что мы
                    // начали декодирование посреди данных.
                    //
                    if (target < startAddress &&
                        !memory.Cell(target).executable)
                    {
                        return false;
                    }

                    if (AddressInside(
                            segment,
                            target))
                    {
                        workList.push_back(
                            target);

                        return true;
                    }

                    //
                    // Выход на уже известный код
                    // вне текущего сегмента допустим.
                    //
                    if (memory.Cell(target).executable)
                    {
                        hasStrongTerminal = true;

                        return true;
                    }

                    return false;
                };

            switch (instruction.instruction)
            {
            //
            // Conditional branches.
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
                ++controlFlowInstructions;

                const u16 target =
                    RelativeTarget(
                        instruction);

                result.targets.push_back(
                    target);

                if (!enqueueTarget(target))
                {
                    return CandidateResult{};
                }

                if (!enqueueNext())
                {
                    return CandidateResult{};
                }

                break;
            }

            //
            // JSR does not terminate the local path.
            //
            case cpu6502::Instruction::JSR:
            {
                ++controlFlowInstructions;

                const u16 runtimeTarget =
                    AbsoluteTarget(
                        instruction);

                const u16 resolvedTarget =
                    ResolveRuntimeTarget(
                        memory,
                        analysis,
                        runtimeTarget);

                if (memory.Cell(
                        resolvedTarget).initialized)
                {
                    result.targets.push_back(
                        resolvedTarget);
                }

                if (!enqueueNext())
                {
                    return CandidateResult{};
                }

                break;
            }

            //
            // JMP terminates the current sequential path.
            //
            case cpu6502::Instruction::JMP:
            {
                ++controlFlowInstructions;
                hasStrongTerminal = true;

                if (instruction.addressMode ==
                    cpu6502::AddressMode::Absolute)
                {
                    const u16 runtimeTarget =
                        AbsoluteTarget(
                            instruction);

                    const u16 resolvedTarget =
                        ResolveRuntimeTarget(
                            memory,
                            analysis,
                            runtimeTarget);

                    if (memory.Cell(
                            resolvedTarget).initialized)
                    {
                        result.targets.push_back(
                            resolvedTarget);
                    }

                    //
                    // JMP внутри этого сегмента:
                    // продолжаем CFG.
                    //
                    if (AddressInside(
                            segment,
                            resolvedTarget))
                    {
                        if (resolvedTarget <
                                startAddress &&
                            !memory.Cell(
                                resolvedTarget)
                                 .executable)
                        {
                            return CandidateResult{};
                        }

                        workList.push_back(
                            resolvedTarget);
                    }
                }

                break;
            }

            //
            // Хорошие признаки конца процедуры.
            //
            case cpu6502::Instruction::RTS:
            case cpu6502::Instruction::RTI:

                ++controlFlowInstructions;
                hasStrongTerminal = true;
                break;

            //
            // BRK допустим как opcode,
            // но сам по себе не доказывает наличие кода.
            //
            case cpu6502::Instruction::BRK:

                ++controlFlowInstructions;
                break;

            default:

                if (!enqueueNext())
                {
                    return CandidateResult{};
                }

                break;
            }
        }

        //
        // Небольшой набор эвристик против
        // случайного дизассемблирования данных.
        //
        if (result.instructions.size() <
            minimumInstructions)
        {
            return CandidateResult{};
        }

        if (controlFlowInstructions == 0)
        {
            return CandidateResult{};
        }

        if (!hasStrongTerminal)
        {
            return CandidateResult{};
        }

        SortUnique(
            result.instructions);

        SortUnique(
            result.targets);

        result.valid = true;

        return result;
    }

    static void SortUnique(
        std::vector<u16>& values)
    {
        std::sort(
            values.begin(),
            values.end());

        values.erase(
            std::unique(
                values.begin(),
                values.end()),
            values.end());
    }

    Disassembler m_disassembler;
};

} // namespace atari
