#pragma once

#include <cstddef>
#include <vector>

#include <AtariStudio/Core/Project.h>
#include <AtariStudio/Core/Types.h>
#include <AtariStudio/Disassembler/BasicBlockAnalyzer.h>
#include <AtariStudio/Disassembler/CodeDataAnalyzer.h>
#include <AtariStudio/Disassembler/CodeIslandAnalyzer.h>
#include <AtariStudio/Disassembler/ControlFlowAnalyzer.h>
#include <AtariStudio/Disassembler/ControlFlowGraph.h>
#include <AtariStudio/Disassembler/DisassemblyListing.h>
#include <AtariStudio/Disassembler/DisassemblyMetadata.h>
#include <AtariStudio/Disassembler/RoutineAnalyzer.h>

namespace atari
{

struct AnalysisEngineResult
{
    //
    // Control-flow analysis + relocation.
    //
    ControlFlowAnalysisResult controlFlow;

    //
    // Symbols, XREF, Atari symbols,
    // runtime / relocation comments.
    //
    DisassemblyMetadata metadata;

    //
    // CODE / DATA regions.
    //
    std::vector<CodeDataRegion> regions;

    //
    // Detected routines.
    //
    RoutineAnalysisResult routines;

    //
    // Basic blocks.
    //
    BasicBlockAnalysisResult basicBlocks;

    //
    // Final CFG model.
    //
    ControlFlowGraphAnalysisResult graphs;

    //
    // GUI-independent listing.
    //
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

        //
        // Initial entry points.
        //
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
        // CFG + relocation.
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
        // disconnected code islands.
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
        // symbols + XREF.
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
        // routines.
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
        // basic blocks.
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
        // CFG graph model.
        //
        ControlFlowGraphBuilder
            graphBuilder;

        result.graphs =
            graphBuilder.Build(
                result.basicBlocks);

        //
        // Phase 8:
        // listing.
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