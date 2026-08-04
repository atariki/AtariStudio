#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <AtariStudio/Core/Project.h>
#include <AtariStudio/Core/Types.h>
#include <AtariStudio/Cpu6502/AddressMode.h>
#include <AtariStudio/Cpu6502/Instruction.h>
#include <AtariStudio/Disassembler/ControlFlowAnalyzer.h>
#include <AtariStudio/Disassembler/Disassembler.h>
#include <AtariStudio/Disassembler/RoutineAnalyzer.h>

namespace atari
{

enum class BasicBlockEdgeType
{
    FallThrough,
    BranchTaken,
    Jump
};

struct BasicBlockEdge
{
    u16 targetAddress = 0;

    BasicBlockEdgeType type =
        BasicBlockEdgeType::FallThrough;
};

struct BasicBlock
{
    //
    // First instruction address.
    //
    u16 beginAddress = 0;

    //
    // Last BYTE occupied by the block.
    //
    u16 endAddress = 0;

    //
    // Instruction start addresses.
    //
    std::vector<u16> instructionAddresses;

    //
    // Outgoing CFG edges.
    //
    std::vector<BasicBlockEdge> successors;

    //
    // True when execution cannot continue to another
    // block inside this routine.
    //
    bool terminal = false;

    [[nodiscard]]
    std::size_t InstructionCount() const noexcept
    {
        return instructionAddresses.size();
    }

    [[nodiscard]]
    std::uint32_t Size() const noexcept
    {
        if (endAddress < beginAddress)
        {
            return 0;
        }

        return
            static_cast<std::uint32_t>(
                endAddress) -
            static_cast<std::uint32_t>(
                beginAddress) +
            1;
    }
};

struct RoutineBasicBlocks
{
    u16 routineEntryAddress = 0;

    std::string routineName;

    std::vector<BasicBlock> blocks;

    [[nodiscard]]
    const BasicBlock* FindBlock(
        u16 beginAddress) const noexcept
    {
        const auto iterator =
            std::find_if(
                blocks.begin(),
                blocks.end(),
                [beginAddress](
                    const BasicBlock& block)
                {
                    return
                        block.beginAddress ==
                        beginAddress;
                });

        if (iterator == blocks.end())
        {
            return nullptr;
        }

        return &*iterator;
    }
};

struct BasicBlockAnalysisResult
{
    std::vector<RoutineBasicBlocks> routines;

    [[nodiscard]]
    std::size_t BlockCount() const noexcept
    {
        std::size_t count = 0;

        for (const auto& routine :
             routines)
        {
            count +=
                routine.blocks.size();
        }

        return count;
    }

    [[nodiscard]]
    const RoutineBasicBlocks* FindRoutine(
        u16 entryAddress) const noexcept
    {
        const auto iterator =
            std::find_if(
                routines.begin(),
                routines.end(),
                [entryAddress](
                    const RoutineBasicBlocks& routine)
                {
                    return
                        routine.routineEntryAddress ==
                        entryAddress;
                });

        if (iterator == routines.end())
        {
            return nullptr;
        }

        return &*iterator;
    }
};

class BasicBlockAnalyzer
{
public:

    [[nodiscard]]
    BasicBlockAnalysisResult Analyze(
        const Project& project,
        const ControlFlowAnalysisResult& controlFlow,
        const RoutineAnalysisResult& routines) const
    {
        BasicBlockAnalysisResult result;

        for (const auto& routine :
             routines.routines)
        {
            RoutineBasicBlocks routineBlocks;

            routineBlocks.routineEntryAddress =
                routine.entryAddress;

            routineBlocks.routineName =
                routine.name;

            BuildRoutineBlocks(
                project,
                controlFlow,
                routine,
                routineBlocks);

            result.routines.push_back(
                std::move(routineBlocks));
        }

        return result;
    }

private:

    [[nodiscard]]
    static bool IsRoutineInstruction(
        const Routine& routine,
        u16 address)
    {
        return std::binary_search(
            routine.instructionAddresses.begin(),
            routine.instructionAddresses.end(),
            address);
    }

    [[nodiscard]]
    static bool IsLeader(
        const std::vector<u16>& leaders,
        u16 address)
    {
        return std::binary_search(
            leaders.begin(),
            leaders.end(),
            address);
    }

    [[nodiscard]]
    static bool IsConditionalBranch(
        cpu6502::Instruction instruction)
    {
        switch (instruction)
        {
        case cpu6502::Instruction::BCC:
        case cpu6502::Instruction::BCS:
        case cpu6502::Instruction::BEQ:
        case cpu6502::Instruction::BMI:
        case cpu6502::Instruction::BNE:
        case cpu6502::Instruction::BPL:
        case cpu6502::Instruction::BVC:
        case cpu6502::Instruction::BVS:

            return true;

        default:

            return false;
        }
    }

    [[nodiscard]]
    static bool IsHardTerminator(
        const DisassembledInstruction& instruction)
    {
        if (IsConditionalBranch(
                instruction.instruction))
        {
            return true;
        }

        switch (instruction.instruction)
        {
        case cpu6502::Instruction::JMP:
        case cpu6502::Instruction::RTS:
        case cpu6502::Instruction::RTI:
        case cpu6502::Instruction::BRK:
        case cpu6502::Instruction::Illegal:

            return true;

        default:

            return false;
        }
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
    static u16 ResolveAbsoluteTarget(
        const ControlFlowAnalysisResult& controlFlow,
        u16 encodedTarget)
    {
        const auto source =
            controlFlow.relocation.
                ResolveDestination(
                    encodedTarget);

        if (!source.has_value())
        {
            return encodedTarget;
        }

        return source.value();
    }

    [[nodiscard]]
    static bool NextInstructionAddress(
        const Routine& routine,
        const DisassembledInstruction& instruction,
        u16& nextAddress)
    {
        const std::uint32_t next =
            static_cast<std::uint32_t>(
                instruction.address) +
            instruction.length;

        if (next > 0xFFFF)
        {
            return false;
        }

        nextAddress =
            static_cast<u16>(
                next);

        return
            IsRoutineInstruction(
                routine,
                nextAddress);
    }

    static void AddLeader(
        std::vector<u16>& leaders,
        const Routine& routine,
        u16 address)
    {
        if (!IsRoutineInstruction(
                routine,
                address))
        {
            return;
        }

        leaders.push_back(
            address);
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

    static void SortUniqueEdges(
        std::vector<BasicBlockEdge>& edges)
    {
        std::sort(
            edges.begin(),
            edges.end(),
            [](const BasicBlockEdge& left,
               const BasicBlockEdge& right)
            {
                if (left.targetAddress !=
                    right.targetAddress)
                {
                    return
                        left.targetAddress <
                        right.targetAddress;
                }

                return
                    static_cast<int>(left.type) <
                    static_cast<int>(right.type);
            });

        edges.erase(
            std::unique(
                edges.begin(),
                edges.end(),
                [](const BasicBlockEdge& left,
                   const BasicBlockEdge& right)
                {
                    return
                        left.targetAddress ==
                            right.targetAddress &&
                        left.type ==
                            right.type;
                }),
            edges.end());
    }

    static void BuildRoutineBlocks(
        const Project& project,
        const ControlFlowAnalysisResult& controlFlow,
        const Routine& routine,
        RoutineBasicBlocks& result)
    {
        if (routine.instructionAddresses.empty())
        {
            return;
        }

        const auto& memory =
            project.GetMemory();

        Disassembler disassembler;

        //
        // =================================================
        // Phase 1:
        // Find basic-block leaders
        // =================================================
        //
        std::vector<u16> leaders;

        AddLeader(
            leaders,
            routine,
            routine.entryAddress);

        for (const u16 address :
             routine.instructionAddresses)
        {
            const auto instruction =
                disassembler.Decode(
                    memory,
                    address);

            if (instruction.length == 0)
            {
                continue;
            }

            //
            // Conditional branch:
            //
            //   branch target       = leader
            //   fall-through target = leader
            //
            if (IsConditionalBranch(
                    instruction.instruction))
            {
                AddLeader(
                    leaders,
                    routine,
                    RelativeTarget(
                        instruction));

                u16 nextAddress = 0;

                if (NextInstructionAddress(
                        routine,
                        instruction,
                        nextAddress))
                {
                    AddLeader(
                        leaders,
                        routine,
                        nextAddress);
                }

                continue;
            }

            //
            // Direct JMP target is a leader when it remains
            // inside this routine.
            //
            if (instruction.instruction ==
                    cpu6502::Instruction::JMP &&
                instruction.addressMode ==
                    cpu6502::AddressMode::Absolute)
            {
                const u16 target =
                    ResolveAbsoluteTarget(
                        controlFlow,
                        AbsoluteTarget(
                            instruction));

                AddLeader(
                    leaders,
                    routine,
                    target);
            }
        }

        SortUnique(
            leaders);

        //
        // =================================================
        // Phase 2:
        // Build blocks from leaders
        // =================================================
        //
        std::vector<bool> consumed(
            MemorySize,
            false);

        for (const u16 leader :
             leaders)
        {
            if (consumed[leader])
            {
                continue;
            }

            BasicBlock block;

            block.beginAddress =
                leader;

            block.endAddress =
                leader;

            u16 current =
                leader;

            while (true)
            {
                if (!IsRoutineInstruction(
                        routine,
                        current))
                {
                    break;
                }

                if (consumed[current])
                {
                    break;
                }

                consumed[current] = true;

                const auto instruction =
                    disassembler.Decode(
                        memory,
                        current);

                if (instruction.length == 0)
                {
                    break;
                }

                block.instructionAddresses.
                    push_back(
                        current);

                const std::uint32_t instructionEnd =
                    static_cast<std::uint32_t>(
                        current) +
                    instruction.length - 1;

                if (instructionEnd <= 0xFFFF)
                {
                    block.endAddress =
                        static_cast<u16>(
                            instructionEnd);
                }

                //
                // Branch/JMP/RTS/etc ends the block.
                //
                if (IsHardTerminator(
                        instruction))
                {
                    break;
                }

                u16 nextAddress = 0;

                if (!NextInstructionAddress(
                        routine,
                        instruction,
                        nextAddress))
                {
                    break;
                }

                //
                // A new leader starts a new block.
                //
                if (nextAddress != leader &&
                    IsLeader(
                        leaders,
                        nextAddress))
                {
                    break;
                }

                current =
                    nextAddress;
            }

            if (!block.
                    instructionAddresses.empty())
            {
                result.blocks.push_back(
                    std::move(block));
            }
        }

        std::sort(
            result.blocks.begin(),
            result.blocks.end(),
            [](const BasicBlock& left,
               const BasicBlock& right)
            {
                return
                    left.beginAddress <
                    right.beginAddress;
            });

        //
        // =================================================
        // Phase 3:
        // Build block edges
        // =================================================
        //
        for (auto& block :
             result.blocks)
        {
            if (block.
                    instructionAddresses.empty())
            {
                block.terminal = true;
                continue;
            }

            const u16 lastAddress =
                block.
                    instructionAddresses.back();

            const auto instruction =
                disassembler.Decode(
                    memory,
                    lastAddress);

            //
            // Conditional branch.
            //
            if (IsConditionalBranch(
                    instruction.instruction))
            {
                const u16 branchTarget =
                    RelativeTarget(
                        instruction);

                AddEdge(
                    result,
                    block,
                    branchTarget,
                    BasicBlockEdgeType::
                        BranchTaken);

                u16 nextAddress = 0;

                if (NextInstructionAddress(
                        routine,
                        instruction,
                        nextAddress))
                {
                    AddEdge(
                        result,
                        block,
                        nextAddress,
                        BasicBlockEdgeType::
                            FallThrough);
                }
            }
            //
            // Direct JMP.
            //
            else if (
                instruction.instruction ==
                    cpu6502::Instruction::JMP &&
                instruction.addressMode ==
                    cpu6502::AddressMode::Absolute)
            {
                const u16 target =
                    ResolveAbsoluteTarget(
                        controlFlow,
                        AbsoluteTarget(
                            instruction));

                AddEdge(
                    result,
                    block,
                    target,
                    BasicBlockEdgeType::Jump);
            }
            //
            // End of execution path.
            //
            else if (
                instruction.instruction ==
                    cpu6502::Instruction::RTS ||
                instruction.instruction ==
                    cpu6502::Instruction::RTI ||
                instruction.instruction ==
                    cpu6502::Instruction::BRK ||
                instruction.instruction ==
                    cpu6502::Instruction::Illegal)
            {
                //
                // No successors.
                //
            }
            //
            // Normal fall-through.
            //
            else
            {
                u16 nextAddress = 0;

                if (NextInstructionAddress(
                        routine,
                        instruction,
                        nextAddress))
                {
                    AddEdge(
                        result,
                        block,
                        nextAddress,
                        BasicBlockEdgeType::
                            FallThrough);
                }
            }

            SortUniqueEdges(
                block.successors);

            block.terminal =
                block.successors.empty();
        }
    }

    static void AddEdge(
        const RoutineBasicBlocks& routineBlocks,
        BasicBlock& source,
        u16 targetAddress,
        BasicBlockEdgeType type)
    {
        //
        // Edges represent only control flow to another
        // block inside the current routine.
        //
        const auto iterator =
            std::find_if(
                routineBlocks.blocks.begin(),
                routineBlocks.blocks.end(),
                [targetAddress](
                    const BasicBlock& block)
                {
                    return
                        block.beginAddress ==
                        targetAddress;
                });

        if (iterator ==
            routineBlocks.blocks.end())
        {
            return;
        }

        BasicBlockEdge edge;

        edge.targetAddress =
            targetAddress;

        edge.type =
            type;

        source.successors.push_back(
            edge);
    }
};

} // namespace atari
