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
    std::vector<u16> entryPoints;
    std::vector<u16> instructionAddresses;
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
            const u16 address = workList.front();
            workList.pop_front();

            const auto instruction =
                disassembler.Decode(
                    memory,
                    address);

            if (instruction.length == 0 ||
                instruction.length > 3)
            {
                continue;
            }

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
                        instruction.length +
                        offset;

                    return static_cast<u16>(target);
                };

            switch (instruction.instruction)
            {
            case cpu6502::Instruction::BCC:
            case cpu6502::Instruction::BCS:
            case cpu6502::Instruction::BEQ:
            case cpu6502::Instruction::BMI:
            case cpu6502::Instruction::BNE:
            case cpu6502::Instruction::BPL:
            case cpu6502::Instruction::BVC:
            case cpu6502::Instruction::BVS:

                enqueue(relativeTarget());
                enqueueNext();
                break;

            case cpu6502::Instruction::JSR:

                enqueue(absoluteTarget());
                enqueueNext();
                break;

            case cpu6502::Instruction::JMP:

                if (instruction.addressMode ==
                    cpu6502::AddressMode::Absolute)
                {
                    enqueue(absoluteTarget());
                }

                break;

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

        std::sort(
            result.instructionAddresses.begin(),
            result.instructionAddresses.end());

        result.instructionAddresses.erase(
            std::unique(
                result.instructionAddresses.begin(),
                result.instructionAddresses.end()),
            result.instructionAddresses.end());

        return result;
    }
};

} // namespace atari