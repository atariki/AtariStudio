#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <AtariStudio/Core/Types.h>
#include <AtariStudio/Disassembler/LoopConditionAnalyzer.h>
#include <AtariStudio/Disassembler/LoopNestingAnalyzer.h>
#include <AtariStudio/Disassembler/NaturalLoopAnalyzer.h>

namespace atari
{

// ============================================================
// High-level loop type
// ============================================================

enum class StructuredLoopKind
{
    //
    // Condition is tested before the main body.
    //
    //     while (condition)
    //     {
    //         ...
    //     }
    //
    While,

    //
    // Condition is tested at a latch after the body.
    //
    //     do
    //     {
    //         ...
    //     }
    //     while (condition);
    //
    DoWhile,

    //
    // No directly identifiable loop-boundary condition.
    //
    //     for (;;)
    //     {
    //         ...
    //     }
    //
    Infinite,

    //
    // More than one loop-boundary condition exists,
    // or the loop cannot safely be represented as a
    // simple while / do-while.
    //
    Complex
};

// ============================================================
// Structured loop
// ============================================================

struct StructuredLoop
{
    //
    // Natural-loop header.
    //
    u16 headerAddress = 0;

    StructuredLoopKind kind =
        StructuredLoopKind::Complex;

    //
    // All basic blocks belonging to the natural loop.
    //
    std::vector<u16> blockAddresses;

    //
    // Back-edge source blocks.
    //
    std::vector<u16> latchAddresses;

    //
    // Unique addresses reached when execution leaves
    // the loop.
    //
    std::vector<u16> exitAddresses;

    //
    // Primary condition used to reconstruct a simple
    // while / do-while.
    //
    // Empty for Infinite or Complex loops when there
    // is no unique controlling condition.
    //
    std::optional<LoopCondition> primaryCondition;

    //
    // Conditions located at the loop header.
    //
    std::vector<LoopCondition> headerConditions;

    //
    // Conditions located at a loop latch.
    //
    std::vector<LoopCondition> latchConditions;

    //
    // Conditions located inside the loop body.
    //
    // These are typically break-like conditions.
    //
    std::vector<LoopCondition> bodyConditions;

    //
    // Loop nesting information.
    //
    std::optional<u16> parentHeaderAddress;

    std::vector<u16> childHeaders;

    std::size_t depth = 0;

    bool selfLoop = false;

    [[nodiscard]]
    bool IsWhile() const noexcept
    {
        return
            kind ==
            StructuredLoopKind::While;
    }

    [[nodiscard]]
    bool IsDoWhile() const noexcept
    {
        return
            kind ==
            StructuredLoopKind::DoWhile;
    }

    [[nodiscard]]
    bool IsInfinite() const noexcept
    {
        return
            kind ==
            StructuredLoopKind::Infinite;
    }

    [[nodiscard]]
    bool IsComplex() const noexcept
    {
        return
            kind ==
            StructuredLoopKind::Complex;
    }

    [[nodiscard]]
    bool HasPrimaryCondition() const noexcept
    {
        return
            primaryCondition.has_value();
    }

    [[nodiscard]]
    bool HasBreakConditions() const noexcept
    {
        return
            !bodyConditions.empty();
    }

    [[nodiscard]]
    std::size_t ConditionCount() const noexcept
    {
        return
            headerConditions.size() +
            latchConditions.size() +
            bodyConditions.size();
    }

    [[nodiscard]]
    std::size_t BlockCount() const noexcept
    {
        return
            blockAddresses.size();
    }

    [[nodiscard]]
    std::size_t ExitCount() const noexcept
    {
        return
            exitAddresses.size();
    }
};

// ============================================================
// Per-routine result
// ============================================================

struct RoutineLoopStructureAnalysis
{
    u16 routineEntryAddress = 0;

    std::string routineName;

    std::vector<StructuredLoop> loops;

    [[nodiscard]]
    const StructuredLoop* FindLoop(
        u16 headerAddress) const noexcept
    {
        const auto iterator =
            std::find_if(
                loops.begin(),
                loops.end(),
                [headerAddress](
                    const StructuredLoop& loop)
                {
                    return
                        loop.headerAddress ==
                        headerAddress;
                });

        if (iterator == loops.end())
        {
            return nullptr;
        }

        return &*iterator;
    }

    [[nodiscard]]
    StructuredLoop* FindLoop(
        u16 headerAddress) noexcept
    {
        const auto iterator =
            std::find_if(
                loops.begin(),
                loops.end(),
                [headerAddress](
                    const StructuredLoop& loop)
                {
                    return
                        loop.headerAddress ==
                        headerAddress;
                });

        if (iterator == loops.end())
        {
            return nullptr;
        }

        return &*iterator;
    }

    [[nodiscard]]
    std::size_t WhileCount() const noexcept
    {
        return CountKind(
            StructuredLoopKind::While);
    }

    [[nodiscard]]
    std::size_t DoWhileCount() const noexcept
    {
        return CountKind(
            StructuredLoopKind::DoWhile);
    }

    [[nodiscard]]
    std::size_t InfiniteCount() const noexcept
    {
        return CountKind(
            StructuredLoopKind::Infinite);
    }

    [[nodiscard]]
    std::size_t ComplexCount() const noexcept
    {
        return CountKind(
            StructuredLoopKind::Complex);
    }

    [[nodiscard]]
    std::size_t BreakConditionCount() const noexcept
    {
        std::size_t count = 0;

        for (const auto& loop :
             loops)
        {
            count +=
                loop.bodyConditions.size();
        }

        return count;
    }

private:

    [[nodiscard]]
    std::size_t CountKind(
        StructuredLoopKind kind) const noexcept
    {
        return static_cast<std::size_t>(
            std::count_if(
                loops.begin(),
                loops.end(),
                [kind](
                    const StructuredLoop& loop)
                {
                    return
                        loop.kind ==
                        kind;
                }));
    }
};

// ============================================================
// Global result
// ============================================================

struct LoopStructureAnalysisResult
{
    std::vector<RoutineLoopStructureAnalysis>
        routines;

    [[nodiscard]]
    const RoutineLoopStructureAnalysis* FindRoutine(
        u16 entryAddress) const noexcept
    {
        const auto iterator =
            std::find_if(
                routines.begin(),
                routines.end(),
                [entryAddress](
                    const RoutineLoopStructureAnalysis& routine)
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
    std::size_t LoopCount() const noexcept
    {
        std::size_t count = 0;

        for (const auto& routine :
             routines)
        {
            count +=
                routine.loops.size();
        }

        return count;
    }

    [[nodiscard]]
    std::size_t WhileCount() const noexcept
    {
        std::size_t count = 0;

        for (const auto& routine :
             routines)
        {
            count +=
                routine.WhileCount();
        }

        return count;
    }

    [[nodiscard]]
    std::size_t DoWhileCount() const noexcept
    {
        std::size_t count = 0;

        for (const auto& routine :
             routines)
        {
            count +=
                routine.DoWhileCount();
        }

        return count;
    }

    [[nodiscard]]
    std::size_t InfiniteCount() const noexcept
    {
        std::size_t count = 0;

        for (const auto& routine :
             routines)
        {
            count +=
                routine.InfiniteCount();
        }

        return count;
    }

    [[nodiscard]]
    std::size_t ComplexCount() const noexcept
    {
        std::size_t count = 0;

        for (const auto& routine :
             routines)
        {
            count +=
                routine.ComplexCount();
        }

        return count;
    }

    [[nodiscard]]
    std::size_t BreakConditionCount() const noexcept
    {
        std::size_t count = 0;

        for (const auto& routine :
             routines)
        {
            count +=
                routine.BreakConditionCount();
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
            for (const auto& loop :
                 routine.loops)
            {
                maximum =
                    std::max(
                        maximum,
                        loop.depth);
            }
        }

        return maximum;
    }
};

// ============================================================
// Analyzer
// ============================================================

class LoopStructureAnalyzer
{
public:

    [[nodiscard]]
    LoopStructureAnalysisResult Analyze(
        const NaturalLoopAnalysisResult& naturalLoops,
        const LoopConditionAnalysisResult& loopConditions,
        const LoopNestingAnalysisResult& loopNesting) const
    {
        LoopStructureAnalysisResult result;

        for (const auto& routineLoops :
             naturalLoops.routines)
        {
            RoutineLoopStructureAnalysis
                routineResult;

            routineResult.routineEntryAddress =
                routineLoops.routineEntryAddress;

            routineResult.routineName =
                routineLoops.routineName;

            const RoutineLoopConditionAnalysis*
                routineConditions =
                    FindConditionRoutine(
                        loopConditions,
                        routineLoops.routineEntryAddress);

            for (const auto& naturalLoop :
                 routineLoops.loops)
            {
                StructuredLoop
                    structuredLoop;

                BuildBaseLoop(
                    naturalLoop,
                    structuredLoop);

                if (routineConditions != nullptr)
                {
                    CollectConditions(
                        *routineConditions,
                        structuredLoop);
                }

                ClassifyLoop(
                    structuredLoop);

                ApplyNesting(
                    loopNesting,
                    routineLoops.routineEntryAddress,
                    structuredLoop);

                routineResult.loops.push_back(
                    std::move(
                        structuredLoop));
            }

            std::sort(
                routineResult.loops.begin(),
                routineResult.loops.end(),
                [](const StructuredLoop& left,
                   const StructuredLoop& right)
                {
                    return
                        left.headerAddress <
                        right.headerAddress;
                });

            result.routines.push_back(
                std::move(
                    routineResult));
        }

        return result;
    }

private:

    // ========================================================
    // Base information
    // ========================================================

    static void BuildBaseLoop(
        const NaturalLoop& naturalLoop,
        StructuredLoop& result)
    {
        result.headerAddress =
            naturalLoop.headerAddress;

        result.blockAddresses =
            naturalLoop.blockAddresses;

        result.latchAddresses =
            naturalLoop.latchAddresses;

        result.selfLoop =
            naturalLoop.IsSelfLoop();

        for (const auto& exit :
             naturalLoop.exits)
        {
            result.exitAddresses.push_back(
                exit.targetAddress);
        }

        SortUnique(
            result.blockAddresses);

        SortUnique(
            result.latchAddresses);

        SortUnique(
            result.exitAddresses);
    }

    // ========================================================
    // Conditions
    // ========================================================

    static void CollectConditions(
        const RoutineLoopConditionAnalysis& routineConditions,
        StructuredLoop& loop)
    {
        for (const auto& condition :
             routineConditions.conditions)
        {
            if (condition.loopHeaderAddress !=
                loop.headerAddress)
            {
                continue;
            }

            switch (condition.position)
            {
            case LoopConditionPosition::Header:

                loop.headerConditions.push_back(
                    condition);

                break;

            case LoopConditionPosition::Latch:

                loop.latchConditions.push_back(
                    condition);

                break;

            case LoopConditionPosition::Body:

                loop.bodyConditions.push_back(
                    condition);

                break;
            }
        }

        SortConditions(
            loop.headerConditions);

        SortConditions(
            loop.latchConditions);

        SortConditions(
            loop.bodyConditions);
    }

    // ========================================================
    // Classification
    // ========================================================

    static void ClassifyLoop(
        StructuredLoop& loop)
    {
        const std::size_t headerCount =
            loop.headerConditions.size();

        const std::size_t latchCount =
            loop.latchConditions.size();

        //
        // No explicit boundary condition.
        //
        // This is the safest representation:
        //
        //     for (;;)
        //
        if (headerCount == 0 &&
            latchCount == 0)
        {
            loop.kind =
                StructuredLoopKind::Infinite;

            loop.primaryCondition.reset();

            return;
        }

        //
        // Exactly one pre-test condition and no
        // post-test condition:
        //
        //     while (...)
        //
        if (headerCount == 1 &&
            latchCount == 0)
        {
            loop.kind =
                StructuredLoopKind::While;

            loop.primaryCondition =
                loop.headerConditions.front();

            return;
        }

        //
        // Exactly one post-test condition and no
        // pre-test condition:
        //
        //     do
        //     {
        //         ...
        //     }
        //     while (...);
        //
        if (headerCount == 0 &&
            latchCount == 1)
        {
            loop.kind =
                StructuredLoopKind::DoWhile;

            loop.primaryCondition =
                loop.latchConditions.front();

            return;
        }

        //
        // Examples:
        //
        //     header condition + latch condition
        //
        // or:
        //
        //     several latches with different conditions
        //
        // must not be simplified prematurely.
        //
        loop.kind =
            StructuredLoopKind::Complex;

        loop.primaryCondition.reset();
    }

    // ========================================================
    // Nesting
    // ========================================================

    static void ApplyNesting(
        const LoopNestingAnalysisResult& loopNesting,
        u16 routineEntryAddress,
        StructuredLoop& loop)
    {
        for (const auto& routine :
             loopNesting.routines)
        {
            if (routine.routineEntryAddress !=
                routineEntryAddress)
            {
                continue;
            }

            for (const auto& node :
                 routine.nodes)
            {
                if (node.headerAddress !=
                    loop.headerAddress)
                {
                    continue;
                }

                loop.parentHeaderAddress =
                    node.parentHeader;

                loop.childHeaders =
                    node.childHeaders;

                loop.depth =
                    node.depth;

                SortUnique(
                    loop.childHeaders);

                return;
            }

            return;
        }
    }

    // ========================================================
    // Helpers
    // ========================================================

    [[nodiscard]]
    static const RoutineLoopConditionAnalysis*
    FindConditionRoutine(
        const LoopConditionAnalysisResult& conditions,
        u16 routineEntryAddress) noexcept
    {
        for (const auto& routine :
             conditions.routines)
        {
            if (routine.routineEntryAddress ==
                routineEntryAddress)
            {
                return
                    &routine;
            }
        }

        return nullptr;
    }

    static void SortConditions(
        std::vector<LoopCondition>& conditions)
    {
        std::sort(
            conditions.begin(),
            conditions.end(),
            [](const LoopCondition& left,
               const LoopCondition& right)
            {
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