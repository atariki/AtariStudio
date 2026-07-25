#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <AtariStudio/Core/Types.h>
#include <AtariStudio/Disassembler/NaturalLoopAnalyzer.h>

namespace atari
{

struct LoopNestingNode
{
    u16 headerAddress = 0;

    //
    // Parent natural loop.
    //
    // Root loops have no parent.
    //
    std::optional<u16> parentHeader;

    //
    // Direct children only.
    //
    std::vector<u16> childHeaders;

    //
    // Root loop = depth 0.
    //
    std::size_t depth = 0;

    std::size_t blockCount = 0;
    std::size_t latchCount = 0;
    std::size_t exitCount = 0;

    bool selfLoop = false;

    [[nodiscard]]
    bool IsRoot() const noexcept
    {
        return
            !parentHeader.has_value();
    }
};

struct RoutineLoopNestingAnalysis
{
    u16 routineEntryAddress = 0;

    std::string routineName;

    std::vector<LoopNestingNode> nodes;

    [[nodiscard]]
    const LoopNestingNode* FindNode(
        u16 headerAddress) const noexcept
    {
        const auto iterator =
            std::find_if(
                nodes.begin(),
                nodes.end(),
                [headerAddress](
                    const LoopNestingNode& node)
                {
                    return
                        node.headerAddress ==
                        headerAddress;
                });

        if (iterator == nodes.end())
        {
            return nullptr;
        }

        return &*iterator;
    }

    [[nodiscard]]
    LoopNestingNode* FindNode(
        u16 headerAddress) noexcept
    {
        const auto iterator =
            std::find_if(
                nodes.begin(),
                nodes.end(),
                [headerAddress](
                    const LoopNestingNode& node)
                {
                    return
                        node.headerAddress ==
                        headerAddress;
                });

        if (iterator == nodes.end())
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
                nodes.begin(),
                nodes.end(),
                [](const LoopNestingNode& node)
                {
                    return node.IsRoot();
                }));
    }

    [[nodiscard]]
    std::size_t MaximumDepth() const noexcept
    {
        std::size_t maximum = 0;

        for (const auto& node :
             nodes)
        {
            maximum =
                std::max(
                    maximum,
                    node.depth);
        }

        return maximum;
    }
};

struct LoopNestingAnalysisResult
{
    std::vector<RoutineLoopNestingAnalysis> routines;

    [[nodiscard]]
    const RoutineLoopNestingAnalysis* FindRoutine(
        u16 entryAddress) const noexcept
    {
        const auto iterator =
            std::find_if(
                routines.begin(),
                routines.end(),
                [entryAddress](
                    const RoutineLoopNestingAnalysis& routine)
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
    std::size_t NodeCount() const noexcept
    {
        std::size_t count = 0;

        for (const auto& routine :
             routines)
        {
            count +=
                routine.nodes.size();
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

class LoopNestingAnalyzer
{
public:

    [[nodiscard]]
    LoopNestingAnalysisResult Analyze(
        const NaturalLoopAnalysisResult& loops) const
    {
        LoopNestingAnalysisResult result;

        for (const auto& routineLoops :
             loops.routines)
        {
            RoutineLoopNestingAnalysis
                routineResult;

            routineResult.routineEntryAddress =
                routineLoops.routineEntryAddress;

            routineResult.routineName =
                routineLoops.routineName;

            //
            // =================================================
            // Phase 1:
            // Create one tree node for every natural loop.
            // =================================================
            //
            for (const auto& loop :
                 routineLoops.loops)
            {
                LoopNestingNode node;

                node.headerAddress =
                    loop.headerAddress;

                node.blockCount =
                    loop.BlockCount();

                node.latchCount =
                    loop.LatchCount();

                node.exitCount =
                    loop.ExitCount();

                node.selfLoop =
                    loop.IsSelfLoop();

                routineResult.nodes.push_back(
                    std::move(node));
            }

            //
            // =================================================
            // Phase 2:
            // Find the nearest containing loop.
            //
            // Parent = smallest strict superset.
            // =================================================
            //
            for (const auto& childLoop :
                 routineLoops.loops)
            {
                const NaturalLoop* bestParent =
                    nullptr;

                for (const auto& candidateParent :
                     routineLoops.loops)
                {
                    if (candidateParent.headerAddress ==
                        childLoop.headerAddress)
                    {
                        continue;
                    }

                    //
                    // Parent must be a strict superset.
                    //
                    if (candidateParent.BlockCount() <=
                        childLoop.BlockCount())
                    {
                        continue;
                    }

                    if (!ContainsLoop(
                            candidateParent,
                            childLoop))
                    {
                        continue;
                    }

                    //
                    // Choose the smallest containing loop.
                    //
                    if (bestParent == nullptr ||
                        candidateParent.BlockCount() <
                            bestParent->BlockCount() ||
                        (candidateParent.BlockCount() ==
                             bestParent->BlockCount() &&
                         candidateParent.headerAddress <
                             bestParent->headerAddress))
                    {
                        bestParent =
                            &candidateParent;
                    }
                }

                if (bestParent == nullptr)
                {
                    continue;
                }

                LoopNestingNode* childNode =
                    routineResult.FindNode(
                        childLoop.headerAddress);

                if (childNode == nullptr)
                {
                    continue;
                }

                childNode->parentHeader =
                    bestParent->headerAddress;
            }

            //
            // =================================================
            // Phase 3:
            // Build child lists.
            // =================================================
            //
            for (const auto& node :
                 routineResult.nodes)
            {
                if (!node.parentHeader.has_value())
                {
                    continue;
                }

                LoopNestingNode* parent =
                    routineResult.FindNode(
                        node.parentHeader.value());

                if (parent == nullptr)
                {
                    continue;
                }

                parent->childHeaders.push_back(
                    node.headerAddress);
            }

            for (auto& node :
                 routineResult.nodes)
            {
                SortUnique(
                    node.childHeaders);
            }

            //
            // =================================================
            // Phase 4:
            // Compute nesting depths.
            // =================================================
            //
            for (auto& node :
                 routineResult.nodes)
            {
                node.depth =
                    CalculateDepth(
                        routineResult,
                        node);
            }

            std::sort(
                routineResult.nodes.begin(),
                routineResult.nodes.end(),
                [](const LoopNestingNode& left,
                   const LoopNestingNode& right)
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

    [[nodiscard]]
    static bool ContainsLoop(
        const NaturalLoop& parent,
        const NaturalLoop& child)
    {
        for (const u16 childBlock :
             child.blockAddresses)
        {
            if (!parent.Contains(
                    childBlock))
            {
                return false;
            }
        }

        return true;
    }

    [[nodiscard]]
    static std::size_t CalculateDepth(
        const RoutineLoopNestingAnalysis& routine,
        const LoopNestingNode& node)
    {
        std::size_t depth = 0;

        std::optional<u16> parent =
            node.parentHeader;

        //
        // Guard protects against malformed nesting data.
        //
        std::size_t guard = 0;

        while (parent.has_value() &&
               guard < routine.nodes.size())
        {
            ++depth;
            ++guard;

            const LoopNestingNode* parentNode =
                routine.FindNode(
                    parent.value());

            if (parentNode == nullptr)
            {
                break;
            }

            parent =
                parentNode->parentHeader;
        }

        return depth;
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