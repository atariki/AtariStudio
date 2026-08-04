#include <AtariStudio/Core/Memory.h>
#include <AtariStudio/Core/Project.h>
#include <AtariStudio/Disassembler/AnalysisEngine.h>
#include <AtariStudio/Disassembler/DisplayListAnalyzer.h>

#include <iostream>
#include <memory>
#include <vector>

namespace
{

bool Expect(
    bool condition,
    const char* message)
{
    if (!condition)
    {
        std::cerr
            << "FAILED: "
            << message
            << '\n';
    }

    return condition;
}

void WriteCompleteDisplayList(
    atari::Memory& memory)
{
    memory.Write8(0x2000, 0x70);

    memory.Write8(0x2001, 0xF2);
    memory.Write16(0x2002, 0x4000);

    memory.Write8(0x2004, 0x03);

    memory.Write8(0x2005, 0xC1);
    memory.Write16(0x2006, 0x2000);
}

} // namespace

int main()
{
    bool passed = true;

    auto memory =
        std::make_unique<atari::Memory>();

    auto& memoryImage = *memory;

    WriteCompleteDisplayList(memoryImage);

    memoryImage.Write16(
        atari::DisplayListAnalyzer::
            OsDisplayListPointerAddress,
        0x2000);

    memoryImage.Write16(
        atari::DisplayListAnalyzer::
            AnticDisplayListPointerAddress,
        0x2000);

    const auto discovered =
        atari::DisplayListAnalyzer::
            DiscoverEntryPoints(memoryImage);

    passed &=
        Expect(
            discovered ==
                std::vector<atari::u16>{0x2000},
            "OS and ANTIC pointers must be discovered and deduplicated");

    atari::DisplayListAnalyzer analyzer;

    const auto analysis =
        analyzer.Analyze(memoryImage);

    passed &=
        Expect(
            analysis.entryPoints == discovered &&
            analysis.displayLists.size() == 1 &&
            analysis.InstructionCount() == 4 &&
            analysis.CompleteCount() == 1 &&
            analysis.screenMemoryAddresses ==
                std::vector<atari::u16>{0x4000},
            "complete display-list summary");

    if (analysis.displayLists.size() != 1 ||
        analysis.displayLists.front().instructions.size() != 4)
    {
        return 1;
    }

    const auto& displayList =
        analysis.displayLists.front();

    passed &=
        Expect(
            displayList.entryPoint == 0x2000 &&
            displayList.ByteCount() == 8 &&
            displayList.Complete() &&
            displayList.stopAddress == 0x2000,
            "JVB must terminate a complete display list");

    const auto& blank =
        displayList.instructions[0];

    passed &=
        Expect(
            blank.kind ==
                atari::DisplayListInstructionKind::Blank &&
            blank.blankScanLines == 8 &&
            blank.length == 1 &&
            !blank.horizontalScroll &&
            !blank.verticalScroll &&
            !blank.loadMemoryScan,
            "blank-line instruction decoding");

    const auto& mode =
        displayList.instructions[1];

    passed &=
        Expect(
            mode.kind ==
                atari::DisplayListInstructionKind::ModeLine &&
            mode.mode == 2 &&
            mode.length == 3 &&
            mode.displayListInterrupt &&
            mode.horizontalScroll &&
            mode.verticalScroll &&
            mode.loadMemoryScan &&
            mode.memoryScanAddress == 0x4000,
            "mode-line flags and LMS operand decoding");

    const auto& jvb =
        displayList.instructions.back();

    passed &=
        Expect(
            jvb.kind ==
                atari::DisplayListInstructionKind::
                    JumpAndWaitForVerticalBlank &&
            jvb.displayListInterrupt &&
            jvb.jumpAddress == 0x2000,
            "JVB target and DLI decoding");

    memoryImage.Write8(0x3000, 0x01);
    memoryImage.Write16(0x3001, 0x3010);
    memoryImage.Write8(0x3010, 0x10);
    memoryImage.Write8(0x3011, 0x41);
    memoryImage.Write16(0x3012, 0x3000);

    const auto jumpAnalysis =
        analyzer.Analyze(
            memoryImage,
            {0x3000});

    passed &=
        Expect(
            jumpAnalysis.displayLists.size() == 1 &&
            jumpAnalysis.displayLists[0].instructions.size() == 3 &&
            jumpAnalysis.displayLists[0].instructions[0].kind ==
                atari::DisplayListInstructionKind::Jump &&
            jumpAnalysis.displayLists[0].instructions[1].address ==
                0x3010 &&
            jumpAnalysis.displayLists[0].Complete(),
            "JMP must continue decoding at its target");

    memoryImage.Write8(0x3200, 0x01);
    memoryImage.Write16(0x3201, 0x3200);

    const auto loopAnalysis =
        analyzer.Analyze(
            memoryImage,
            {0x3200});

    passed &=
        Expect(
            loopAnalysis.displayLists[0].instructions.size() == 1 &&
            loopAnalysis.displayLists[0].stopReason ==
                atari::DisplayListStopReason::LoopDetected &&
            loopAnalysis.displayLists[0].stopAddress == 0x3200,
            "plain JMP cycles must terminate safely");

    memoryImage.Write8(0x3300, 0x42);
    memoryImage.Write8(0x3301, 0x00);

    const auto truncatedAnalysis =
        analyzer.Analyze(
            memoryImage,
            {0x3300});

    passed &=
        Expect(
            truncatedAnalysis.displayLists[0].instructions.empty() &&
            truncatedAnalysis.displayLists[0].stopReason ==
                atari::DisplayListStopReason::
                    TruncatedInstruction &&
            truncatedAnalysis.displayLists[0].stopAddress == 0x3302,
            "missing LMS operand bytes must be reported");

    memoryImage.Write8(0x23FF, 0x00);
    memoryImage.Write8(0x2400, 0x00);

    const auto boundaryAnalysis =
        analyzer.Analyze(
            memoryImage,
            {0x23FF});

    passed &=
        Expect(
            boundaryAnalysis.displayLists[0].instructions.size() == 1 &&
            boundaryAnalysis.displayLists[0].stopReason ==
                atari::DisplayListStopReason::
                    OneKilobyteBoundary &&
            boundaryAnalysis.displayLists[0].stopAddress == 0x2400,
            "sequential decoding must not cross ANTIC's 1 KiB boundary");

    memoryImage.Write8(0xFFFF, 0x41);

    const auto addressSpaceAnalysis =
        analyzer.Analyze(
            memoryImage,
            {0xFFFF});

    passed &=
        Expect(
            addressSpaceAnalysis.displayLists[0].instructions.empty() &&
            addressSpaceAnalysis.displayLists[0].stopReason ==
                atari::DisplayListStopReason::
                    AddressSpaceBoundary,
            "multi-byte instructions must not wrap at $FFFF");

    memoryImage.Write8(0x3400, 0x00);
    memoryImage.Write8(0x3401, 0x00);
    memoryImage.Write8(0x3402, 0x00);

    const auto limitedAnalysis =
        analyzer.Analyze(
            memoryImage,
            {0x3400},
            2);

    passed &=
        Expect(
            limitedAnalysis.displayLists[0].instructions.size() == 2 &&
            limitedAnalysis.displayLists[0].stopReason ==
                atari::DisplayListStopReason::InstructionLimit &&
            limitedAnalysis.displayLists[0].stopAddress == 0x3402,
            "instruction limits must bound malformed lists");

    const auto explicitAnalysis =
        analyzer.Analyze(
            memoryImage,
            {0x3200, 0x2000, 0x3200});

    passed &=
        Expect(
            explicitAnalysis.entryPoints ==
                std::vector<atari::u16>{
                    0x2000,
                    0x3200},
            "explicit entry points must be sorted and deduplicated");

    auto project =
        std::make_unique<atari::Project>();

    WriteCompleteDisplayList(
        project->GetMemory());

    project->GetMemory().Write16(
        atari::DisplayListAnalyzer::
            OsDisplayListPointerAddress,
        0x2000);

    auto integrated =
        std::make_unique<
            atari::AnalysisEngineResult>(
                atari::AnalysisEngine{}.
                    Analyze(*project));

    passed &=
        Expect(
            integrated->DisplayListCount() == 1 &&
            integrated->CompleteDisplayListCount() == 1 &&
            integrated->DisplayListInstructionCount() == 4 &&
            project->Segments().empty(),
            "AnalysisEngine must expose display lists without mutating segments");

    return passed ? 0 : 1;
}
