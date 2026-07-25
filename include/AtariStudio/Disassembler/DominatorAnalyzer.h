#pragma once

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <optional>
#include <utility>
#include <vector>

#include <AtariStudio/Core/Types.h>
#include <AtariStudio/Disassembler/ControlFlowGraph.h>

namespace atari
{

struct DominatorNode
{
    //
    // CFG node being described.
    //
    u16 address = 0;

    //
    // All nodes that dominate this node.
    //
    std::vector<u16> dominators;

    //
    // Immediate dominator.
    //
    // Entry node has no immediate dominator.
    //
    std::optional<u16> immediateDominator;

    //
    // Depth in the dominator tree.
    //
    std::size_t depth = 0;

    [[nodiscard]]
    bool IsDominatedBy(
        u16 dominatorAddress) const noexcept
    {
        return std::binary_search(
            dominators.begin(),
            dominators.end(),
            dominatorAddress);
    }
};

struct DominatorBackEdge
{
    //
    // CFG edge:
    //
    // source -> target
    //
    // where target dominates source.
    //
    u16 sourceAddress = 0;
    u16 targetAddress = 0;

    ControlFlowGraphEdgeType type =
        ControlFlowGraphEdgeType::FallThrough;
};

struct RoutineDominatorAnalysis
{
    u16 routineEntryAddress = 0;

    std::vector<DominatorNode> nodes;

    std::vector<DominatorBackEdge> backEdges;

    [[nodiscard]]
    const DominatorNode* FindNode(
        u16 address) const noexcept
    {
        const auto iterator =
            std::find_if(
                nodes.begin(),
                nodes.end(),
                [address](
                    const DominatorNode& node)
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
    bool Dominates(
        u16 dominatorAddress,
        u16 nodeAddress) const noexcept
    {
        const DominatorNode* node =
            FindNode(
                nodeAddress);

        if (node == nullptr)
        {
            return false;
        }

        return
            node->IsDominatedBy(
                dominatorAddress);
    }
};

struct DominatorAnalysisResult
{
    std::vector<RoutineDominatorAnalysis> routines;

    [[nodiscard]]
    const RoutineDominatorAnalysis* FindRoutine(
        u16 entryAddress) const noexcept
    {
        const auto iterator =
            std::find_if(
                routines.begin(),
                routines.end(),
                [entryAddress](
                    const RoutineDominatorAnalysis& routine)
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
    std::size_t BackEdgeCount() const noexcept
    {
        std::size_t count = 0;

        for (const auto& routine :
             routines)
        {
            count +=
                routine.backEdges.size();
        }

        return count;
    }
};

class DominatorAnalyzer
{
public:

    [[nodiscard]]
    DominatorAnalysisResult Analyze(
        const ControlFlowGraphAnalysisResult& graphs) const
    {
        DominatorAnalysisResult result;

        for (const auto& graph :
             graphs.routines)
        {
            RoutineDominatorAnalysis routine;

            routine.routineEntryAddress =
                graph.routineEntryAddress;

            BuildDominators(
                graph,
                routine);

            BuildImmediateDominators(
                graph,
                routine);

            BuildDepths(
                routine);

            BuildBackEdges(
                graph,
                routine);

            result.routines.push_back(
                std::move(routine));
        }

        return result;
    }

private:

    static void BuildDominators(
        const RoutineControlFlowGraph& graph,
        RoutineDominatorAnalysis& result)
    {
        if (graph.nodes.empty())
        {
            return;
        }

        std::vector<u16> allAddresses;

        allAddresses.reserve(
            graph.nodes.size());

        for (const auto& node :
             graph.nodes)
        {
            allAddresses.push_back(
                node.address);
        }

        SortUnique(
            allAddresses);

        //
        // Initial state:
        //
        // entry:
        //     DOM(entry) = { entry }
        //
        // all other nodes:
        //     DOM(n) = all nodes
        //
        for (const auto& graphNode :
             graph.nodes)
        {
            DominatorNode node;

            node.address =
                graphNode.address;

            if (graphNode.address ==
                graph.routineEntryAddress)
            {
                node.dominators.push_back(
                    graphNode.address);
            }
            else
            {
                node.dominators =
                    allAddresses;
            }

            result.nodes.push_back(
                std::move(node));
        }

        std::sort(
            result.nodes.begin(),
            result.nodes.end(),
            [](const DominatorNode& left,
               const DominatorNode& right)
            {
                return
                    left.address <
                    right.address;
            });

        //
        // Classical iterative dominator algorithm:
        //
        // DOM(n) =
        //
        //     { n } UNION
        //
        //     intersection(
        //         DOM(p)
        //         for every predecessor p)
        //
        bool changed = true;

        while (changed)
        {
            changed = false;

            for (const auto& graphNode :
                 graph.nodes)
            {
                if (graphNode.address ==
                    graph.routineEntryAddress)
                {
                    continue;
                }

                DominatorNode* current =
                    FindMutableNode(
                        result,
                        graphNode.address);

                if (current == nullptr)
                {
                    continue;
                }

                std::vector<u16>
                    intersection;

                bool firstPredecessor = true;

                for (const u16 predecessorAddress :
                     graphNode.predecessors)
                {
                    const DominatorNode* predecessor =
                        result.FindNode(
                            predecessorAddress);

                    if (predecessor == nullptr)
                    {
                        continue;
                    }

                    if (firstPredecessor)
                    {
                        intersection =
                            predecessor->dominators;

                        firstPredecessor = false;
                    }
                    else
                    {
                        intersection =
                            IntersectSorted(
                                intersection,
                                predecessor->dominators);
                    }
                }

                //
                // A non-entry node with no predecessors is
                // unreachable from this routine entry.
                //
                // Keep only itself in that case.
                //
                if (firstPredecessor)
                {
                    intersection.clear();
                }

                intersection.push_back(
                    graphNode.address);

                SortUnique(
                    intersection);

                if (intersection !=
                    current->dominators)
                {
                    current->dominators =
                        std::move(
                            intersection);

                    changed = true;
                }
            }
        }
    }

    static void BuildImmediateDominators(
        const RoutineControlFlowGraph& graph,
        RoutineDominatorAnalysis& result)
    {
        for (auto& node :
             result.nodes)
        {
            if (node.address ==
                graph.routineEntryAddress)
            {
                node.immediateDominator.reset();
                continue;
            }

            std::vector<u16>
                strictDominators;

            for (const u16 dominator :
                 node.dominators)
            {
                if (dominator !=
                    node.address)
                {
                    strictDominators.push_back(
                        dominator);
                }
            }

            //
            // idom(n) is the strict dominator that is
            // itself dominated by every other strict
            // dominator of n.
            //
            for (const u16 candidate :
                 strictDominators)
            {
                const DominatorNode* candidateNode =
                    result.FindNode(
                        candidate);

                if (candidateNode == nullptr)
                {
                    continue;
                }

                bool dominatedByAllOthers =
                    true;

                for (const u16 other :
                     strictDominators)
                {
                    if (other == candidate)
                    {
                        continue;
                    }

                    if (!candidateNode->
                            IsDominatedBy(
                                other))
                    {
                        dominatedByAllOthers =
                            false;

                        break;
                    }
                }

                if (dominatedByAllOthers)
                {
                    node.immediateDominator =
                        candidate;

                    break;
                }
            }
        }
    }

    static void BuildDepths(
        RoutineDominatorAnalysis& result)
    {
        for (auto& node :
             result.nodes)
        {
            std::size_t depth = 0;

            std::optional<u16> current =
                node.immediateDominator;

            //
            // Guard against malformed input.
            //
            std::size_t guard = 0;

            while (current.has_value() &&
                   guard < result.nodes.size())
            {
                ++depth;
                ++guard;

                const DominatorNode* parent =
                    result.FindNode(
                        current.value());

                if (parent == nullptr)
                {
                    break;
                }

                current =
                    parent->
                        immediateDominator;
            }

            node.depth =
                depth;
        }
    }

    static void BuildBackEdges(
        const RoutineControlFlowGraph& graph,
        RoutineDominatorAnalysis& result)
    {
        for (const auto& edge :
             graph.edges)
        {
            //
            // Edge A -> B is a back edge when
            // B dominates A.
            //
            if (!result.Dominates(
                    edge.targetAddress,
                    edge.sourceAddress))
            {
                continue;
            }

            DominatorBackEdge backEdge;

            backEdge.sourceAddress =
                edge.sourceAddress;

            backEdge.targetAddress =
                edge.targetAddress;

            backEdge.type =
                edge.type;

            result.backEdges.push_back(
                backEdge);
        }

        std::sort(
            result.backEdges.begin(),
            result.backEdges.end(),
            [](const DominatorBackEdge& left,
               const DominatorBackEdge& right)
            {
                if (left.sourceAddress !=
                    right.sourceAddress)
                {
                    return
                        left.sourceAddress <
                        right.sourceAddress;
                }

                if (left.targetAddress !=
                    right.targetAddress)
                {
                    return
                        left.targetAddress <
                        right.targetAddress;
                }

                return
                    static_cast<int>(
                        left.type) <
                    static_cast<int>(
                        right.type);
            });

        result.backEdges.erase(
            std::unique(
                result.backEdges.begin(),
                result.backEdges.end(),
                [](const DominatorBackEdge& left,
                   const DominatorBackEdge& right)
                {
                    return
                        left.sourceAddress ==
                            right.sourceAddress &&
                        left.targetAddress ==
                            right.targetAddress &&
                        left.type ==
                            right.type;
                }),
            result.backEdges.end());
    }

    [[nodiscard]]
    static DominatorNode* FindMutableNode(
        RoutineDominatorAnalysis& result,
        u16 address)
    {
        const auto iterator =
            std::find_if(
                result.nodes.begin(),
                result.nodes.end(),
                [address](
                    const DominatorNode& node)
                {
                    return
                        node.address ==
                        address;
                });

        if (iterator ==
            result.nodes.end())
        {
            return nullptr;
        }

        return &*iterator;
    }

    [[nodiscard]]
    static std::vector<u16> IntersectSorted(
        const std::vector<u16>& left,
        const std::vector<u16>& right)
    {
        std::vector<u16> result;

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