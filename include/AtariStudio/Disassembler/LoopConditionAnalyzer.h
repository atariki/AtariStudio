#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include <AtariStudio/Core/Project.h>
#include <AtariStudio/Core/Types.h>
#include <AtariStudio/Cpu6502/Instruction.h>
#include <AtariStudio/Disassembler/BranchConditionAnalyzer.h>
#include <AtariStudio/Disassembler/ControlFlowGraph.h>
#include <AtariStudio/Disassembler/Disassembler.h>
#include <AtariStudio/Disassembler/NaturalLoopAnalyzer.h>

namespace atari
{

enum class LoopConditionPosition
{
    Header,
    Latch,
    Body
};

struct LoopCondition
{
    u16 loopHeaderAddress = 0;

    u16 sourceBlockAddress = 0;

    u16 instructionAddress = 0;

    cpu6502::Instruction instruction =
        cpu6502::Instruction::Illegal;

    ProcessorFlag flag =
        ProcessorFlag::Zero;

    FlagState branchTakenState =
        FlagState::Clear;

    FlagState fallthroughState =
        FlagState::Set;

    FlagState continueState =
        FlagState::Clear;

    FlagState exitState =
        FlagState::Set;

    u16 branchTargetAddress = 0;

    u16 fallthroughTargetAddress = 0;

    u16 continueTargetAddress = 0;

    u16 exitTargetAddress = 0;

    LoopConditionPosition position =
        LoopConditionPosition::Body;

    bool branchTakenContinues = false;
};

struct RoutineLoopConditionAnalysis
{
    u16 routineEntryAddress = 0;

    std::string routineName;

    std::vector<LoopCondition> conditions;

    [[nodiscard]]
    std::size_t CountForLoop(
        u16 headerAddress) const noexcept
    {
        return static_cast<std::size_t>(
            std::count_if(
                conditions.begin(),
                conditions.end(),
                [headerAddress](
                    const LoopCondition& condition)
                {
                    return
                        condition.loopHeaderAddress ==
                        headerAddress;
                }));
    }
};

struct LoopConditionAnalysisResult
{
    std::vector<RoutineLoopConditionAnalysis>
        routines;

    [[nodiscard]]
    const RoutineLoopConditionAnalysis* FindRoutine(
        u16 entryAddress) const noexcept
    {
        const auto iterator =
            std::find_if(
                routines.begin(),
                routines.end(),
                [entryAddress](
                    const RoutineLoopConditionAnalysis& routine)
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

    [[nodiscard]]
    std::size_t ConditionCount() const noexcept
    {
        std::size_t count = 0;

        for (const auto& routine :
             routines)
        {
            count +=
                routine.conditions.size();
        }

        return count;
    }
};

class LoopConditionAnalyzer
{
public:

    [[nodiscard]]
    LoopConditionAnalysisResult Analyze(
        Project& project,
        const ControlFlowGraphAnalysisResult& graphs,
        const NaturalLoopAnalysisResult& loops) const
    {
        LoopConditionAnalysisResult result;

        Disassembler disassembler;

        for (const auto& routineLoops :
             loops.routines)
        {
            RoutineLoopConditionAnalysis
                routineResult;

            routineResult.routineEntryAddress =
                routineLoops.routineEntryAddress;

            routineResult.routineName =
                routineLoops.routineName;

            const RoutineControlFlowGraph* graph =
                graphs.FindRoutine(
                    routineLoops.routineEntryAddress);

            if (graph == nullptr)
            {
                result.routines.push_back(
                    std::move(
                        routineResult));

                continue;
            }

            for (const auto& loop :
                 routineLoops.loops)
            {
                AnalyzeLoop(
                    project,
                    disassembler,
                    *graph,
                    loop,
                    routineResult);
            }

            std::sort(
                routineResult.conditions.begin(),
                routineResult.conditions.end(),
                [](const LoopCondition& left,
                   const LoopCondition& right)
                {
                    if (left.loopHeaderAddress !=
                        right.loopHeaderAddress)
                    {
                        return
                            left.loopHeaderAddress <
                            right.loopHeaderAddress;
                    }

                    if (left.sourceBlockAddress !=
                        right.sourceBlockAddress)
                    {
                        return
                            left.sourceBlockAddress <
                            right.sourceBlockAddress;
                    }

                    return
                        left.instructionAddress <
                        right.instructionAddress;
                });

            result.routines.push_back(
                std::move(
                    routineResult));
        }

        return result;
    }

private:

    static void AnalyzeLoop(
        Project& project,
        const Disassembler& disassembler,
        const RoutineControlFlowGraph& graph,
        const NaturalLoop& loop,
        RoutineLoopConditionAnalysis& routineResult)
    {
        for (const u16 sourceAddress :
             loop.blockAddresses)
        {
            const ControlFlowGraphNode* node =
                graph.FindNode(
                    sourceAddress);

            if (node == nullptr ||
                node->instructionAddresses.empty())
            {
                continue;
            }

            const ControlFlowGraphEdge* branchEdge =
                nullptr;

            const ControlFlowGraphEdge* fallthroughEdge =
                nullptr;

            for (const auto& edge :
                 graph.edges)
            {
                if (edge.sourceAddress !=
                    sourceAddress)
                {
                    continue;
                }

                if (edge.type ==
                    ControlFlowGraphEdgeType::BranchTaken)
                {
                    branchEdge =
                        &edge;
                }
                else if (edge.type ==
                         ControlFlowGraphEdgeType::FallThrough)
                {
                    fallthroughEdge =
                        &edge;
                }
            }

            if (branchEdge == nullptr ||
                fallthroughEdge == nullptr)
            {
                continue;
            }

            const bool branchInside =
                loop.Contains(
                    branchEdge->targetAddress);

            const bool fallthroughInside =
                loop.Contains(
                    fallthroughEdge->targetAddress);

            if (branchInside ==
                fallthroughInside)
            {
                continue;
            }

            const u16 instructionAddress =
                node->instructionAddresses.back();

            const auto decoded =
                disassembler.Decode(
                    project.GetMemory(),
                    instructionAddress);

            LoopCondition condition;

            if (!DecodeCondition(
                    decoded.instruction,
                    condition))
            {
                continue;
            }

            condition.loopHeaderAddress =
                loop.headerAddress;

            condition.sourceBlockAddress =
                sourceAddress;

            condition.instructionAddress =
                instructionAddress;

            condition.instruction =
                decoded.instruction;

            condition.branchTargetAddress =
                branchEdge->targetAddress;

            condition.fallthroughTargetAddress =
                fallthroughEdge->targetAddress;

            condition.branchTakenContinues =
                branchInside;

            if (branchInside)
            {
                condition.continueState =
                    condition.branchTakenState;

                condition.exitState =
                    condition.fallthroughState;

                condition.continueTargetAddress =
                    branchEdge->targetAddress;

                condition.exitTargetAddress =
                    fallthroughEdge->targetAddress;
            }
            else
            {
                condition.continueState =
                    condition.fallthroughState;

                condition.exitState =
                    condition.branchTakenState;

                condition.continueTargetAddress =
                    fallthroughEdge->targetAddress;

                condition.exitTargetAddress =
                    branchEdge->targetAddress;
            }

            condition.position =
                DeterminePosition(
                    loop,
                    sourceAddress);

            routineResult.conditions.push_back(
                condition);
        }
    }

    [[nodiscard]]
    static LoopConditionPosition DeterminePosition(
        const NaturalLoop& loop,
        u16 sourceAddress) noexcept
    {
        //
        // IMPORTANT:
        //
        // A self-loop has:
        //
        //     header == latch
        //
        // Since its conditional branch is located at
        // the end of the block, treat it as a latch
        // condition. This lets the later structured
        // reconstruction model it as do/while.
        //
        if (std::find(
                loop.latchAddresses.begin(),
                loop.latchAddresses.end(),
                sourceAddress) !=
            loop.latchAddresses.end())
        {
            return
                LoopConditionPosition::Latch;
        }

        //
        // A non-self-loop condition at the entry block
        // behaves like a pre-test.
        //
        if (sourceAddress ==
            loop.headerAddress)
        {
            return
                LoopConditionPosition::Header;
        }

        //
        // Conditional exit somewhere inside a loop.
        //
        // This will later become a break-like construct.
        //
        return
            LoopConditionPosition::Body;
    }

    [[nodiscard]]
    static bool DecodeCondition(
        cpu6502::Instruction instruction,
        LoopCondition& condition) noexcept
    {
        using cpu6502::Instruction;

        switch (instruction)
        {
        case Instruction::BCC:

            condition.flag =
                ProcessorFlag::Carry;

            condition.branchTakenState =
                FlagState::Clear;

            condition.fallthroughState =
                FlagState::Set;

            return true;

        case Instruction::BCS:

            condition.flag =
                ProcessorFlag::Carry;

            condition.branchTakenState =
                FlagState::Set;

            condition.fallthroughState =
                FlagState::Clear;

            return true;

        case Instruction::BEQ:

            condition.flag =
                ProcessorFlag::Zero;

            condition.branchTakenState =
                FlagState::Set;

            condition.fallthroughState =
                FlagState::Clear;

            return true;

        case Instruction::BNE:

            condition.flag =
                ProcessorFlag::Zero;

            condition.branchTakenState =
                FlagState::Clear;

            condition.fallthroughState =
                FlagState::Set;

            return true;

        case Instruction::BMI:

            condition.flag =
                ProcessorFlag::Negative;

            condition.branchTakenState =
                FlagState::Set;

            condition.fallthroughState =
                FlagState::Clear;

            return true;

        case Instruction::BPL:

            condition.flag =
                ProcessorFlag::Negative;

            condition.branchTakenState =
                FlagState::Clear;

            condition.fallthroughState =
                FlagState::Set;

            return true;

        case Instruction::BVC:

            condition.flag =
                ProcessorFlag::Overflow;

            condition.branchTakenState =
                FlagState::Clear;

            condition.fallthroughState =
                FlagState::Set;

            return true;

        case Instruction::BVS:

            condition.flag =
                ProcessorFlag::Overflow;

            condition.branchTakenState =
                FlagState::Set;

            condition.fallthroughState =
                FlagState::Clear;

            return true;

        default:

            return false;
        }
    }
};

} // namespace atari