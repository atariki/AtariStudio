#pragma once

#include <cstddef>
#include <vector>

#include <AtariStudio/Core/Project.h>
#include <AtariStudio/Core/Types.h>

#include <AtariStudio/Disassembler/BasicBlockAnalyzer.h>
#include <AtariStudio/Disassembler/BranchConditionAnalyzer.h>
#include <AtariStudio/Disassembler/CharacterSetAnalyzer.h>
#include <AtariStudio/Disassembler/CodeDataAnalyzer.h>
#include <AtariStudio/Disassembler/CodeIslandAnalyzer.h>
#include <AtariStudio/Disassembler/ConditionalRegionAnalyzer.h>
#include <AtariStudio/Disassembler/ControlFlowAnalyzer.h>
#include <AtariStudio/Disassembler/ControlFlowGraph.h>
#include <AtariStudio/Disassembler/DisassemblyListing.h>
#include <AtariStudio/Disassembler/DisassemblyMetadata.h>
#include <AtariStudio/Disassembler/DisplayListAnalyzer.h>
#include <AtariStudio/Disassembler/DominatorAnalyzer.h>
#include <AtariStudio/Disassembler/FlagProducerAnalyzer.h>
#include <AtariStudio/Disassembler/LoopConditionAnalyzer.h>
#include <AtariStudio/Disassembler/LoopNestingAnalyzer.h>
#include <AtariStudio/Disassembler/LoopStructureAnalyzer.h>
#include <AtariStudio/Disassembler/NaturalLoopAnalyzer.h>
#include <AtariStudio/Disassembler/PostDominatorAnalyzer.h>
#include <AtariStudio/Disassembler/RoutineAnalyzer.h>
#include <AtariStudio/Disassembler/ScreenMemoryAnalyzer.h>
#include <AtariStudio/Disassembler/ScreenPixelRenderer.h>
#include <AtariStudio/Disassembler/SemanticConditionAnalyzer.h>
#include <AtariStudio/Disassembler/StructuredAnalysisResult.h>
#include <AtariStudio/Disassembler/StructuredCodeGenerator.h>
#include <AtariStudio/Disassembler/StructuredTranslationUnitGenerator.h>
#include <AtariStudio/Disassembler/StructuredControlFlowAnalyzer.h>
#include <AtariStudio/Disassembler/StructuredExpressionBuilder.h>

namespace atari
{

struct AnalysisEngineResult
{
    //
    // ========================================================
    // Core control-flow analysis
    // ========================================================
    //

    ControlFlowAnalysisResult controlFlow;

    DisplayListAnalysisResult displayLists;

    CharacterSetAnalysisResult characterSets;

    ScreenMemoryAnalysisResult screens;

    ScreenPixelRenderResult renderedScreens;

    DisassemblyMetadata metadata;

    std::vector<CodeDataRegion> regions;

    RoutineAnalysisResult routines;

    BasicBlockAnalysisResult basicBlocks;

    ControlFlowGraphAnalysisResult graphs;

    //
    // ========================================================
    // Flag producer analysis
    // ========================================================
    //

    FlagProducerAnalysisResult flagProducers;

    //
    // ========================================================
    // Semantic condition analysis
    // ========================================================
    //

    SemanticConditionAnalysisResult semanticConditions;

    //
    // ========================================================
    // Dominator analysis
    // ========================================================
    //

    DominatorAnalysisResult dominators;

    PostDominatorAnalysisResult postDominators;

    //
    // ========================================================
    // Conditional reconstruction
    // ========================================================
    //

    ConditionalRegionAnalysisResult conditionals;

    BranchConditionAnalysisResult branchConditions;

    StructuredControlFlowAnalysisResult structuredControlFlow;

    //
    // ========================================================
    // Loop reconstruction
    // ========================================================
    //

    NaturalLoopAnalysisResult loops;

    LoopConditionAnalysisResult loopConditions;

    LoopNestingAnalysisResult loopNesting;

    LoopStructureAnalysisResult loopStructures;

    //
    // ========================================================
    // Structured expression analysis
    // ========================================================
    //

    StructuredAnalysisResult structured;

    //
    // ========================================================
    // Final listing
    // ========================================================
    //

    DisassemblyListing listing;

    //
    // ========================================================
    // Analysis counters
    // ========================================================
    //

    std::size_t cfgInstructionCount = 0;

    std::size_t codeIslandInstructionCount = 0;

    // --------------------------------------------------------
    // Instructions
    // --------------------------------------------------------

    [[nodiscard]]
    std::size_t TotalInstructionCount() const noexcept
    {
        return
            controlFlow
                .instructionAddresses
                .size();
    }

    [[nodiscard]]
    std::size_t DisplayListCount() const noexcept
    {
        return displayLists.displayLists.size();
    }

    [[nodiscard]]
    std::size_t DisplayListInstructionCount() const noexcept
    {
        return displayLists.InstructionCount();
    }

    [[nodiscard]]
    std::size_t CompleteDisplayListCount() const noexcept
    {
        return displayLists.CompleteCount();
    }

    [[nodiscard]]
    std::size_t CharacterSetCount() const noexcept
    {
        return characterSets.characterSets.size();
    }

    [[nodiscard]]
    std::size_t CompleteCharacterSetCount() const noexcept
    {
        return characterSets.CompleteCount();
    }

    [[nodiscard]]
    std::size_t CharacterGlyphCount() const noexcept
    {
        return characterSets.GlyphCount();
    }

    [[nodiscard]]
    std::size_t ScreenCount() const noexcept
    {
        return screens.screens.size();
    }

    [[nodiscard]]
    std::size_t CompleteScreenCount() const noexcept
    {
        return screens.CompleteCount();
    }

    [[nodiscard]]
    std::size_t ScreenRowCount() const noexcept
    {
        return screens.RowCount();
    }

    [[nodiscard]]
    std::size_t ScreenRenderCount() const noexcept
    {
        return renderedScreens.renders.size();
    }

    [[nodiscard]]
    std::size_t CompleteScreenRenderCount() const noexcept
    {
        return renderedScreens.CompleteCount();
    }

    [[nodiscard]]
    std::size_t RenderedPixelCount() const noexcept
    {
        return renderedScreens.PixelCount();
    }

    // --------------------------------------------------------
    // Metadata
    // --------------------------------------------------------

    [[nodiscard]]
    std::size_t CrossReferenceCount() const noexcept
    {
        return
            metadata
                .CrossReferences()
                .references
                .size();
    }

    [[nodiscard]]
    std::size_t SymbolCount() const noexcept
    {
        return
            metadata
                .Symbols()
                .Size();
    }

    // --------------------------------------------------------
    // Routines
    // --------------------------------------------------------

    [[nodiscard]]
    std::size_t RoutineCount() const noexcept
    {
        return
            routines
                .routines
                .size();
    }

    // --------------------------------------------------------
    // Basic blocks
    // --------------------------------------------------------

    [[nodiscard]]
    std::size_t BasicBlockCount() const noexcept
    {
        return
            basicBlocks
                .BlockCount();
    }

    // --------------------------------------------------------
    // CFG
    // --------------------------------------------------------

    [[nodiscard]]
    std::size_t GraphNodeCount() const noexcept
    {
        return
            graphs
                .NodeCount();
    }

    [[nodiscard]]
    std::size_t GraphEdgeCount() const noexcept
    {
        return
            graphs
                .EdgeCount();
    }

    // --------------------------------------------------------
    // Flag producers
    // --------------------------------------------------------

    [[nodiscard]]
    std::size_t FlagProducerBranchCount() const noexcept
    {
        return
            flagProducers
                .BranchCount();
    }

    [[nodiscard]]
    std::size_t FlagProducerFoundCount() const noexcept
    {
        return
            flagProducers
                .FoundCount();
    }

    [[nodiscard]]
    std::size_t FlagProducerUnresolvedCount() const noexcept
    {
        return
            flagProducers
                .UnresolvedCount();
    }

    // --------------------------------------------------------
    // Semantic conditions
    // --------------------------------------------------------

    [[nodiscard]]
    std::size_t SemanticConditionCount() const noexcept
    {
        return
            semanticConditions
                .ConditionCount();
    }

    [[nodiscard]]
    std::size_t SemanticConditionResolvedCount() const noexcept
    {
        return
            semanticConditions
                .ResolvedCount();
    }

    [[nodiscard]]
    std::size_t SemanticConditionUnresolvedCount() const noexcept
    {
        return
            semanticConditions
                .UnresolvedCount();
    }

    // --------------------------------------------------------
    // Dominators
    // --------------------------------------------------------

    [[nodiscard]]
    std::size_t DominatorNodeCount() const noexcept
    {
        return
            dominators
                .NodeCount();
    }

    [[nodiscard]]
    std::size_t BackEdgeCount() const noexcept
    {
        return
            dominators
                .BackEdgeCount();
    }

    // --------------------------------------------------------
    // Post-dominators
    // --------------------------------------------------------

    [[nodiscard]]
    std::size_t PostDominatorNodeCount() const noexcept
    {
        return
            postDominators
                .NodeCount();
    }

    [[nodiscard]]
    std::size_t PostDominatorTerminalCount() const noexcept
    {
        return
            postDominators
                .TerminalCount();
    }

    // --------------------------------------------------------
    // Conditional regions
    // --------------------------------------------------------

    [[nodiscard]]
    std::size_t ConditionalRegionCount() const noexcept
    {
        return
            conditionals
                .RegionCount();
    }

    [[nodiscard]]
    std::size_t IfThenCount() const noexcept
    {
        return
            conditionals
                .IfThenCount();
    }

    [[nodiscard]]
    std::size_t IfElseCount() const noexcept
    {
        return
            conditionals
                .IfElseCount();
    }

    // --------------------------------------------------------
    // Branch conditions
    // --------------------------------------------------------

    [[nodiscard]]
    std::size_t BranchConditionCount() const noexcept
    {
        return
            branchConditions
                .ConditionCount();
    }

    // --------------------------------------------------------
    // Structured IFs
    // --------------------------------------------------------

    [[nodiscard]]
    std::size_t StructuredIfCount() const noexcept
    {
        return
            structuredControlFlow
                .IfCount();
    }

    [[nodiscard]]
    std::size_t StructuredIfElseCount() const noexcept
    {
        return
            structuredControlFlow
                .IfElseCount();
    }

    [[nodiscard]]
    std::size_t StructuredRootCount() const noexcept
    {
        return
            structuredControlFlow
                .RootCount();
    }

    [[nodiscard]]
    std::size_t StructuredMaximumDepth() const noexcept
    {
        return
            structuredControlFlow
                .MaximumDepth();
    }

    // --------------------------------------------------------
    // Natural loops
    // --------------------------------------------------------

    [[nodiscard]]
    std::size_t NaturalLoopCount() const noexcept
    {
        return
            loops
                .LoopCount();
    }

    [[nodiscard]]
    std::size_t LoopExitCount() const noexcept
    {
        return
            loops
                .ExitCount();
    }

    // --------------------------------------------------------
    // Loop conditions
    // --------------------------------------------------------

    [[nodiscard]]
    std::size_t LoopConditionCount() const noexcept
    {
        return
            loopConditions
                .ConditionCount();
    }

    // --------------------------------------------------------
    // Loop nesting
    // --------------------------------------------------------

    [[nodiscard]]
    std::size_t LoopTreeNodeCount() const noexcept
    {
        return
            loopNesting
                .NodeCount();
    }

    [[nodiscard]]
    std::size_t RootLoopCount() const noexcept
    {
        return
            loopNesting
                .RootCount();
    }

    [[nodiscard]]
    std::size_t MaximumLoopDepth() const noexcept
    {
        return
            loopNesting
                .MaximumDepth();
    }

    // --------------------------------------------------------
    // Structured loops
    // --------------------------------------------------------

    [[nodiscard]]
    std::size_t StructuredLoopCount() const noexcept
    {
        return
            loopStructures
                .LoopCount();
    }

    [[nodiscard]]
    std::size_t StructuredWhileCount() const noexcept
    {
        return
            loopStructures
                .WhileCount();
    }

    [[nodiscard]]
    std::size_t StructuredDoWhileCount() const noexcept
    {
        return
            loopStructures
                .DoWhileCount();
    }

    [[nodiscard]]
    std::size_t StructuredInfiniteLoopCount() const noexcept
    {
        return
            loopStructures
                .InfiniteCount();
    }

    [[nodiscard]]
    std::size_t StructuredComplexLoopCount() const noexcept
    {
        return
            loopStructures
                .ComplexCount();
    }

    [[nodiscard]]
    std::size_t StructuredBreakConditionCount() const noexcept
    {
        return
            loopStructures
                .BreakConditionCount();
    }

    [[nodiscard]]
    std::size_t StructuredLoopMaximumDepth() const noexcept
    {
        return
            loopStructures
                .MaximumDepth();
    }

    // --------------------------------------------------------
    // Structured expressions
    // --------------------------------------------------------

    [[nodiscard]]
    std::size_t StructuredExpressionCount() const noexcept
    {
        return
            structured
                .ExpressionCount();
    }

    [[nodiscard]]
    std::size_t StructuredStatementCount() const noexcept
    {
        return
            structured
                .StatementCount();
    }

    // --------------------------------------------------------
    // Listing
    // --------------------------------------------------------

    [[nodiscard]]
    std::size_t ListingRowCount() const noexcept
    {
        return
            listing
                .RowCount();
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

        //
        // =====================================================
        // Phase 0
        //
        // Clear executable flags from previous analysis.
        // =====================================================
        //

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
        // =====================================================
        // Phase 1
        //
        // Control-flow analysis + relocation detection.
        // =====================================================
        //

        ControlFlowAnalyzer
            controlFlowAnalyzer;

        result.controlFlow =
            controlFlowAnalyzer.Analyze(
                project.GetMemory(),
                entryPoints);

        result.cfgInstructionCount =
            result.controlFlow
                .instructionAddresses
                .size();

        //
        // =====================================================
        // Phase 2
        //
        // Search for disconnected executable code islands.
        // =====================================================
        //

        CodeIslandAnalyzer
            codeIslandAnalyzer;

        codeIslandAnalyzer.Analyze(
            project,
            result.controlFlow);

        result.codeIslandInstructionCount =
            result.controlFlow
                .instructionAddresses
                .size() -
            result.cfgInstructionCount;

        //
        // =====================================================
        // Phase 3
        //
        // Build symbols, XREFs and relocation metadata.
        // =====================================================
        //

        result.metadata.Build(
            project,
            result.controlFlow);

        //
        // =====================================================
        // Phase 4
        //
        // CODE / DATA classification.
        // =====================================================
        //

        CodeDataAnalyzer
            codeDataAnalyzer;

        result.regions =
            codeDataAnalyzer.Analyze(
                project);

        //
        // =====================================================
        // Phase 5
        //
        // Routine detection.
        // =====================================================
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
        // =====================================================
        // Phase 6
        //
        // Basic-block analysis.
        // =====================================================
        //

        BasicBlockAnalyzer
            basicBlockAnalyzer;

        result.basicBlocks =
            basicBlockAnalyzer.Analyze(
                project,
                result.controlFlow,
                result.routines);

        //
        // =====================================================
        // Phase 7
        //
        // Build per-routine control-flow graphs.
        // =====================================================
        //

        ControlFlowGraphBuilder
            graphBuilder;

        result.graphs =
            graphBuilder.Build(
                result.basicBlocks);

        //
        // =====================================================
        // Phase 8
        //
        // Flag producer analysis.
        // =====================================================
        //

        FlagProducerAnalyzer
            flagProducerAnalyzer;

        result.flagProducers =
            flagProducerAnalyzer.Analyze(
                project,
                result.graphs);

        //
        // =====================================================
        // Phase 9
        //
        // Semantic condition reconstruction.
        //
        // Examples:
        //
        //     INX + BNE
        //         -> X != 0
        //
        //     CPY #$03 + BNE
        //         -> Y != $03
        //
        // =====================================================
        //

        SemanticConditionAnalyzer
            semanticConditionAnalyzer;

        result.semanticConditions =
            semanticConditionAnalyzer.Analyze(
                project,
                result.flagProducers);

        //
        // =====================================================
        // Phase 10
        //
        // Dominator analysis + back edges.
        // =====================================================
        //

        DominatorAnalyzer
            dominatorAnalyzer;

        result.dominators =
            dominatorAnalyzer.Analyze(
                result.graphs);

        //
        // =====================================================
        // Phase 11
        //
        // Post-dominator analysis.
        // =====================================================
        //

        PostDominatorAnalyzer
            postDominatorAnalyzer;

        result.postDominators =
            postDominatorAnalyzer.Analyze(
                result.graphs);

        //
        // =====================================================
        // Phase 12
        //
        // Detect structured conditional regions.
        // =====================================================
        //

        ConditionalRegionAnalyzer
            conditionalRegionAnalyzer;

        result.conditionals =
            conditionalRegionAnalyzer.Analyze(
                result.graphs,
                result.postDominators);

        //
        // =====================================================
        // Phase 13
        //
        // Decode branch conditions.
        // =====================================================
        //

        BranchConditionAnalyzer
            branchConditionAnalyzer;

        result.branchConditions =
            branchConditionAnalyzer.Analyze(
                project,
                result.graphs,
                result.conditionals);

        //
        // =====================================================
        // Phase 14
        //
        // Reconstruct high-level IF structures.
        // =====================================================
        //

        StructuredControlFlowAnalyzer
            structuredControlFlowAnalyzer;

        result.structuredControlFlow =
            structuredControlFlowAnalyzer.Analyze(
                result.conditionals,
                result.branchConditions);

        //
        // =====================================================
        // Phase 15
        //
        // Natural-loop detection.
        // =====================================================
        //

        NaturalLoopAnalyzer
            naturalLoopAnalyzer;

        result.loops =
            naturalLoopAnalyzer.Analyze(
                result.graphs,
                result.dominators);

        //
        // =====================================================
        // Phase 16
        //
        // Loop-condition analysis.
        // =====================================================
        //

        LoopConditionAnalyzer
            loopConditionAnalyzer;

        result.loopConditions =
            loopConditionAnalyzer.Analyze(
                project,
                result.graphs,
                result.loops);

        //
        // =====================================================
        // Phase 17
        //
        // Loop nesting hierarchy.
        // =====================================================
        //

        LoopNestingAnalyzer
            loopNestingAnalyzer;

        result.loopNesting =
            loopNestingAnalyzer.Analyze(
                result.loops);

        //
        // =====================================================
        // Phase 18
        //
        // High-level loop reconstruction.
        // =====================================================
        //

        LoopStructureAnalyzer
            loopStructureAnalyzer;

        result.loopStructures =
            loopStructureAnalyzer.Analyze(
                result.loops,
                result.loopConditions,
                result.loopNesting);

        //
        // =====================================================
        // Phase 19
        //
        // Build high-level expression tree.
        //
        // Combines:
        //
        //     structured IFs
        //     semantic branch conditions
        //     structured loops
        //
        // =====================================================
        //

        StructuredExpressionBuilder
            structuredExpressionBuilder;

        result.structured.expressions =
            structuredExpressionBuilder.Build(
                project,
                result.basicBlocks,
                result.metadata,
                result.structuredControlFlow,
                result.semanticConditions,
                result.loopStructures);

        //
        // =====================================================
        // Phase 20
        //
        // Generate pseudo-C from the structured tree.
        // =====================================================
        //

        StructuredCodeGenerator
            structuredCodeGenerator;

        result.structured.generatedCode =
            structuredCodeGenerator.Generate(
                result.structured.expressions);

        result.structured.generatedTranslationUnit =
            StructuredTranslationUnitGenerator{}
                .Generate(
                    project,
                    result.structured.expressions);

        //
        // =====================================================
        // Phase 21
        //
        // Final disassembly listing.
        // =====================================================
        //

        result.listing.Build(
            project,
            result.controlFlow,
            result.metadata,
            result.regions);

        //
        // =====================================================
        // Phase 22
        //
        // Discover and decode ANTIC display lists from the OS
        // shadow pointer and the hardware DLIST pointer.
        // This analysis is read-only and preserves XEX segment
        // metadata for repeatable analysis.
        // =====================================================
        //

        DisplayListAnalyzer
            displayListAnalyzer;

        result.displayLists =
            displayListAnalyzer.Analyze(
                project.GetMemory());

        //
        // =====================================================
        // Phase 23
        //
        // Decode character sets referenced through CHBAS or
        // CHBASE. Display-list text modes determine whether the
        // hardware uses a 64- or 128-character layout.
        // =====================================================
        //

        CharacterSetAnalyzer
            characterSetAnalyzer;

        result.characterSets =
            characterSetAnalyzer.Analyze(
                project.GetMemory(),
                result.displayLists);

        //
        // =====================================================
        // Phase 24
        //
        // Reconstruct screen-memory rows from LMS addresses,
        // ANTIC modes, DMACTL width, and horizontal scrolling.
        // =====================================================
        //

        ScreenMemoryAnalyzer
            screenMemoryAnalyzer;

        result.screens =
            screenMemoryAnalyzer.Analyze(
                project.GetMemory(),
                result.displayLists);

        //
        // =====================================================
        // Phase 25
        //
        // Decode reconstructed screen bytes and character glyphs
        // into palette-independent ANTIC pixel indices. Character
        // color modifiers remain separate because DLI-driven color
        // register changes are outside static memory analysis.
        // =====================================================
        //

        ScreenPixelRenderer
            screenPixelRenderer;

        result.renderedScreens =
            screenPixelRenderer.Render(
                result.screens,
                result.characterSets);

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
