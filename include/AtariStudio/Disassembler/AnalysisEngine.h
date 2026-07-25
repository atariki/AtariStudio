#pragma once

#include <cstddef>
#include <vector>

#include <AtariStudio/Core/Project.h>
#include <AtariStudio/Core/Types.h>
#include <AtariStudio/Disassembler/CodeDataAnalyzer.h>
#include <AtariStudio/Disassembler/CodeIslandAnalyzer.h>
#include <AtariStudio/Disassembler/ControlFlowAnalyzer.h>
#include <AtariStudio/Disassembler/DisassemblyMetadata.h>

namespace atari
{

struct AnalysisEngineResult
{
    //
    // Control-flow analysis, relocation,
    // discovered instruction addresses
    // and targets.
    //
    ControlFlowAnalysisResult controlFlow;

    //
    // Symbols, XREF, Atari comments
    // and relocation metadata.
    //
    DisassemblyMetadata metadata;

    //
    // Final CODE / DATA regions.
    //
    std::vector<CodeDataRegion> regions;

    //
    // Statistics.
    //
    std::size_t cfgInstructionCount = 0;

    std::size_t codeIslandInstructionCount = 0;

    [[nodiscard]]
    std::size_t TotalInstructionCount() const noexcept
    {
        return
            controlFlow.instructionAddresses.size();
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
        // Important:
        //
        // AnalysisEngine may be called more than once
        // for the same project.
        //
        // Remove executable flags left by a previous
        // analysis without destroying loaded memory.
        //
        ResetAnalysisState(
            project);

        //
        // Build initial entry points.
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
        //
        // CFG + relocation analysis.
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
        //
        // Detect disconnected code islands.
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
        //
        // Build symbols, XREF, Atari comments
        // and relocation metadata.
        //
        result.metadata.Build(
            project,
            result.controlFlow);

        //
        // Phase 4:
        //
        // Build final CODE / DATA regions.
        //
        CodeDataAnalyzer
            codeDataAnalyzer;

        result.regions =
            codeDataAnalyzer.Analyze(
                project);

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