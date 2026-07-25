#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <deque>
#include <string>
#include <utility>
#include <vector>

#include <AtariStudio/Core/Types.h>
#include <AtariStudio/Disassembler/ControlFlowGraph.h>
#include <AtariStudio/Disassembler/PostDominatorAnalyzer.h>

namespace atari
{

enum class ConditionalRegionKind
{
    IfThen,
    IfElse
};

struct ConditionalRegion
{
    //
    // Basic block containing the conditional branch.
    //
    u16 headerAddress = 0;

    //
    // Branch-taken successor.
    //
    u16 branchTargetAddress = 0;

    //
    // Fall-through successor.
    //
    u16 fallthroughTargetAddress = 0;

    //
    // First common block after the conditional region.
    //
    u16 joinAddress = 0;

    ConditionalRegionKind kind =
        ConditionalRegionKind::IfThen;

    //
    // Blocks reachable through the branch-taken edge
    // before reaching joinAddress.
    //
    std::vector<u16> branchBlocks;

    //
    // Blocks reachable through the fall-through edge
    // before reaching joinAddress.
    //
    std::vector<u16> fallthroughBlocks;

    [[nodiscard]]
    bool IsIfThen() const noexcept
    {
        return
            kind ==
            ConditionalRegionKind::IfThen;
    }

    [[nodiscard]]
    bool IsIfElse() const noexcept
    {
        return
            kind ==
            ConditionalRegionKind::IfElse;
    }
};

struct RoutineConditionalAnalysis
{
    u16 routineEntryAddress = 0;

    std::string routineName;

    std::vector<ConditionalRegion> regions;

    [[nodiscard]]
    const ConditionalRegion* FindRegion(
        u16 headerAddress) const noexcept
    {
        const auto iterator =
            std::find_if(
                regions.begin(),
                regions.end(),
                [headerAddress](
                    const ConditionalRegion& region)
                {
                    return
                        region.headerAddress ==
                        headerAddress;
                });

        if (iterator == regions.end())
        {
            return nullptr;
        }

        return &*iterator;
    }

    [[nodiscard]]
    std::size_t IfThenCount() const noexcept
    {
        return static_cast<std::size_t>(
            std::count_if(
                regions.begin(),
                regions.end(),
                [](const ConditionalRegion& region)
                {
                    return region.IsIfThen();
                }));
    }

    [[nodiscard]]
    std::size_t IfElseCount() const noexcept
    {
        return static_cast<std::size_t>(
            std::count_if(
                regions.begin(),
                regions.end(),
                [](const ConditionalRegion& region)
                {
                    return region.IsIfElse();
                }));
    }
};

struct ConditionalRegionAnalysisResult
{
    std::vector<RoutineConditionalAnalysis> routines;

    [[nodiscard]]
    const RoutineConditionalAnalysis* FindRoutine(
        u16 entryAddress) const noexcept
    {
        const auto iterator =
            std::find_if(
                routines.begin(),
                routines.end(),
                [entryAddress](
                    const RoutineConditionalAnalysis& routine)
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
    std::size_t RegionCount() const noexcept
    {
        std::size_t count = 0;

        for (const auto& routine :
             routines)
        {
            count +=
                routine.regions.size();
        }

        return count;
    }

    [[nodiscard]]
    std::size_t IfThenCount() const noexcept
    {
        std::size_t count = 0;

        for (const auto& routine :
             routines)
        {
            count +=
                routine.IfThenCount();
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
};

class ConditionalRegionAnalyzer
{
public:

    [[nodiscard]]
    ConditionalRegionAnalysisResult Analyze(
        const ControlFlowGraphAnalysisResult& graphs,
        const PostDominatorAnalysisResult& postDominators) const
    {
        ConditionalRegionAnalysisResult result;

        for (const auto& graph :
             graphs.routines)
        {
            RoutineConditionalAnalysis
                routineResult;

            routineResult.routineEntryAddress =
                graph.routineEntryAddress;

            routineResult.routineName =
                graph.routineName;

            const auto* routinePostDominators =
                postDominators.FindRoutine(
                    graph.routineEntryAddress);

            if (routinePostDominators == nullptr ||
                !routinePostDominators->converged)
            {
                result.routines.push_back(
                    std::move(
                        routineResult));

                continue;
            }

            for (const auto& node :
                 graph.nodes)
            {
                AnalyzeNode(
                    graph,
                    *routinePostDominators,
                    node,
                    routineResult);
            }

            std::sort(
                routineResult.regions.begin(),
                routineResult.regions.end(),
                [](const ConditionalRegion& left,
                   const ConditionalRegion& right)
                {
                    return
                        left.headerAddress <
                        right.headerAddress;
                });

            routineResult.regions.erase(
                std::unique(
                    routineResult.regions.begin(),
                    routineResult.regions.end(),
                    [](const ConditionalRegion& left,
                       const ConditionalRegion& right)
                    {
                        return
                            left.headerAddress ==
                                right.headerAddress;
                    }),
                routineResult.regions.end());

            result.routines.push_back(
                std::move(
                    routineResult));
        }

        return result;
    }

private:

    struct PathCollection
    {
        std::vector<u16> blocks;

        bool reachedJoin = false;

        bool reenteredHeader = false;

        bool escapedBeforeJoin = false;
    };

    static void AnalyzeNode(
        const RoutineControlFlowGraph& graph,
        const RoutinePostDominatorAnalysis& postDominators,
        const ControlFlowGraphNode& node,
        RoutineConditionalAnalysis& result)
    {
        //
        // A conditional branch must have exactly two
        // different CFG successors.
        //
        if (node.successors.size() != 2)
        {
            return;
        }

        const ControlFlowGraphEdge*
            branchEdge = nullptr;

        const ControlFlowGraphEdge*
            fallthroughEdge = nullptr;

        //
        // Find semantic branch edges.
        //
        for (const auto& edge :
             graph.edges)
        {
            if (edge.sourceAddress !=
                node.address)
            {
                continue;
            }

            switch (edge.type)
            {
            case ControlFlowGraphEdgeType::BranchTaken:

                if (branchEdge != nullptr)
                {
                    return;
                }

                branchEdge =
                    &edge;

                break;

            case ControlFlowGraphEdgeType::FallThrough:

                if (fallthroughEdge != nullptr)
                {
                    return;
                }

                fallthroughEdge =
                    &edge;

                break;

            case ControlFlowGraphEdgeType::Jump:
            default:

                break;
            }
        }

        if (branchEdge == nullptr ||
            fallthroughEdge == nullptr)
        {
            return;
        }

        if (branchEdge->targetAddress ==
            fallthroughEdge->targetAddress)
        {
            return;
        }

        //
        // The immediate post-dominator is our candidate
        // merge/join block.
        //
        const PostDominatorNode* postNode =
            postDominators.FindNode(
                node.address);

        if (postNode == nullptr ||
            !postNode->
                immediatePostDominator.has_value())
        {
            return;
        }

        const u16 joinAddress =
            postNode->
                immediatePostDominator.value();

        if (joinAddress ==
            node.address)
        {
            return;
        }

        if (graph.FindNode(
                joinAddress) == nullptr)
        {
            return;
        }

        //
        // Collect both arms independently.
        //
        const PathCollection branchPath =
            CollectPath(
                graph,
                branchEdge->targetAddress,
                joinAddress,
                node.address);

        const PathCollection fallthroughPath =
            CollectPath(
                graph,
                fallthroughEdge->targetAddress,
                joinAddress,
                node.address);

        //
        // Structured conditionals must actually reach
        // their common join.
        //
        if (!branchPath.reachedJoin ||
            !fallthroughPath.reachedJoin)
        {
            return;
        }

        //
        // If an arm comes back to the conditional header
        // before reaching the join, this is loop control,
        // not a simple if/else region.
        //
        if (branchPath.reenteredHeader ||
            fallthroughPath.reenteredHeader)
        {
            return;
        }

        //
        // A terminal path before the proposed join means
        // the region has multiple independent exits.
        //
        if (branchPath.escapedBeforeJoin ||
            fallthroughPath.escapedBeforeJoin)
        {
            return;
        }

        //
        // The two arms must not share ordinary blocks
        // before the join.
        //
        if (PathsOverlap(
                branchPath.blocks,
                fallthroughPath.blocks))
        {
            return;
        }

        const bool branchDirectToJoin =
            branchEdge->targetAddress ==
            joinAddress;

        const bool fallthroughDirectToJoin =
            fallthroughEdge->targetAddress ==
            joinAddress;

        //
        // Both arms pointing directly to the same join
        // is not a meaningful conditional region.
        //
        if (branchDirectToJoin &&
            fallthroughDirectToJoin)
        {
            return;
        }

        ConditionalRegion region;

        region.headerAddress =
            node.address;

        region.branchTargetAddress =
            branchEdge->targetAddress;

        region.fallthroughTargetAddress =
            fallthroughEdge->targetAddress;

        region.joinAddress =
            joinAddress;

        region.branchBlocks =
            branchPath.blocks;

        region.fallthroughBlocks =
            fallthroughPath.blocks;

        //
        // One arm goes directly to the join:
        //
        //     if (...) {
        //         ...
        //     }
        //
        // Both arms contain blocks:
        //
        //     if (...) {
        //         ...
        //     } else {
        //         ...
        //     }
        //
        if (branchDirectToJoin ||
            fallthroughDirectToJoin)
        {
            region.kind =
                ConditionalRegionKind::IfThen;
        }
        else
        {
            if (region.branchBlocks.empty() ||
                region.fallthroughBlocks.empty())
            {
                return;
            }

            region.kind =
                ConditionalRegionKind::IfElse;
        }

        result.regions.push_back(
            std::move(region));
    }

    [[nodiscard]]
    static PathCollection CollectPath(
        const RoutineControlFlowGraph& graph,
        u16 startAddress,
        u16 joinAddress,
        u16 headerAddress)
    {
        PathCollection result;

        //
        // Direct edge to join means an empty arm.
        //
        if (startAddress ==
            joinAddress)
        {
            result.reachedJoin = true;

            return result;
        }

        //
        // Direct branch back to the conditional header
        // is loop control.
        //
        if (startAddress ==
            headerAddress)
        {
            result.reenteredHeader = true;

            return result;
        }

        std::array<bool, MemorySize>
            visited{};

        std::deque<u16>
            workList;

        visited[startAddress] = true;

        workList.push_back(
            startAddress);

        while (!workList.empty())
        {
            const u16 address =
                workList.front();

            workList.pop_front();

            if (address ==
                joinAddress)
            {
                result.reachedJoin = true;
                continue;
            }

            if (address ==
                headerAddress)
            {
                result.reenteredHeader = true;
                continue;
            }

            const ControlFlowGraphNode* node =
                graph.FindNode(
                    address);

            if (node == nullptr)
            {
                result.escapedBeforeJoin = true;
                continue;
            }

            result.blocks.push_back(
                address);

            if (node->successors.empty())
            {
                result.escapedBeforeJoin = true;
                continue;
            }

            for (const u16 successor :
                 node->successors)
            {
                if (successor ==
                    joinAddress)
                {
                    result.reachedJoin = true;
                    continue;
                }

                if (successor ==
                    headerAddress)
                {
                    result.reenteredHeader = true;
                    continue;
                }

                if (visited[successor])
                {
                    continue;
                }

                visited[successor] = true;

                workList.push_back(
                    successor);
            }
        }

        SortUnique(
            result.blocks);

        return result;
    }

    [[nodiscard]]
    static bool PathsOverlap(
        const std::vector<u16>& left,
        const std::vector<u16>& right)
    {
        std::size_t leftIndex = 0;
        std::size_t rightIndex = 0;

        while (leftIndex < left.size() &&
               rightIndex < right.size())
        {
            if (left[leftIndex] ==
                right[rightIndex])
            {
                return true;
            }

            if (left[leftIndex] <
                right[rightIndex])
            {
                ++leftIndex;
            }
            else
            {
                ++rightIndex;
            }
        }

        return false;
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