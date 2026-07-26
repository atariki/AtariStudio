#pragma once

#include <cstddef>
#include <vector>

#include <AtariStudio/Core/Project.h>
#include <AtariStudio/Core/Types.h>

#include <AtariStudio/Disassembler/BasicBlockAnalyzer.h>
#include <AtariStudio/Disassembler/BranchConditionAnalyzer.h>
#include <AtariStudio/Disassembler/CodeDataAnalyzer.h>
#include <AtariStudio/Disassembler/CodeIslandAnalyzer.h>
#include <AtariStudio/Disassembler/ConditionalRegionAnalyzer.h>
#include <AtariStudio/Disassembler/ControlFlowAnalyzer.h>
#include <AtariStudio/Disassembler/ControlFlowGraph.h>
#include <AtariStudio/Disassembler/DisassemblyListing.h>
#include <AtariStudio/Disassembler/DisassemblyMetadata.h>
#include <AtariStudio/Disassembler/DominatorAnalyzer.h>
#include <AtariStudio/Disassembler/LoopNestingAnalyzer.h>
#include <AtariStudio/Disassembler/NaturalLoopAnalyzer.h>
#include <AtariStudio/Disassembler/PostDominatorAnalyzer.h>
#include <AtariStudio/Disassembler/RoutineAnalyzer.h>

namespace atari
{

struct AnalysisEngineResult
{
    ControlFlowAnalysisResult controlFlow;

    DisassemblyMetadata metadata;

    std::vector<CodeDataRegion> regions;

    RoutineAnalysisResult routines;

    BasicBlockAnalysisResult basicBlocks;

    ControlFlowGraphAnalysisResult graphs;

    DominatorAnalysisResult dominators;

    PostDominatorAnalysisResult postDominators;

    ConditionalRegionAnalysisResult conditionals;

    BranchConditionAnalysisResult branchConditions;

    NaturalLoopAnalysisResult loops;

    LoopNestingAnalysisResult loopNesting;

    DisassemblyListing listing;

    std::size_t cfgInstructionCount = 0;

    std::size_t codeIslandInstructionCount = 0;

    [[nodiscard]]
    std::size_t TotalInstructionCount() const noexcept
    {
        return
            controlFlow.
                instructionAddresses.size();
    }

    [[nodiscard]]
    std::size_t CrossReferenceCount() const noexcept
    {
        return
            metadata.CrossReferences().
                references.size();
    }

    [[nodiscard]]
    std::size_t SymbolCount() const noexcept
    {
        return
            metadata.Symbols().Size();
    }

    [[nodiscard]]
    std::size_t RoutineCount() const noexcept
    {
        return
            routines.routines.size();
    }

    [[nodiscard]]
    std::size_t BasicBlockCount() const noexcept
    {
        return
            basicBlocks.BlockCount();
    }

    [[nodiscard]]
    std::size_t GraphNodeCount() const noexcept
    {
        return
            graphs.NodeCount();
    }

    [[nodiscard]]
    std::size_t GraphEdgeCount() const noexcept
    {
        return
            graphs.EdgeCount();
    }

    [[nodiscard]]
    std::size_t DominatorNodeCount() const noexcept
    {
        return
            dominators.NodeCount();
    }

    [[nodiscard]]
    std::size_t BackEdgeCount() const noexcept
    {
        return
            dominators.BackEdgeCount();
    }

    [[nodiscard]]
    std::size_t PostDominatorNodeCount() const noexcept
    {
        return
            postDominators.NodeCount();
    }

    [[nodiscard]]
    std::size_t PostDominatorTerminalCount() const noexcept
    {
        return
            postDominators.TerminalCount();
    }

    [[nodiscard]]
    std::size_t ConditionalRegionCount() const noexcept
    {
        return
            conditionals.RegionCount();
    }

    [[nodiscard]]
    std::size_t IfThenCount() const noexcept
    {
        return
            conditionals.IfThenCount();
    }

    [[nodiscard]]
    std::size_t IfElseCount() const noexcept
    {
        return
            conditionals.IfElseCount();
    }

    [[nodiscard]]
    std::size_t BranchConditionCount() const noexcept
    {
        return
            branchConditions.ConditionCount();
    }

    [[nodiscard]]
    std::size_t NaturalLoopCount() const noexcept
    {
        return
            loops.LoopCount();
    }

    [[nodiscard]]
    std::size_t LoopExitCount() const noexcept
    {
        return
            loops.ExitCount();
    }

    [[nodiscard]]
    std::size_t LoopTreeNodeCount() const noexcept
    {
        return
            loopNesting.NodeCount();
    }

    [[nodiscard]]
    std::size_t RootLoopCount() const noexcept
    {
        return
            loopNesting.RootCount();
    }

    [[nodiscard]]
    std::size_t MaximumLoopDepth() const noexcept
    {
        return
            loopNesting.MaximumDepth();
    }

    [[nodiscard]]
    std::size_t ListingRowCount() const noexcept
    {
        return
            listing.RowCount();
    }
};

class AnalysisEngine
{
public:

    [[nodiscard]]
    AnalysisEngineResult Analyze(
        Project& project) const
    {
        AnalysisEngineResult result;

        ResetAnalysisState(
            project);

        std::vector<u16> entryPoints;

        if (project.RunAddress() != 0)
        {
            entryPoints.push_back(
                project.RunAddress());
        }

        if (project.InitAddress() != 0 &&
            project.InitAddress() !=
                project.RunAddress())
        {
            entryPoints.push_back(
                project.InitAddress());
        }

        //
        // Phase 1:
        // Control flow + relocation.
        //
        ControlFlowAnalyzer
            controlFlowAnalyzer;

        result.controlFlow =
            controlFlowAnalyzer.Analyze(
                project.GetMemory(),
                entryPoints);

        result.cfgInstructionCount =
            result.controlFlow.
                instructionAddresses.size();

        //
        // Phase 2:
        // Disconnected code islands.
        //
        CodeIslandAnalyzer
            codeIslandAnalyzer;

        codeIslandAnalyzer.Analyze(
            project,
            result.controlFlow);

        result.codeIslandInstructionCount =
            result.controlFlow.
                instructionAddresses.size() -
            result.cfgInstructionCount;

        //
        // Phase 3:
        // Symbols + XREF.
        //
        result.metadata.Build(
            project,
            result.controlFlow);

        //
        // Phase 4:
        // CODE / DATA.
        //
        CodeDataAnalyzer
            codeDataAnalyzer;

        result.regions =
            codeDataAnalyzer.Analyze(
                project);

        //
        // Phase 5:
        // Routines.
        //
        RoutineAnalyzer
            routineAnalyzer;

        result.routines =
            routineAnalyzer.Analyze(
                project,
                result.controlFlow,
                result.metadata,
                result.regions);

        //
        // Phase 6:
        // Basic blocks.
        //
        BasicBlockAnalyzer
            basicBlockAnalyzer;

        result.basicBlocks =
            basicBlockAnalyzer.Analyze(
                project,
                result.controlFlow,
                result.routines);

        //
        // Phase 7:
        // CFG.
        //
        ControlFlowGraphBuilder
            graphBuilder;

        result.graphs =
            graphBuilder.Build(
                result.basicBlocks);

        //
        // Phase 8:
        // Dominators.
        //
        DominatorAnalyzer
            dominatorAnalyzer;

        result.dominators =
            dominatorAnalyzer.Analyze(
                result.graphs);

        //
        // Phase 9:
        // Post-dominators.
        //
        PostDominatorAnalyzer
            postDominatorAnalyzer;

        result.postDominators =
            postDominatorAnalyzer.Analyze(
                result.graphs);

        //
        // Phase 10:
        // Conditional regions.
        //
        ConditionalRegionAnalyzer
            conditionalRegionAnalyzer;

        result.conditionals =
            conditionalRegionAnalyzer.Analyze(
                result.graphs,
                result.postDominators);

        //
        // Phase 11:
        // 6502 branch-condition semantics.
        //
        BranchConditionAnalyzer
            branchConditionAnalyzer;

        result.branchConditions =
            branchConditionAnalyzer.Analyze(
                project,
                result.graphs,
                result.conditionals);

        //
        // Phase 12:
        // Natural loops.
        //
        NaturalLoopAnalyzer
            naturalLoopAnalyzer;

        result.loops =
            naturalLoopAnalyzer.Analyze(
                result.graphs,
                result.dominators);

        //
        // Phase 13:
        // Loop nesting.
        //
        LoopNestingAnalyzer
            loopNestingAnalyzer;

        result.loopNesting =
            loopNestingAnalyzer.Analyze(
                result.loops);

        //
        // Phase 14:
        // Listing.
        //
        result.listing.Build(
            project,
            result.controlFlow,
            result.metadata,
            result.regions);

        return result;
    }

private:

    static void ResetAnalysisState(
        Project& project)
    {
        auto& memory =
            project.GetMemory();

        for (std::size_t address = 0;
             address < MemorySize;
             ++address)
        {
            memory.Cell(
                static_cast<u16>(
                    address))
                .executable = false;
        }
    }
};

} // namespace atari