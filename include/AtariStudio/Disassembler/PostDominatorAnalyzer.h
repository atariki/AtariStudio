#pragma once

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <AtariStudio/Core/Types.h>
#include <AtariStudio/Disassembler/ControlFlowGraph.h>

namespace atari
{

struct PostDominatorNode
{
    u16 address = 0;

    //
    // All CFG nodes that post-dominate this node.
    //
    std::vector<u16> postDominators;

    //
    // Immediate post-dominator.
    //
    // May be empty for:
    //
    // - terminal nodes;
    // - nodes whose paths terminate through different exits;
    // - CFGs without a real exit.
    //
    std::optional<u16> immediatePostDominator;

    //
    // Depth in the post-dominator forest.
    //
    std::size_t depth = 0;

    //
    // True when this CFG node has no internal successors.
    //
    bool terminal = false;

    [[nodiscard]]
    bool IsPostDominatedBy(
        u16 addressValue) const noexcept
    {
        return std::binary_search(
            postDominators.begin(),
            postDominators.end(),
            addressValue);
    }
};

struct RoutinePostDominatorAnalysis
{
    u16 routineEntryAddress = 0;

    std::string routineName;

    std::vector<PostDominatorNode> nodes;

    //
    // Real CFG exits.
    //
    std::vector<u16> terminalAddresses;

    //
    // Diagnostic information.
    //
    // The fixed-point algorithm has a hard upper
    // iteration limit, therefore it cannot hang.
    //
    bool converged = true;

    std::size_t iterations = 0;

    [[nodiscard]]
    const PostDominatorNode* FindNode(
        u16 address) const noexcept
    {
        const auto iterator =
            std::find_if(
                nodes.begin(),
                nodes.end(),
                [address](
                    const PostDominatorNode& node)
                {
                    return
                        node.address ==
                        address;
                });

        if (iterator == nodes.end())
        {
            return nullptr;
        }

        return &*iterator;
    }

    [[nodiscard]]
    PostDominatorNode* FindNode(
        u16 address) noexcept
    {
        const auto iterator =
            std::find_if(
                nodes.begin(),
                nodes.end(),
                [address](
                    const PostDominatorNode& node)
                {
                    return
                        node.address ==
                        address;
                });

        if (iterator == nodes.end())
        {
            return nullptr;
        }

        return &*iterator;
    }

    [[nodiscard]]
    bool PostDominates(
        u16 postDominatorAddress,
        u16 nodeAddress) const noexcept
    {
        const PostDominatorNode* node =
            FindNode(
                nodeAddress);

        if (node == nullptr)
        {
            return false;
        }

        return
            node->IsPostDominatedBy(
                postDominatorAddress);
    }

    [[nodiscard]]
    std::size_t TerminalCount() const noexcept
    {
        return terminalAddresses.size();
    }
};

struct PostDominatorAnalysisResult
{
    std::vector<RoutinePostDominatorAnalysis> routines;

    [[nodiscard]]
    const RoutinePostDominatorAnalysis* FindRoutine(
        u16 entryAddress) const noexcept
    {
        const auto iterator =
            std::find_if(
                routines.begin(),
                routines.end(),
                [entryAddress](
                    const RoutinePostDominatorAnalysis& routine)
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
    std::size_t TerminalCount() const noexcept
    {
        std::size_t count = 0;

        for (const auto& routine :
             routines)
        {
            count +=
                routine.terminalAddresses.size();
        }

        return count;
    }

    [[nodiscard]]
    std::size_t ConvergedRoutineCount() const noexcept
    {
        return static_cast<std::size_t>(
            std::count_if(
                routines.begin(),
                routines.end(),
                [](const RoutinePostDominatorAnalysis& routine)
                {
                    return routine.converged;
                }));
    }
};

class PostDominatorAnalyzer
{
public:

    [[nodiscard]]
    PostDominatorAnalysisResult Analyze(
        const ControlFlowGraphAnalysisResult& graphs) const
    {
        PostDominatorAnalysisResult result;

        for (const auto& graph :
             graphs.routines)
        {
            RoutinePostDominatorAnalysis
                routineResult;

            routineResult.routineEntryAddress =
                graph.routineEntryAddress;

            routineResult.routineName =
                graph.routineName;

            BuildPostDominators(
                graph,
                routineResult);

            BuildImmediatePostDominators(
                routineResult);

            BuildDepths(
                routineResult);

            result.routines.push_back(
                std::move(
                    routineResult));
        }

        return result;
    }

private:

    static void BuildPostDominators(
        const RoutineControlFlowGraph& graph,
        RoutinePostDominatorAnalysis& result)
    {
        if (graph.nodes.empty())
        {
            result.converged = true;
            result.iterations = 0;
            return;
        }

        std::vector<u16> allAddresses;

        allAddresses.reserve(
            graph.nodes.size());

        //
        // -------------------------------------------------
        // Phase 1:
        // Collect all CFG node addresses.
        // -------------------------------------------------
        //
        for (const auto& graphNode :
             graph.nodes)
        {
            allAddresses.push_back(
                graphNode.address);
        }

        SortUnique(
            allAddresses);

        //
        // -------------------------------------------------
        // Phase 2:
        // Initialize post-dominator sets.
        //
        // Terminal:
        //
        //     PDOM(exit) = { exit }
        //
        // Non-terminal:
        //
        //     PDOM(n) = all nodes
        //
        // -------------------------------------------------
        //
        for (const auto& graphNode :
             graph.nodes)
        {
            PostDominatorNode node;

            node.address =
                graphNode.address;

            node.terminal =
                graphNode.terminal ||
                graphNode.successors.empty();

            if (node.terminal)
            {
                node.postDominators.push_back(
                    graphNode.address);

                result.terminalAddresses.push_back(
                    graphNode.address);
            }
            else
            {
                node.postDominators =
                    allAddresses;
            }

            result.nodes.push_back(
                std::move(node));
        }

        std::sort(
            result.nodes.begin(),
            result.nodes.end(),
            [](const PostDominatorNode& left,
               const PostDominatorNode& right)
            {
                return
                    left.address <
                    right.address;
            });

        SortUnique(
            result.terminalAddresses);

        //
        // -------------------------------------------------
        // CFG without any exit.
        //
        // Example:
        //
        //     loop:
        //         ...
        //         JMP loop
        //
        // A classical real-exit post-dominator relation is
        // undefined for such a graph.
        //
        // Be conservative and keep only each node itself.
        //
        // Most importantly: do not iterate forever.
        // -------------------------------------------------
        //
        if (result.terminalAddresses.empty())
        {
            for (auto& node :
                 result.nodes)
            {
                node.postDominators.clear();

                node.postDominators.push_back(
                    node.address);
            }

            result.converged = true;
            result.iterations = 0;

            return;
        }

        //
        // -------------------------------------------------
        // Phase 3:
        // Fixed-point algorithm.
        //
        // PDOM(n) =
        //
        //     { n } UNION
        //
        //     intersection(
        //         PDOM(successor)
        //     )
        //
        // -------------------------------------------------
        //
        bool changed = true;

        const std::size_t nodeCount =
            result.nodes.size();

        //
        // Every set can only lose elements.
        //
        // Therefore convergence requires at most a finite
        // number of changes. This limit is deliberately
        // generous and prevents any accidental hang.
        //
        const std::size_t maximumIterations =
            nodeCount * nodeCount +
            nodeCount +
            1;

        result.iterations = 0;

        while (changed &&
               result.iterations <
                   maximumIterations)
        {
            changed = false;

            ++result.iterations;

            for (const auto& graphNode :
                 graph.nodes)
            {
                PostDominatorNode* current =
                    result.FindNode(
                        graphNode.address);

                if (current == nullptr)
                {
                    continue;
                }

                if (current->terminal)
                {
                    continue;
                }

                std::vector<u16>
                    intersection;

                bool firstSuccessor = true;

                for (const u16 successorAddress :
                     graphNode.successors)
                {
                    const PostDominatorNode* successor =
                        result.FindNode(
                            successorAddress);

                    if (successor == nullptr)
                    {
                        continue;
                    }

                    if (firstSuccessor)
                    {
                        intersection =
                            successor->postDominators;

                        firstSuccessor = false;
                    }
                    else
                    {
                        intersection =
                            IntersectSorted(
                                intersection,
                                successor->
                                    postDominators);
                    }
                }

                //
                // Defensive fallback for malformed CFG
                // nodes having no valid successor.
                //
                if (firstSuccessor)
                {
                    intersection.clear();
                }

                intersection.push_back(
                    graphNode.address);

                SortUnique(
                    intersection);

                if (intersection !=
                    current->postDominators)
                {
                    current->postDominators =
                        std::move(
                            intersection);

                    changed = true;
                }
            }
        }

        result.converged =
            !changed;
    }

    static void BuildImmediatePostDominators(
        RoutinePostDominatorAnalysis& result)
    {
        for (auto& node :
             result.nodes)
        {
            node.immediatePostDominator.reset();

            if (node.terminal)
            {
                continue;
            }

            std::vector<u16>
                strictPostDominators;

            for (const u16 postDominator :
                 node.postDominators)
            {
                if (postDominator !=
                    node.address)
                {
                    strictPostDominators.push_back(
                        postDominator);
                }
            }

            //
            // There may be no real common post-dominator
            // when a node can terminate through multiple
            // independent exits.
            //
            if (strictPostDominators.empty())
            {
                continue;
            }

            //
            // Immediate post-dominator is the strict
            // post-dominator which is itself
            // post-dominated by every other strict
            // post-dominator.
            //
            for (const u16 candidate :
                 strictPostDominators)
            {
                const PostDominatorNode* candidateNode =
                    result.FindNode(
                        candidate);

                if (candidateNode == nullptr)
                {
                    continue;
                }

                bool postDominatedByAllOthers =
                    true;

                for (const u16 other :
                     strictPostDominators)
                {
                    if (other == candidate)
                    {
                        continue;
                    }

                    if (!candidateNode->
                            IsPostDominatedBy(
                                other))
                    {
                        postDominatedByAllOthers =
                            false;

                        break;
                    }
                }

                if (!postDominatedByAllOthers)
                {
                    continue;
                }

                node.immediatePostDominator =
                    candidate;

                break;
            }
        }
    }

    static void BuildDepths(
        RoutinePostDominatorAnalysis& result)
    {
        for (auto& node :
             result.nodes)
        {
            std::size_t depth = 0;

            std::optional<u16> current =
                node.immediatePostDominator;

            //
            // No recursion.
            //
            // The guard also protects against malformed
            // parent cycles.
            //
            std::size_t guard = 0;

            while (current.has_value() &&
                   guard < result.nodes.size())
            {
                ++depth;
                ++guard;

                const PostDominatorNode* parent =
                    result.FindNode(
                        current.value());

                if (parent == nullptr)
                {
                    break;
                }

                current =
                    parent->
                        immediatePostDominator;
            }

            node.depth =
                depth;
        }
    }

    [[nodiscard]]
    static std::vector<u16> IntersectSorted(
        const std::vector<u16>& left,
        const std::vector<u16>& right)
    {
        std::vector<u16> result;

        result.reserve(
            std::min(
                left.size(),
                right.size()));

        std::set_intersection(
            left.begin(),
            left.end(),
            right.begin(),
            right.end(),
            std::back_inserter(
                result));

        return result;
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