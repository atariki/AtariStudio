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
#include <AtariStudio/Disassembler/DominatorAnalyzer.h>

namespace atari
{

struct NaturalLoopExit
{
    u16 sourceAddress = 0;
    u16 targetAddress = 0;

    ControlFlowGraphEdgeType type =
        ControlFlowGraphEdgeType::FallThrough;
};

struct NaturalLoop
{
    //
    // Loop entry / header.
    //
    // The header dominates every latch belonging
    // to this natural loop.
    //
    u16 headerAddress = 0;

    //
    // Sources of back edges:
    //
    //     latch -> header
    //
    // One natural loop may have more than one latch.
    //
    std::vector<u16> latchAddresses;

    //
    // Basic-block addresses belonging to this loop.
    //
    std::vector<u16> blockAddresses;

    //
    // CFG edges leaving the loop.
    //
    std::vector<NaturalLoopExit> exits;

    [[nodiscard]]
    std::size_t BlockCount() const noexcept
    {
        return blockAddresses.size();
    }

    [[nodiscard]]
    std::size_t LatchCount() const noexcept
    {
        return latchAddresses.size();
    }

    [[nodiscard]]
    std::size_t ExitCount() const noexcept
    {
        return exits.size();
    }

    [[nodiscard]]
    bool Contains(
        u16 address) const noexcept
    {
        return std::binary_search(
            blockAddresses.begin(),
            blockAddresses.end(),
            address);
    }

    [[nodiscard]]
    bool IsSelfLoop() const noexcept
    {
        if (blockAddresses.size() != 1)
        {
            return false;
        }

        if (blockAddresses.front() !=
            headerAddress)
        {
            return false;
        }

        return std::binary_search(
            latchAddresses.begin(),
            latchAddresses.end(),
            headerAddress);
    }
};

struct RoutineNaturalLoopAnalysis
{
    u16 routineEntryAddress = 0;

    std::string routineName;

    std::vector<NaturalLoop> loops;

    [[nodiscard]]
    const NaturalLoop* FindLoop(
        u16 headerAddress) const noexcept
    {
        const auto iterator =
            std::find_if(
                loops.begin(),
                loops.end(),
                [headerAddress](
                    const NaturalLoop& loop)
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
};

struct NaturalLoopAnalysisResult
{
    std::vector<RoutineNaturalLoopAnalysis> routines;

    [[nodiscard]]
    const RoutineNaturalLoopAnalysis* FindRoutine(
        u16 entryAddress) const noexcept
    {
        const auto iterator =
            std::find_if(
                routines.begin(),
                routines.end(),
                [entryAddress](
                    const RoutineNaturalLoopAnalysis& routine)
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
    std::size_t ExitCount() const noexcept
    {
        std::size_t count = 0;

        for (const auto& routine :
             routines)
        {
            for (const auto& loop :
                 routine.loops)
            {
                count +=
                    loop.exits.size();
            }
        }

        return count;
    }
};

class NaturalLoopAnalyzer
{
public:

    [[nodiscard]]
    NaturalLoopAnalysisResult Analyze(
        const ControlFlowGraphAnalysisResult& graphs,
        const DominatorAnalysisResult& dominators) const
    {
        NaturalLoopAnalysisResult result;

        for (const auto& graph :
             graphs.routines)
        {
            RoutineNaturalLoopAnalysis
                routineResult;

            routineResult.routineEntryAddress =
                graph.routineEntryAddress;

            routineResult.routineName =
                graph.routineName;

            const auto* routineDominators =
                dominators.FindRoutine(
                    graph.routineEntryAddress);

            if (routineDominators == nullptr)
            {
                result.routines.push_back(
                    std::move(
                        routineResult));

                continue;
            }

            //
            // =================================================
            // Phase 1:
            // Convert every back edge into a natural loop.
            //
            // Loops with the same header are merged.
            // =================================================
            //
            for (const auto& backEdge :
                 routineDominators->backEdges)
            {
                NaturalLoop* loop =
                    FindMutableLoop(
                        routineResult,
                        backEdge.targetAddress);

                if (loop == nullptr)
                {
                    NaturalLoop newLoop;

                    newLoop.headerAddress =
                        backEdge.targetAddress;

                    routineResult.loops.push_back(
                        std::move(
                            newLoop));

                    loop =
                        &routineResult.loops.back();
                }

                loop->latchAddresses.push_back(
                    backEdge.sourceAddress);

                const auto members =
                    BuildNaturalLoopMembers(
                        graph,
                        *routineDominators,
                        backEdge.sourceAddress,
                        backEdge.targetAddress);

                loop->blockAddresses.insert(
                    loop->blockAddresses.end(),
                    members.begin(),
                    members.end());
            }

            //
            // =================================================
            // Phase 2:
            // Normalize merged loops.
            // =================================================
            //
            for (auto& loop :
                 routineResult.loops)
            {
                SortUniqueAddresses(
                    loop.latchAddresses);

                SortUniqueAddresses(
                    loop.blockAddresses);
            }

            //
            // =================================================
            // Phase 3:
            // Find CFG edges leaving each loop.
            // =================================================
            //
            for (auto& loop :
                 routineResult.loops)
            {
                BuildLoopExits(
                    graph,
                    loop);
            }

            std::sort(
                routineResult.loops.begin(),
                routineResult.loops.end(),
                [](const NaturalLoop& left,
                   const NaturalLoop& right)
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
    static NaturalLoop* FindMutableLoop(
        RoutineNaturalLoopAnalysis& routine,
        u16 headerAddress)
    {
        const auto iterator =
            std::find_if(
                routine.loops.begin(),
                routine.loops.end(),
                [headerAddress](
                    const NaturalLoop& loop)
                {
                    return
                        loop.headerAddress ==
                        headerAddress;
                });

        if (iterator ==
            routine.loops.end())
        {
            return nullptr;
        }

        return &*iterator;
    }

    [[nodiscard]]
    static std::vector<u16>
    BuildNaturalLoopMembers(
        const RoutineControlFlowGraph& graph,
        const RoutineDominatorAnalysis& dominators,
        u16 latchAddress,
        u16 headerAddress)
    {
        std::vector<u16> members;

        std::array<bool, MemorySize>
            visited{};

        std::deque<u16>
            workList;

        //
        // Every natural loop contains its header.
        //
        visited[headerAddress] = true;

        members.push_back(
            headerAddress);

        //
        // Self-loop:
        //
        //     header -> header
        //
        // The loop consists of this block alone.
        //
        if (latchAddress ==
            headerAddress)
        {
            return members;
        }

        visited[latchAddress] = true;

        members.push_back(
            latchAddress);

        workList.push_back(
            latchAddress);

        //
        // Classical natural-loop algorithm:
        //
        // walk backwards through predecessors from the
        // latch until the header is reached.
        //
        while (!workList.empty())
        {
            const u16 address =
                workList.front();

            workList.pop_front();

            const auto* node =
                graph.FindNode(
                    address);

            if (node == nullptr)
            {
                continue;
            }

            for (const u16 predecessor :
                 node->predecessors)
            {
                if (visited[predecessor])
                {
                    continue;
                }

                //
                // A natural-loop node must be dominated
                // by its header.
                //
                // This extra check also protects us from
                // malformed or irreducible CFG input.
                //
                if (!dominators.Dominates(
                        headerAddress,
                        predecessor))
                {
                    continue;
                }

                visited[predecessor] = true;

                members.push_back(
                    predecessor);

                //
                // Header is included, but we do not walk
                // backwards beyond it.
                //
                if (predecessor !=
                    headerAddress)
                {
                    workList.push_back(
                        predecessor);
                }
            }
        }

        SortUniqueAddresses(
            members);

        return members;
    }

    static void BuildLoopExits(
        const RoutineControlFlowGraph& graph,
        NaturalLoop& loop)
    {
        loop.exits.clear();

        for (const auto& edge :
             graph.edges)
        {
            if (!loop.Contains(
                    edge.sourceAddress))
            {
                continue;
            }

            if (loop.Contains(
                    edge.targetAddress))
            {
                continue;
            }

            NaturalLoopExit exit;

            exit.sourceAddress =
                edge.sourceAddress;

            exit.targetAddress =
                edge.targetAddress;

            exit.type =
                edge.type;

            loop.exits.push_back(
                exit);
        }

        std::sort(
            loop.exits.begin(),
            loop.exits.end(),
            [](const NaturalLoopExit& left,
               const NaturalLoopExit& right)
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

        loop.exits.erase(
            std::unique(
                loop.exits.begin(),
                loop.exits.end(),
                [](const NaturalLoopExit& left,
                   const NaturalLoopExit& right)
                {
                    return
                        left.sourceAddress ==
                            right.sourceAddress &&
                        left.targetAddress ==
                            right.targetAddress &&
                        left.type ==
                            right.type;
                }),
            loop.exits.end());
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
};

} // namespace atari