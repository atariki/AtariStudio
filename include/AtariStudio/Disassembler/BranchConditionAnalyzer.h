#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include <AtariStudio/Core/Project.h>
#include <AtariStudio/Core/Types.h>
#include <AtariStudio/Cpu6502/Instruction.h>
#include <AtariStudio/Disassembler/ConditionalRegionAnalyzer.h>
#include <AtariStudio/Disassembler/ControlFlowGraph.h>
#include <AtariStudio/Disassembler/Disassembler.h>

namespace atari
{

enum class ProcessorFlag
{
    Carry,
    Zero,
    Negative,
    Overflow
};

enum class FlagState
{
    Clear,
    Set
};

struct BranchCondition
{
    //
    // Basic-block address containing the conditional branch.
    //
    u16 headerAddress = 0;

    //
    // Address of the actual 6502 branch instruction.
    //
    u16 instructionAddress = 0;

    cpu6502::Instruction instruction =
        cpu6502::Instruction::Illegal;

    ProcessorFlag flag =
        ProcessorFlag::Zero;

    //
    // Condition under which the branch edge is taken.
    //
    FlagState branchTakenState =
        FlagState::Clear;

    //
    // Opposite condition used by the fall-through edge.
    //
    FlagState fallthroughState =
        FlagState::Set;

    u16 branchTargetAddress = 0;

    u16 fallthroughTargetAddress = 0;

    u16 joinAddress = 0;

    ConditionalRegionKind regionKind =
        ConditionalRegionKind::IfThen;

    [[nodiscard]]
    bool IsBranchTakenWhenSet() const noexcept
    {
        return
            branchTakenState ==
            FlagState::Set;
    }
};

struct RoutineBranchConditionAnalysis
{
    u16 routineEntryAddress = 0;

    std::string routineName;

    std::vector<BranchCondition> conditions;

    [[nodiscard]]
    const BranchCondition* FindCondition(
        u16 headerAddress) const noexcept
    {
        const auto iterator =
            std::find_if(
                conditions.begin(),
                conditions.end(),
                [headerAddress](
                    const BranchCondition& condition)
                {
                    return
                        condition.headerAddress ==
                        headerAddress;
                });

        if (iterator == conditions.end())
        {
            return nullptr;
        }

        return &*iterator;
    }
};

struct BranchConditionAnalysisResult
{
    std::vector<RoutineBranchConditionAnalysis> routines;

    [[nodiscard]]
    const RoutineBranchConditionAnalysis* FindRoutine(
        u16 entryAddress) const noexcept
    {
        const auto iterator =
            std::find_if(
                routines.begin(),
                routines.end(),
                [entryAddress](
                    const RoutineBranchConditionAnalysis& routine)
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

class BranchConditionAnalyzer
{
public:

    [[nodiscard]]
    BranchConditionAnalysisResult Analyze(
        Project& project,
        const ControlFlowGraphAnalysisResult& graphs,
        const ConditionalRegionAnalysisResult& conditionals) const
    {
        BranchConditionAnalysisResult result;

        Disassembler disassembler;

        for (const auto& routineConditions :
             conditionals.routines)
        {
            RoutineBranchConditionAnalysis
                routineResult;

            routineResult.routineEntryAddress =
                routineConditions.routineEntryAddress;

            routineResult.routineName =
                routineConditions.routineName;

            const RoutineControlFlowGraph* graph =
                graphs.FindRoutine(
                    routineConditions.routineEntryAddress);

            if (graph == nullptr)
            {
                result.routines.push_back(
                    std::move(
                        routineResult));

                continue;
            }

            for (const auto& region :
                 routineConditions.regions)
            {
                const ControlFlowGraphNode* node =
                    graph->FindNode(
                        region.headerAddress);

                if (node == nullptr ||
                    node->instructionAddresses.empty())
                {
                    continue;
                }

                //
                // A conditional branch terminates its
                // basic block, therefore the final
                // instruction is the branch instruction.
                //
                const u16 instructionAddress =
                    node->instructionAddresses.back();

                const auto decoded =
                    disassembler.Decode(
                        project.GetMemory(),
                        instructionAddress);

                BranchCondition condition;

                if (!DecodeCondition(
                        decoded.instruction,
                        condition))
                {
                    continue;
                }

                condition.headerAddress =
                    region.headerAddress;

                condition.instructionAddress =
                    instructionAddress;

                condition.instruction =
                    decoded.instruction;

                condition.branchTargetAddress =
                    region.branchTargetAddress;

                condition.fallthroughTargetAddress =
                    region.fallthroughTargetAddress;

                condition.joinAddress =
                    region.joinAddress;

                condition.regionKind =
                    region.kind;

                routineResult.conditions.push_back(
                    condition);
            }

            std::sort(
                routineResult.conditions.begin(),
                routineResult.conditions.end(),
                [](const BranchCondition& left,
                   const BranchCondition& right)
                {
                    return
                        left.headerAddress <
                        right.headerAddress;
                });

            routineResult.conditions.erase(
                std::unique(
                    routineResult.conditions.begin(),
                    routineResult.conditions.end(),
                    [](const BranchCondition& left,
                       const BranchCondition& right)
                    {
                        return
                            left.headerAddress ==
                                right.headerAddress;
                    }),
                routineResult.conditions.end());

            result.routines.push_back(
                std::move(
                    routineResult));
        }

        return result;
    }

private:

    [[nodiscard]]
    static bool DecodeCondition(
        cpu6502::Instruction instruction,
        BranchCondition& result)
    {
        switch (instruction)
        {
        case cpu6502::Instruction::BCC:

            result.flag =
                ProcessorFlag::Carry;

            result.branchTakenState =
                FlagState::Clear;

            result.fallthroughState =
                FlagState::Set;

            return true;

        case cpu6502::Instruction::BCS:

            result.flag =
                ProcessorFlag::Carry;

            result.branchTakenState =
                FlagState::Set;

            result.fallthroughState =
                FlagState::Clear;

            return true;

        case cpu6502::Instruction::BEQ:

            result.flag =
                ProcessorFlag::Zero;

            result.branchTakenState =
                FlagState::Set;

            result.fallthroughState =
                FlagState::Clear;

            return true;

        case cpu6502::Instruction::BNE:

            result.flag =
                ProcessorFlag::Zero;

            result.branchTakenState =
                FlagState::Clear;

            result.fallthroughState =
                FlagState::Set;

            return true;

        case cpu6502::Instruction::BMI:

            result.flag =
                ProcessorFlag::Negative;

            result.branchTakenState =
                FlagState::Set;

            result.fallthroughState =
                FlagState::Clear;

            return true;

        case cpu6502::Instruction::BPL:

            result.flag =
                ProcessorFlag::Negative;

            result.branchTakenState =
                FlagState::Clear;

            result.fallthroughState =
                FlagState::Set;

            return true;

        case cpu6502::Instruction::BVC:

            result.flag =
                ProcessorFlag::Overflow;

            result.branchTakenState =
                FlagState::Clear;

            result.fallthroughState =
                FlagState::Set;

            return true;

        case cpu6502::Instruction::BVS:

            result.flag =
                ProcessorFlag::Overflow;

            result.branchTakenState =
                FlagState::Set;

            result.fallthroughState =
                FlagState::Clear;

            return true;

        default:

            return false;
        }
    }
};

} // namespace atari