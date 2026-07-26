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
#include <AtariStudio/Disassembler/FlagProducerAnalyzer.h>
#include <AtariStudio/Disassembler/LoopConditionAnalyzer.h>
#include <AtariStudio/Disassembler/LoopNestingAnalyzer.h>
#include <AtariStudio/Disassembler/LoopStructureAnalyzer.h>
#include <AtariStudio/Disassembler/NaturalLoopAnalyzer.h>
#include <AtariStudio/Disassembler/PostDominatorAnalyzer.h>
#include <AtariStudio/Disassembler/RoutineAnalyzer.h>
#include <AtariStudio/Disassembler/StructuredControlFlowAnalyzer.h>
#include <AtariStudio/Disassembler/SemanticConditionAnalyzer.h>

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
    // Converts processor flags into high-level expressions:
    //
    //     INX
    //     BNE label
    //
    // becomes:
    //
    //     X != 0
    //
    //     CPY #$03
    //     BNE label
    //
    // becomes:
    //
    //     Y != $03
    //
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


    //
    // --------------------------------------------------------
    // Instructions
    // --------------------------------------------------------
    //

    [[nodiscard]]
    std::size_t TotalInstructionCount() const noexcept
    {
        return
            controlFlow
                .instructionAddresses
                .size();
    }


    //
    // --------------------------------------------------------
    // Metadata
    // --------------------------------------------------------
    //

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


    //
    // --------------------------------------------------------
    // Routines
    // --------------------------------------------------------
    //

    [[nodiscard]]
    std::size_t RoutineCount() const noexcept
    {
        return
            routines
                .routines
                .size();
    }


    //
    // --------------------------------------------------------
    // Basic blocks
    // --------------------------------------------------------
    //

    [[nodiscard]]
    std::size_t BasicBlockCount() const noexcept
    {
        return
            basicBlocks
                .BlockCount();
    }


    //
    // --------------------------------------------------------
    // CFG
    // --------------------------------------------------------
    //

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


    //
    // --------------------------------------------------------
    // Flag producers
    // --------------------------------------------------------
    //

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


    //
    // --------------------------------------------------------
    // Semantic conditions
    // --------------------------------------------------------
    //

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


    //
    // --------------------------------------------------------
    // Dominators
    // --------------------------------------------------------
    //

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


    //
    // --------------------------------------------------------
    // Post dominators
    // --------------------------------------------------------
    //

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

        //
    // --------------------------------------------------------
    // Conditional regions
    // --------------------------------------------------------
    //

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


    //
    // --------------------------------------------------------
    // Branch conditions
    // --------------------------------------------------------
    //

    [[nodiscard]]
    std::size_t BranchConditionCount() const noexcept
    {
        return
            branchConditions
                .ConditionCount();
    }


    //
    // --------------------------------------------------------
    // Structured IF
    // --------------------------------------------------------
    //

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


    //
    // --------------------------------------------------------
    // Natural loops
    // --------------------------------------------------------
    //

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


    //
    // --------------------------------------------------------
    // Loop conditions
    // --------------------------------------------------------
    //

    [[nodiscard]]
    std::size_t LoopConditionCount() const noexcept
    {
        return
            loopConditions
                .ConditionCount();
    }


    //
    // --------------------------------------------------------
    // Loop nesting
    // --------------------------------------------------------
    //

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


    //
    // --------------------------------------------------------
    // Structured loops
    // --------------------------------------------------------
    //

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


    //
    // --------------------------------------------------------
    // Listing
    // --------------------------------------------------------
    //

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
                .size()
            -
            result.cfgInstructionCount;


            SemanticConditionAnalyzer semanticConditionAnalyzer;
result.semanticConditions =
    semanticConditionAnalyzer.Analyze(
        project,
        result.flagProducers);

        