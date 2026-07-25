#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <vector>

#include <AtariStudio/Core/Memory.h>
#include <AtariStudio/Core/Types.h>
#include <AtariStudio/Cpu6502/Instruction.h>
#include <AtariStudio/Disassembler/Disassembler.h>
#include <AtariStudio/Disassembler/RelocationAnalyzer.h>

namespace atari
{

struct ControlFlowAnalysisResult
{
    std::vector<u16> entryPoints;
    std::vector<u16> instructionAddresses;
    std::vector<u16> targetAddresses;

    RelocationAnalysisResult relocation;
};

class ControlFlowAnalyzer
{
public:

    [[nodiscard]]
    ControlFlowAnalysisResult Analyze(
        Memory& memory,
        const std::vector<u16>& entryPoints) const
    {
        ControlFlowAnalysisResult result;

        result.entryPoints = entryPoints;

        //
        // Сначала ищем relocation/copy loops.
        //
        RelocationAnalyzer relocationAnalyzer;

        result.relocation =
            relocationAnalyzer.Analyze(memory);

        std::array<bool, MemorySize> scheduled{};
        std::deque<u16> workList;

        //
        // Runtime address -> original XEX address.
        //
        const auto resolveAddress =
            [&](u16 address) -> u16
            {
                const auto source =
                    result.relocation.ResolveDestination(
                        address);

                if (source.has_value() &&
                    memory.Cell(source.value()).initialized)
                {
                    return source.value();
                }

                return address;
            };

        const auto enqueue =
            [&](u16 address)
            {
                const u16 resolvedAddress =
                    resolveAddress(address);

                if (scheduled[resolvedAddress])
                {
                    return;
                }

                if (!memory.Cell(
                        resolvedAddress).initialized)
                {
                    return;
                }

                scheduled[resolvedAddress] = true;

                workList.push_back(
                    resolvedAddress);
            };

        const auto addTarget =
            [&](u16 address)
            {
                const u16 resolvedAddress =
                    resolveAddress(address);

                if (!memory.Cell(
                        resolvedAddress).initialized)
                {
                    return;
                }

                result.targetAddresses.push_back(
                    resolvedAddress);
            };

        //
        // RUNAD / INITAD.
        //
        for (const u16 entryPoint : entryPoints)
        {
            if (entryPoint != 0)
            {
                enqueue(entryPoint);
            }
        }

        //
        // IMPORTANT
        //
        // Каждый найденный relocation block может
        // содержать отдельный исполняемый модуль.
        //
        // Например hello.xex:
        //
        //   $0500 -> $0700
        //   $0592 -> $0800
        //
        // Второй блок невозможно гарантированно
        // обнаружить только обычным CFG, потому что
        // вход в него может происходить уже после
        // перемещения программы или косвенно.
        //
        // Поэтому начало каждого source-блока
        // считаем дополнительной точкой анализа.
        //
        for (const auto& range :
             result.relocation.ranges)
        {
            if (!memory.Cell(
                    range.sourceBegin).initialized)
            {
                continue;
            }

            enqueue(
                range.sourceBegin);
        }

        Disassembler disassembler;

        while (!workList.empty())
        {
            const u16 address =
                workList.front();

            workList.pop_front();

            if (!memory.Cell(address).initialized)
            {
                continue;
            }

            const auto instruction =
                disassembler.Decode(
                    memory,
                    address);

            if (instruction.length == 0 ||
                instruction.length > 3)
            {
                continue;
            }

            //
            // Проверяем, что все байты инструкции
            // действительно загружены.
            //
            bool completeInstruction = true;

            for (u16 i = 0;
                 i < instruction.length;
                 ++i)
            {
                const std::uint32_t byteAddress =
                    static_cast<std::uint32_t>(
                        address) + i;

                if (byteAddress > 0xFFFF)
                {
                    completeInstruction = false;
                    break;
                }

                if (!memory.Cell(
                        static_cast<u16>(
                            byteAddress)).initialized)
                {
                    completeInstruction = false;
                    break;
                }
            }

            if (!completeInstruction)
            {
                continue;
            }

            //
            // Помечаем байты инструкции как CODE.
            //
            for (u16 i = 0;
                 i < instruction.length;
                 ++i)
            {
                memory.Cell(
                    static_cast<u16>(
                        address + i)).executable = true;
            }

            result.instructionAddresses.push_back(
                address);

            const std::uint32_t nextAddress =
                static_cast<std::uint32_t>(
                    address) +
                instruction.length;

            const auto enqueueNext =
                [&]()
                {
                    if (nextAddress <= 0xFFFF)
                    {
                        enqueue(
                            static_cast<u16>(
                                nextAddress));
                    }
                };

            const auto absoluteTarget =
                [&]() -> u16
                {
                    return static_cast<u16>(
                        static_cast<u16>(
                            instruction.bytes[1]) |
                        (static_cast<u16>(
                            instruction.bytes[2]) << 8));
                };

            const auto relativeTarget =
                [&]() -> u16
                {
                    const auto offset =
                        static_cast<std::int8_t>(
                            instruction.bytes[1]);

                    const std::int32_t target =
                        static_cast<std::int32_t>(
                            address) +
                        static_cast<std::int32_t>(
                            instruction.length) +
                        static_cast<std::int32_t>(
                            offset);

                    return static_cast<u16>(
                        target);
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
                const u16 target =
                    relativeTarget();

                addTarget(target);
                enqueue(target);

                enqueueNext();

                break;
            }

            //
            // Subroutine.
            //
            case cpu6502::Instruction::JSR:
            {
                const u16 target =
                    absoluteTarget();

                addTarget(target);
                enqueue(target);

                enqueueNext();

                break;
            }

            //
            // JMP absolute.
            //
            case cpu6502::Instruction::JMP:
            {
                if (instruction.addressMode ==
                    cpu6502::AddressMode::Absolute)
                {
                    const u16 target =
                        absoluteTarget();

                    addTarget(target);
                    enqueue(target);
                }

                break;
            }

            //
            // Конец текущего пути.
            //
            case cpu6502::Instruction::RTS:
            case cpu6502::Instruction::RTI:
            case cpu6502::Instruction::BRK:
            case cpu6502::Instruction::Illegal:
                break;

            default:

                enqueueNext();
                break;
            }
        }

        //
        // Sort + unique instructions.
        //
        std::sort(
            result.instructionAddresses.begin(),
            result.instructionAddresses.end());

        result.instructionAddresses.erase(
            std::unique(
                result.instructionAddresses.begin(),
                result.instructionAddresses.end()),
            result.instructionAddresses.end());

        //
        // Sort + unique targets.
        //
        std::sort(
            result.targetAddresses.begin(),
            result.targetAddresses.end());

        result.targetAddresses.erase(
            std::unique(
                result.targetAddresses.begin(),
                result.targetAddresses.end()),
            result.targetAddresses.end());

        return result;
    }
};

} // namespace atari