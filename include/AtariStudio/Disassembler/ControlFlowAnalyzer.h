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

namespace atari
{

struct ControlFlowAnalysisResult
{
    //
    // RUNAD / INITAD и другие точки входа.
    //
    std::vector<u16> entryPoints;

    //
    // Адреса реально достижимых инструкций.
    //
    std::vector<u16> instructionAddresses;

    //
    // Цели переходов:
    //
    // JSR
    // JMP absolute
    // BCC/BCS/BEQ/BMI/BNE/BPL/BVC/BVS
    //
    std::vector<u16> targetAddresses;
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

        std::array<bool, MemorySize> scheduled{};

        std::deque<u16> workList;

        auto enqueue =
            [&](u16 address)
            {
                if (scheduled[address])
                {
                    return;
                }

                if (!memory.Cell(address).initialized)
                {
                    return;
                }

                scheduled[address] = true;

                workList.push_back(address);
            };

        auto addTarget =
            [&](u16 address)
            {
                //
                // Метку создаём только для адреса,
                // который реально присутствует
                // в загруженной памяти.
                //
                if (!memory.Cell(address).initialized)
                {
                    return;
                }

                result.targetAddresses.push_back(address);
            };

        //
        // Добавляем точки входа.
        //
        for (const u16 entryPoint : entryPoints)
        {
            if (entryPoint != 0)
            {
                enqueue(entryPoint);
            }
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
            // Проверяем, что вся инструкция
            // находится в загруженной памяти.
            //
            bool completeInstruction = true;

            for (u16 i = 0;
                 i < instruction.length;
                 ++i)
            {
                const std::uint32_t byteAddress =
                    static_cast<std::uint32_t>(address) + i;

                if (byteAddress > 0xFFFF)
                {
                    completeInstruction = false;
                    break;
                }

                if (!memory.Cell(
                        static_cast<u16>(byteAddress)).initialized)
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
            // Помечаем байты как исполняемые.
            //
            for (u16 i = 0;
                 i < instruction.length;
                 ++i)
            {
                memory.Cell(
                    static_cast<u16>(address + i)).executable = true;
            }

            result.instructionAddresses.push_back(address);

            const std::uint32_t nextAddress =
                static_cast<std::uint32_t>(address) +
                instruction.length;

            auto enqueueNext =
                [&]()
                {
                    if (nextAddress <= 0xFFFF)
                    {
                        enqueue(
                            static_cast<u16>(nextAddress));
                    }
                };

            auto absoluteTarget =
                [&]() -> u16
                {
                    return static_cast<u16>(
                        static_cast<u16>(
                            instruction.bytes[1]) |
                        (static_cast<u16>(
                            instruction.bytes[2]) << 8));
                };

            auto relativeTarget =
                [&]() -> u16
                {
                    const auto offset =
                        static_cast<std::int8_t>(
                            instruction.bytes[1]);

                    const std::int32_t target =
                        static_cast<std::int32_t>(address) +
                        static_cast<std::int32_t>(
                            instruction.length) +
                        static_cast<std::int32_t>(offset);

                    return static_cast<u16>(target);
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
            // Subroutine call.
            //
            case cpu6502::Instruction::JSR:
            {
                const u16 target =
                    absoluteTarget();

                addTarget(target);
                enqueue(target);

                //
                // После RTS выполнение вернётся
                // к следующей инструкции.
                //
                enqueueNext();

                break;
            }

            //
            // Absolute JMP.
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

                //
                // JMP indirect пока не разрешаем.
                //
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

            //
            // Обычная последовательная инструкция.
            //
            default:
                enqueueNext();
                break;
            }
        }

        //
        // Сортируем и удаляем дубликаты.
        //
        std::sort(
            result.instructionAddresses.begin(),
            result.instructionAddresses.end());

        result.instructionAddresses.erase(
            std::unique(
                result.instructionAddresses.begin(),
                result.instructionAddresses.end()),
            result.instructionAddresses.end());

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