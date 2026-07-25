#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include <AtariStudio/Core/Types.h>
#include <AtariStudio/Disassembler/BasicBlockAnalyzer.h>

namespace atari
{

enum class ControlFlowGraphEdgeType
{
    FallThrough,
    BranchTaken,
    Jump
};

struct ControlFlowGraphEdge
{
    u16 sourceAddress = 0;
    u16 targetAddress = 0;

    ControlFlowGraphEdgeType type =
        ControlFlowGraphEdgeType::FallThrough;
};

struct ControlFlowGraphNode
{
    //
    // Basic block start.
    //
    u16 address = 0;

    //
    // Last byte occupied by the block.
    //
    u16 endAddress = 0;

    std::vector<u16> instructionAddresses;

    //
    // Addresses of predecessor blocks.
    //
    std::vector<u16> predecessors;

    //
    // Addresses of successor blocks.
    //
    std::vector<u16> successors;

    bool entry = false;
    bool terminal = false;

    [[nodiscard]]
    std::size_t InstructionCount() const noexcept
    {
        return instructionAddresses.size();
    }
};

struct RoutineControlFlowGraph
{
    u16 routineEntryAddress = 0;

    std::string routineName;

    std::vector<ControlFlowGraphNode> nodes;

    std::vector<ControlFlowGraphEdge> edges;

    [[nodiscard]]
    const ControlFlowGraphNode* FindNode(
        u16 address) const noexcept
    {
        const auto iterator =
            std::find_if(
                nodes.begin(),
                nodes.end(),
                [address](
                    const ControlFlowGraphNode& node)
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
    ControlFlowGraphNode* FindNode(
        u16 address) noexcept
    {
        const auto iterator =
            std::find_if(
                nodes.begin(),
                nodes.end(),
                [address](
                    const ControlFlowGraphNode& node)
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
};

struct ControlFlowGraphAnalysisResult
{
    std::vector<RoutineControlFlowGraph> routines;

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
    std::size_t EdgeCount() const noexcept
    {
        std::size_t count = 0;

        for (const auto& routine :
             routines)
        {
            count +=
                routine.edges.size();
        }

        return count;
    }

    [[nodiscard]]
    const RoutineControlFlowGraph* FindRoutine(
        u16 entryAddress) const noexcept
    {
        const auto iterator =
            std::find_if(
                routines.begin(),
                routines.end(),
                [entryAddress](
                    const RoutineControlFlowGraph& routine)
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

class ControlFlowGraphBuilder
{
public:

    [[nodiscard]]
    ControlFlowGraphAnalysisResult Build(
        const BasicBlockAnalysisResult& basicBlocks) const
    {
        ControlFlowGraphAnalysisResult result;

        for (const auto& sourceRoutine :
             basicBlocks.routines)
        {
            RoutineControlFlowGraph graph;

            graph.routineEntryAddress =
                sourceRoutine.routineEntryAddress;

            graph.routineName =
                sourceRoutine.routineName;

            //
            // -------------------------------------------------
            // Phase 1:
            // Create CFG nodes
            // -------------------------------------------------
            //
            for (const auto& block :
                 sourceRoutine.blocks)
            {
                ControlFlowGraphNode node;

                node.address =
                    block.beginAddress;

                node.endAddress =
                    block.endAddress;

                node.instructionAddresses =
                    block.instructionAddresses;

                node.entry =
                    block.beginAddress ==
                    sourceRoutine.routineEntryAddress;

                node.terminal =
                    block.terminal;

                graph.nodes.push_back(
                    std::move(node));
            }

            std::sort(
                graph.nodes.begin(),
                graph.nodes.end(),
                [](const ControlFlowGraphNode& left,
                   const ControlFlowGraphNode& right)
                {
                    return
                        left.address <
                        right.address;
                });

            //
            // -------------------------------------------------
            // Phase 2:
            // Create CFG edges
            // -------------------------------------------------
            //
            for (const auto& block :
                 sourceRoutine.blocks)
            {
                for (const auto& successor :
                     block.successors)
                {
                    ControlFlowGraphEdge edge;

                    edge.sourceAddress =
                        block.beginAddress;

                    edge.targetAddress =
                        successor.targetAddress;

                    edge.type =
                        ConvertEdgeType(
                            successor.type);

                    graph.edges.push_back(
                        edge);
                }
            }

            SortUniqueEdges(
                graph.edges);

            //
            // -------------------------------------------------
            // Phase 3:
            // Build predecessor / successor lists
            // -------------------------------------------------
            //
            for (const auto& edge :
                 graph.edges)
            {
                ControlFlowGraphNode* source =
                    graph.FindNode(
                        edge.sourceAddress);

                ControlFlowGraphNode* target =
                    graph.FindNode(
                        edge.targetAddress);

                if (source == nullptr ||
                    target == nullptr)
                {
                    continue;
                }

                source->successors.push_back(
                    target->address);

                target->predecessors.push_back(
                    source->address);
            }

            for (auto& node :
                 graph.nodes)
            {
                SortUniqueAddresses(
                    node.successors);

                SortUniqueAddresses(
                    node.predecessors);
            }

            result.routines.push_back(
                std::move(graph));
        }

        return result;
    }

private:

    [[nodiscard]]
    static ControlFlowGraphEdgeType ConvertEdgeType(
        BasicBlockEdgeType type)
    {
        switch (type)
        {
        case BasicBlockEdgeType::FallThrough:
            return
                ControlFlowGraphEdgeType::
                    FallThrough;

        case BasicBlockEdgeType::BranchTaken:
            return
                ControlFlowGraphEdgeType::
                    BranchTaken;

        case BasicBlockEdgeType::Jump:
            return
                ControlFlowGraphEdgeType::
                    Jump;

        default:
            return
                ControlFlowGraphEdgeType::
                    FallThrough;
        }
    }

    static void SortUniqueAddresses(
        std::vector<u16>& addresses)
    {
        std::sort(
            addresses.begin(),
            addresses.end());

        addresses.erase(
            std::unique(
                addresses.begin(),
                addresses.end()),
            addresses.end());
    }

    static void SortUniqueEdges(
        std::vector<ControlFlowGraphEdge>& edges)
    {
        std::sort(
            edges.begin(),
            edges.end(),
            [](const ControlFlowGraphEdge& left,
               const ControlFlowGraphEdge& right)
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

        edges.erase(
            std::unique(
                edges.begin(),
                edges.end(),
                [](const ControlFlowGraphEdge& left,
                   const ControlFlowGraphEdge& right)
                {
                    return
                        left.sourceAddress ==
                            right.sourceAddress &&
                        left.targetAddress ==
                            right.targetAddress &&
                        left.type ==
                            right.type;
                }),
            edges.end());
    }
};

} // namespace atari