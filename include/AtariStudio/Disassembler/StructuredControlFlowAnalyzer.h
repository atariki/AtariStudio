#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <AtariStudio/Core/Types.h>
#include <AtariStudio/Cpu6502/Instruction.h>
#include <AtariStudio/Disassembler/BranchConditionAnalyzer.h>
#include <AtariStudio/Disassembler/ConditionalRegionAnalyzer.h>

namespace atari
{

enum class StructuredArm
{
    None,
    Then,
    Else
};

struct StructuredIf
{
    u16 headerAddress = 0;
    u16 instructionAddress = 0;
    u16 joinAddress = 0;

    cpu6502::Instruction sourceInstruction =
        cpu6502::Instruction::Illegal;

    ProcessorFlag flag =
        ProcessorFlag::Zero;

    // High-level condition controlling entry into THEN.
    FlagState thenState =
        FlagState::Set;

    // True when the high-level IF condition is the opposite
    // of the original branch-taken condition.
    bool branchConditionInverted = false;

    u16 thenEntryAddress = 0;

    std::optional<u16> elseEntryAddress;

    std::vector<u16> thenBlocks;
    std::vector<u16> elseBlocks;

    std::optional<u16> parentHeaderAddress;

    StructuredArm parentArm =
        StructuredArm::None;

    std::vector<u16> childHeaders;

    std::size_t depth = 0;

    [[nodiscard]]
    bool HasElse() const noexcept
    {
        return
            elseEntryAddress.has_value() ||
            !elseBlocks.empty();
    }
};

struct RoutineStructuredControlFlowAnalysis
{
    u16 routineEntryAddress = 0;

    std::string routineName;

    std::vector<StructuredIf> ifStatements;

    [[nodiscard]]
    const StructuredIf* FindIf(
        u16 headerAddress) const noexcept
    {
        const auto iterator =
            std::find_if(
                ifStatements.begin(),
                ifStatements.end(),
                [headerAddress](
                    const StructuredIf& statement)
                {
                    return
                        statement.headerAddress ==
                        headerAddress;
                });

        if (iterator == ifStatements.end())
        {
            return nullptr;
        }

        return &*iterator;
    }

    [[nodiscard]]
    StructuredIf* FindIf(
        u16 headerAddress) noexcept
    {
        const auto iterator =
            std::find_if(
                ifStatements.begin(),
                ifStatements.end(),
                [headerAddress](
                    const StructuredIf& statement)
                {
                    return
                        statement.headerAddress ==
                        headerAddress;
                });

        if (iterator == ifStatements.end())
        {
            return nullptr;
        }

        return &*iterator;
    }

    [[nodiscard]]
    std::size_t RootCount() const noexcept
    {
        return static_cast<std::size_t>(
            std::count_if(
                ifStatements.begin(),
                ifStatements.end(),
                [](const StructuredIf& statement)
                {
                    return
                        !statement.parentHeaderAddress.has_value();
                }));
    }

    [[nodiscard]]
    std::size_t IfElseCount() const noexcept
    {
        return static_cast<std::size_t>(
            std::count_if(
                ifStatements.begin(),
                ifStatements.end(),
                [](const StructuredIf& statement)
                {
                    return
                        statement.HasElse();
                }));
    }

    [[nodiscard]]
    std::size_t MaximumDepth() const noexcept
    {
        std::size_t maximum = 0;

        for (const auto& statement :
             ifStatements)
        {
            maximum =
                std::max(
                    maximum,
                    statement.depth);
        }

        return maximum;
    }
};

struct StructuredControlFlowAnalysisResult
{
    std::vector<RoutineStructuredControlFlowAnalysis>
        routines;

    [[nodiscard]]
    const RoutineStructuredControlFlowAnalysis* FindRoutine(
        u16 entryAddress) const noexcept
    {
        const auto iterator =
            std::find_if(
                routines.begin(),
                routines.end(),
                [entryAddress](
                    const RoutineStructuredControlFlowAnalysis& routine)
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
    std::size_t IfCount() const noexcept
    {
        std::size_t count = 0;

        for (const auto& routine :
             routines)
        {
            count +=
                routine.ifStatements.size();
        }

        return count;
    }

    [[nodiscard]]
    std::size_t IfElseCount() const noexcept
    {
        std::size_t count = 0;

        for (const auto& routine :
             routines)
        {
            count +=
                routine.IfElseCount();
        }

        return count;
    }

    [[nodiscard]]
    std::size_t RootCount() const noexcept
    {
        std::size_t count = 0;

        for (const auto& routine :
             routines)
        {
            count +=
                routine.RootCount();
        }

        return count;
    }

    [[nodiscard]]
    std::size_t MaximumDepth() const noexcept
    {
        std::size_t maximum = 0;

        for (const auto& routine :
             routines)
        {
            maximum =
                std::max(
                    maximum,
                    routine.MaximumDepth());
        }

        return maximum;
    }
};

class StructuredControlFlowAnalyzer
{
public:

    [[nodiscard]]
    StructuredControlFlowAnalysisResult Analyze(
        const ConditionalRegionAnalysisResult& conditionals,
        const BranchConditionAnalysisResult& branchConditions) const
    {
        StructuredControlFlowAnalysisResult result;

        for (const auto& routineConditions :
             conditionals.routines)
        {
            RoutineStructuredControlFlowAnalysis
                routineResult;

            routineResult.routineEntryAddress =
                routineConditions.routineEntryAddress;

            routineResult.routineName =
                routineConditions.routineName;

            const auto* routineBranches =
                branchConditions.FindRoutine(
                    routineConditions.routineEntryAddress);

            if (routineBranches == nullptr)
            {
                result.routines.push_back(
                    std::move(
                        routineResult));

                continue;
            }

            for (const auto& region :
                 routineConditions.regions)
            {
                const BranchCondition* branch =
                    routineBranches->FindCondition(
                        region.headerAddress);

                if (branch == nullptr)
                {
                    continue;
                }

                StructuredIf statement;

                if (!BuildStructuredIf(
                        region,
                        *branch,
                        statement))
                {
                    continue;
                }

                routineResult.ifStatements.push_back(
                    std::move(
                        statement));
            }

            std::sort(
                routineResult.ifStatements.begin(),
                routineResult.ifStatements.end(),
                [](const StructuredIf& left,
                   const StructuredIf& right)
                {
                    return
                        left.headerAddress <
                        right.headerAddress;
                });

            BuildNesting(
                routineResult);

            result.routines.push_back(
                std::move(
                    routineResult));
        }

        return result;
    }

private:

    [[nodiscard]]
    static bool BuildStructuredIf(
        const ConditionalRegion& region,
        const BranchCondition& branch,
        StructuredIf& result)
    {
        result.headerAddress =
            region.headerAddress;

        result.instructionAddress =
            branch.instructionAddress;

        result.joinAddress =
            region.joinAddress;

        result.sourceInstruction =
            branch.instruction;

        result.flag =
            branch.flag;

        //
        // Proper IF / ELSE:
        //
        // both arms contain real blocks.
        //
        if (region.kind ==
            ConditionalRegionKind::IfElse)
        {
            if (region.branchBlocks.empty() ||
                region.fallthroughBlocks.empty())
            {
                return false;
            }

            result.thenState =
                branch.branchTakenState;

            result.branchConditionInverted =
                false;

            result.thenEntryAddress =
                region.branchTargetAddress;

            result.elseEntryAddress =
                region.fallthroughTargetAddress;

            result.thenBlocks =
                region.branchBlocks;

            result.elseBlocks =
                region.fallthroughBlocks;

            return true;
        }

        //
        // Simple IF:
        //
        // one CFG arm goes directly to JOIN,
        // while the other arm contains the body.
        //
        const bool branchArmEmpty =
            region.branchTargetAddress ==
                region.joinAddress ||
            region.branchBlocks.empty();

        const bool fallthroughArmEmpty =
            region.fallthroughTargetAddress ==
                region.joinAddress ||
            region.fallthroughBlocks.empty();

        //
        // For a valid simple IF exactly one arm
        // must represent an empty body.
        //
        if (branchArmEmpty ==
            fallthroughArmEmpty)
        {
            return false;
        }

        if (branchArmEmpty)
        {
            //
            // Example:
            //
            //     BNE JOIN
            //     BODY
            // JOIN:
            //
            // BNE executes when Z == 0 and skips BODY.
            //
            // Therefore BODY executes when:
            //
            //     Z == 1
            //
            // Thus the high-level IF condition is the
            // fall-through condition.
            //
            result.thenState =
                branch.fallthroughState;

            result.branchConditionInverted =
                true;

            result.thenEntryAddress =
                region.fallthroughTargetAddress;

            result.thenBlocks =
                region.fallthroughBlocks;

            result.elseEntryAddress.reset();
            result.elseBlocks.clear();
        }
        else
        {
            //
            // Conditional branch enters the IF body.
            //
            result.thenState =
                branch.branchTakenState;

            result.branchConditionInverted =
                false;

            result.thenEntryAddress =
                region.branchTargetAddress;

            result.thenBlocks =
                region.branchBlocks;

            result.elseEntryAddress.reset();
            result.elseBlocks.clear();
        }

        return
            !result.thenBlocks.empty();
    }

    static void BuildNesting(
        RoutineStructuredControlFlowAnalysis& routine)
    {
        //
        // Reset nesting information first.
        //
        for (auto& statement :
             routine.ifStatements)
        {
            statement.parentHeaderAddress.reset();

            statement.parentArm =
                StructuredArm::None;

            statement.childHeaders.clear();

            statement.depth = 0;
        }

        //
        // For each IF find the smallest other IF arm
        // which contains its header.
        //
        // That enclosing IF becomes the direct parent.
        //
        for (auto& child :
             routine.ifStatements)
        {
            const StructuredIf* bestParent =
                nullptr;

            StructuredArm bestArm =
                StructuredArm::None;

            std::size_t bestSize = 0;

            for (const auto& candidate :
                 routine.ifStatements)
            {
                //
                // An IF cannot contain itself.
                //
                if (candidate.headerAddress ==
                    child.headerAddress)
                {
                    continue;
                }

                StructuredArm containingArm =
                    StructuredArm::None;

                std::size_t containingSize = 0;

                if (ContainsAddress(
                        candidate.thenBlocks,
                        child.headerAddress))
                {
                    containingArm =
                        StructuredArm::Then;

                    containingSize =
                        candidate.thenBlocks.size();
                }
                else if (ContainsAddress(
                             candidate.elseBlocks,
                             child.headerAddress))
                {
                    containingArm =
                        StructuredArm::Else;

                    containingSize =
                        candidate.elseBlocks.size();
                }

                if (containingArm ==
                    StructuredArm::None)
                {
                    continue;
                }

                //
                // Choose the smallest enclosing arm.
                //
                if (bestParent == nullptr ||
                    containingSize < bestSize ||
                    (containingSize == bestSize &&
                     candidate.headerAddress <
                         bestParent->headerAddress))
                {
                    bestParent =
                        &candidate;

                    bestArm =
                        containingArm;

                    bestSize =
                        containingSize;
                }
            }

            if (bestParent != nullptr)
            {
                child.parentHeaderAddress =
                    bestParent->headerAddress;

                child.parentArm =
                    bestArm;
            }
        }

        //
        // Build reverse parent -> children lists.
        //
        for (const auto& child :
             routine.ifStatements)
        {
            if (!child.parentHeaderAddress.has_value())
            {
                continue;
            }

            StructuredIf* parent =
                routine.FindIf(
                    child.parentHeaderAddress.value());

            if (parent == nullptr)
            {
                continue;
            }

            parent->childHeaders.push_back(
                child.headerAddress);
        }

        //
        // Keep children deterministic.
        //
        for (auto& statement :
             routine.ifStatements)
        {
            SortUnique(
                statement.childHeaders);
        }

        //
        // Calculate nesting depth.
        //
        for (auto& statement :
             routine.ifStatements)
        {
            statement.depth =
                CalculateDepth(
                    routine,
                    statement);
        }
    }

    [[nodiscard]]
    static std::size_t CalculateDepth(
        const RoutineStructuredControlFlowAnalysis& routine,
        const StructuredIf& statement)
    {
        std::size_t depth = 0;

        std::optional<u16> parent =
            statement.parentHeaderAddress;

        //
        // Guard protects us from malformed parent cycles.
        //
        std::size_t guard = 0;

        while (parent.has_value() &&
               guard < routine.ifStatements.size())
        {
            ++depth;
            ++guard;

            const StructuredIf* parentStatement =
                routine.FindIf(
                    parent.value());

            if (parentStatement == nullptr)
            {
                break;
            }

            parent =
                parentStatement->parentHeaderAddress;
        }

        return depth;
    }

    [[nodiscard]]
    static bool ContainsAddress(
        const std::vector<u16>& addresses,
        u16 address)
    {
        return
            std::find(
                addresses.begin(),
                addresses.end(),
                address) !=
            addresses.end();
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
};

} // namespace atari
