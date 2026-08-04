#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include <AtariStudio/Core/Memory.h>
#include <AtariStudio/Core/Types.h>

namespace atari
{

enum class DisplayListInstructionKind
{
    Blank,
    ModeLine,
    Jump,
    JumpAndWaitForVerticalBlank
};

enum class DisplayListStopReason
{
    None,
    JumpAndWaitForVerticalBlank,
    UninitializedMemory,
    TruncatedInstruction,
    AddressSpaceBoundary,
    OneKilobyteBoundary,
    LoopDetected,
    InstructionLimit
};

struct DisplayListInstruction
{
    u16 address = 0;
    u8 opcode = 0;
    u8 mode = 0;
    u8 length = 1;

    DisplayListInstructionKind kind =
        DisplayListInstructionKind::Blank;

    bool displayListInterrupt = false;
    bool horizontalScroll = false;
    bool verticalScroll = false;
    bool loadMemoryScan = false;
    bool reservedJumpModifier = false;

    u8 blankScanLines = 0;

    std::optional<u16> jumpAddress;
    std::optional<u16> memoryScanAddress;
};

struct DisplayListAnalysis
{
    u16 entryPoint = 0;

    std::vector<DisplayListInstruction>
        instructions;

    DisplayListStopReason stopReason =
        DisplayListStopReason::None;

    std::optional<u16> stopAddress;

    [[nodiscard]]
    std::size_t ByteCount() const noexcept
    {
        std::size_t result = 0;

        for (const auto& instruction :
             instructions)
        {
            result += instruction.length;
        }

        return result;
    }

    [[nodiscard]]
    bool Complete() const noexcept
    {
        return
            stopReason ==
            DisplayListStopReason::
                JumpAndWaitForVerticalBlank;
    }
};

struct DisplayListAnalysisResult
{
    std::vector<u16> entryPoints;

    std::vector<DisplayListAnalysis>
        displayLists;

    std::vector<u16> screenMemoryAddresses;

    [[nodiscard]]
    std::size_t InstructionCount() const noexcept
    {
        std::size_t result = 0;

        for (const auto& displayList :
             displayLists)
        {
            result +=
                displayList.instructions.size();
        }

        return result;
    }

    [[nodiscard]]
    std::size_t CompleteCount() const noexcept
    {
        return static_cast<std::size_t>(
            std::count_if(
                displayLists.begin(),
                displayLists.end(),
                [](const DisplayListAnalysis&
                       displayList)
                {
                    return displayList.Complete();
                }));
    }
};

class DisplayListAnalyzer
{
public:

    static constexpr u16
        OsDisplayListPointerAddress = 0x0230;

    static constexpr u16
        AnticDisplayListPointerAddress = 0xD402;

    static constexpr std::size_t
        DefaultInstructionLimit = 1024;

    [[nodiscard]]
    static std::vector<u16> DiscoverEntryPoints(
        const Memory& memory)
    {
        std::vector<u16> result;

        AddPointerEntryPoint(
            memory,
            OsDisplayListPointerAddress,
            result);

        AddPointerEntryPoint(
            memory,
            AnticDisplayListPointerAddress,
            result);

        SortAndRemoveDuplicates(result);

        return result;
    }

    [[nodiscard]]
    DisplayListAnalysisResult Analyze(
        const Memory& memory) const
    {
        return Analyze(
            memory,
            DiscoverEntryPoints(memory),
            DefaultInstructionLimit);
    }

    [[nodiscard]]
    DisplayListAnalysisResult AnalyzeDiscovered(
        const Memory& memory,
        std::size_t instructionLimit) const
    {
        return Analyze(
            memory,
            DiscoverEntryPoints(memory),
            instructionLimit);
    }

    [[nodiscard]]
    DisplayListAnalysisResult Analyze(
        const Memory& memory,
        std::vector<u16> entryPoints,
        std::size_t instructionLimit =
            DefaultInstructionLimit) const
    {
        DisplayListAnalysisResult result;

        SortAndRemoveDuplicates(entryPoints);

        result.entryPoints = entryPoints;

        for (const u16 entryPoint :
             result.entryPoints)
        {
            auto displayList =
                AnalyzeOne(
                    memory,
                    entryPoint,
                    instructionLimit);

            for (const auto& instruction :
                 displayList.instructions)
            {
                if (instruction.memoryScanAddress.
                        has_value())
                {
                    result.screenMemoryAddresses.
                        push_back(
                            instruction.
                                memoryScanAddress.
                                value());
                }
            }

            result.displayLists.push_back(
                std::move(displayList));
        }

        SortAndRemoveDuplicates(
            result.screenMemoryAddresses);

        return result;
    }

private:

    static void AddPointerEntryPoint(
        const Memory& memory,
        u16 pointerAddress,
        std::vector<u16>& entryPoints)
    {
        const auto highAddress =
            static_cast<u16>(
                pointerAddress + 1);

        if (!memory.Cell(pointerAddress).
                initialized ||
            !memory.Cell(highAddress).
                initialized)
        {
            return;
        }

        entryPoints.push_back(
            memory.Read16(pointerAddress));
    }

    static void SortAndRemoveDuplicates(
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

    [[nodiscard]]
    static bool CrossesOneKilobyteBoundary(
        u16 begin,
        std::uint32_t end) noexcept
    {
        return
            (static_cast<std::uint32_t>(
                 begin) &
             0xFC00U) !=
            (end & 0xFC00U);
    }

    [[nodiscard]]
    static DisplayListAnalysis AnalyzeOne(
        const Memory& memory,
        u16 entryPoint,
        std::size_t instructionLimit)
    {
        DisplayListAnalysis result;
        result.entryPoint = entryPoint;

        std::vector<bool> visited(
            MemorySize,
            false);

        u16 address = entryPoint;

        while (result.instructions.size() <
               instructionLimit)
        {
            if (visited[address])
            {
                result.stopReason =
                    DisplayListStopReason::
                        LoopDetected;

                result.stopAddress = address;
                return result;
            }

            if (!memory.Cell(address).initialized)
            {
                result.stopReason =
                    DisplayListStopReason::
                        UninitializedMemory;

                result.stopAddress = address;
                return result;
            }

            visited[address] = true;

            DisplayListInstruction instruction;
            instruction.address = address;
            instruction.opcode =
                memory.Read8(address);
            instruction.mode =
                static_cast<u8>(
                    instruction.opcode & 0x0F);
            instruction.displayListInterrupt =
                (instruction.opcode & 0x80) != 0;

            if (instruction.mode == 0)
            {
                instruction.kind =
                    DisplayListInstructionKind::
                        Blank;

                instruction.blankScanLines =
                    static_cast<u8>(
                        ((instruction.opcode >> 4) &
                          0x07) +
                        1);
            }
            else if (instruction.mode == 1)
            {
                instruction.length = 3;
                instruction.reservedJumpModifier =
                    (instruction.opcode & 0x30) != 0;

                if ((instruction.opcode & 0x40) != 0)
                {
                    instruction.kind =
                        DisplayListInstructionKind::
                            JumpAndWaitForVerticalBlank;
                }
                else
                {
                    instruction.kind =
                        DisplayListInstructionKind::
                            Jump;
                }
            }
            else
            {
                instruction.kind =
                    DisplayListInstructionKind::
                        ModeLine;

                instruction.horizontalScroll =
                    (instruction.opcode & 0x10) != 0;

                instruction.verticalScroll =
                    (instruction.opcode & 0x20) != 0;

                instruction.loadMemoryScan =
                    (instruction.opcode & 0x40) != 0;

                if (instruction.loadMemoryScan)
                {
                    instruction.length = 3;
                }
            }

            const std::uint32_t instructionEnd =
                static_cast<std::uint32_t>(
                    address) +
                instruction.length - 1;

            if (instructionEnd >= MemorySize)
            {
                result.stopReason =
                    DisplayListStopReason::
                        AddressSpaceBoundary;

                result.stopAddress = address;
                return result;
            }

            if (instruction.length == 3)
            {
                const auto operandLowAddress =
                    static_cast<u16>(
                        static_cast<std::uint32_t>(
                            address) +
                        1);

                const auto operandHighAddress =
                    static_cast<u16>(
                        static_cast<std::uint32_t>(
                            address) +
                        2);

                if (!memory.Cell(operandLowAddress).
                        initialized ||
                    !memory.Cell(operandHighAddress).
                        initialized)
                {
                    result.stopReason =
                        DisplayListStopReason::
                            TruncatedInstruction;

                    result.stopAddress =
                        !memory.Cell(operandLowAddress).
                            initialized
                            ? operandLowAddress
                            : operandHighAddress;

                    return result;
                }

                const u16 operand =
                    memory.Read16(
                        operandLowAddress);

                if (instruction.kind ==
                        DisplayListInstructionKind::
                            ModeLine)
                {
                    instruction.memoryScanAddress =
                        operand;
                }
                else
                {
                    instruction.jumpAddress = operand;
                }
            }

            result.instructions.push_back(
                instruction);

            if (CrossesOneKilobyteBoundary(
                    address,
                    instructionEnd))
            {
                result.stopReason =
                    DisplayListStopReason::
                        OneKilobyteBoundary;

                result.stopAddress = address;
                return result;
            }

            if (instruction.kind ==
                DisplayListInstructionKind::
                    JumpAndWaitForVerticalBlank)
            {
                result.stopReason =
                    DisplayListStopReason::
                        JumpAndWaitForVerticalBlank;

                result.stopAddress =
                    instruction.jumpAddress;

                return result;
            }

            if (instruction.kind ==
                DisplayListInstructionKind::Jump)
            {
                address =
                    instruction.jumpAddress.value();

                continue;
            }

            const std::uint32_t nextAddress =
                instructionEnd + 1;

            if (nextAddress >= MemorySize)
            {
                result.stopReason =
                    DisplayListStopReason::
                        AddressSpaceBoundary;

                result.stopAddress = address;
                return result;
            }

            if (CrossesOneKilobyteBoundary(
                    address,
                    nextAddress))
            {
                result.stopReason =
                    DisplayListStopReason::
                        OneKilobyteBoundary;

                result.stopAddress =
                    static_cast<u16>(
                        nextAddress);

                return result;
            }

            address =
                static_cast<u16>(
                    nextAddress);
        }

        result.stopReason =
            DisplayListStopReason::
                InstructionLimit;

        result.stopAddress = address;

        return result;
    }
};

} // namespace atari
