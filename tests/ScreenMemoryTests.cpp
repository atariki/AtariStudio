#include <AtariStudio/Core/Memory.h>
#include <AtariStudio/Core/Project.h>
#include <AtariStudio/Disassembler/AnalysisEngine.h>
#include <AtariStudio/Disassembler/ScreenMemoryAnalyzer.h>

#include <cstdint>
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

void FillRange(
    atari::Memory& memory,
    atari::u16 begin,
    std::size_t size)
{
    for (std::size_t offset = 0;
         offset < size;
         ++offset)
    {
        const auto address =
            static_cast<atari::u16>(
                static_cast<std::uint32_t>(begin) +
                static_cast<std::uint32_t>(offset));

        memory.Write8(
            address,
            static_cast<atari::u8>(address));
    }
}

atari::DisplayListAnalysisResult
MakeScreenDisplayList()
{
    atari::DisplayListInstruction blank;
    blank.address = 0x2000;
    blank.kind =
        atari::DisplayListInstructionKind::Blank;
    blank.blankScanLines = 8;

    atari::DisplayListInstruction mode2;
    mode2.address = 0x2001;
    mode2.kind =
        atari::DisplayListInstructionKind::ModeLine;
    mode2.mode = 2;
    mode2.loadMemoryScan = true;
    mode2.memoryScanAddress =
        static_cast<atari::u16>(0x4FF0);

    atari::DisplayListInstruction mode6;
    mode6.address = 0x2004;
    mode6.kind =
        atari::DisplayListInstructionKind::ModeLine;
    mode6.mode = 6;
    mode6.horizontalScroll = true;

    atari::DisplayListInstruction mode15;
    mode15.address = 0x2005;
    mode15.kind =
        atari::DisplayListInstructionKind::ModeLine;
    mode15.mode = 15;
    mode15.loadMemoryScan = true;
    mode15.memoryScanAddress =
        static_cast<atari::u16>(0x6000);

    atari::DisplayListAnalysis displayList;
    displayList.entryPoint = 0x2000;
    displayList.instructions =
        {blank, mode2, mode6, mode15};
    displayList.stopReason =
        atari::DisplayListStopReason::
            JumpAndWaitForVerticalBlank;

    atari::DisplayListAnalysisResult result;
    result.entryPoints.push_back(0x2000);
    result.displayLists.push_back(displayList);

    return result;
}

void WriteIntegratedDisplayList(
    atari::Memory& memory)
{
    memory.Write8(0x2000, 0x70);

    memory.Write8(0x2001, 0x42);
    memory.Write16(0x2002, 0x4000);

    memory.Write8(0x2004, 0x16);

    memory.Write8(0x2005, 0x41);
    memory.Write16(0x2006, 0x2000);
}

} // namespace

int main()
{
    bool passed = true;

    passed &=
        Expect(
            atari::ScreenMemoryAnalyzer::
                NominalScanLinesForMode(2) == 8 &&
            atari::ScreenMemoryAnalyzer::
                NominalScanLinesForMode(3) == 10 &&
            atari::ScreenMemoryAnalyzer::
                NominalScanLinesForMode(5) == 16 &&
            atari::ScreenMemoryAnalyzer::
                NominalScanLinesForMode(9) == 4 &&
            atari::ScreenMemoryAnalyzer::
                NominalScanLinesForMode(11) == 2 &&
            atari::ScreenMemoryAnalyzer::
                NominalScanLinesForMode(15) == 1 &&
            atari::ScreenMemoryAnalyzer::
                NominalScanLinesForMode(1) == 0,
            "nominal ANTIC mode heights");

    passed &=
        Expect(
            atari::ScreenMemoryAnalyzer::
                BytesPerModeLine(
                    2,
                    atari::PlayfieldWidth::Narrow) == 32 &&
            atari::ScreenMemoryAnalyzer::
                BytesPerModeLine(
                    2,
                    atari::PlayfieldWidth::Normal) == 40 &&
            atari::ScreenMemoryAnalyzer::
                BytesPerModeLine(
                    2,
                    atari::PlayfieldWidth::Wide) == 48 &&
            atari::ScreenMemoryAnalyzer::
                BytesPerModeLine(
                    6,
                    atari::PlayfieldWidth::Normal,
                    true) == 24 &&
            atari::ScreenMemoryAnalyzer::
                BytesPerModeLine(
                    8,
                    atari::PlayfieldWidth::Normal) == 10 &&
            atari::ScreenMemoryAnalyzer::
                BytesPerModeLine(
                    10,
                    atari::PlayfieldWidth::Normal) == 20 &&
            atari::ScreenMemoryAnalyzer::
                BytesPerModeLine(
                    15,
                    atari::PlayfieldWidth::Normal) == 40 &&
            atari::ScreenMemoryAnalyzer::
                BytesPerModeLine(
                    2,
                    atari::PlayfieldWidth::Disabled) == 0,
            "mode-line byte counts and HSCROL fetch width");

    passed &=
        Expect(
            atari::ScreenMemoryAnalyzer::
                EffectiveFetchWidth(
                    atari::PlayfieldWidth::Narrow,
                    true) ==
                atari::PlayfieldWidth::Normal &&
            atari::ScreenMemoryAnalyzer::
                EffectiveFetchWidth(
                    atari::PlayfieldWidth::Normal,
                    true) ==
                atari::PlayfieldWidth::Wide &&
            atari::ScreenMemoryAnalyzer::
                EffectiveFetchWidth(
                    atari::PlayfieldWidth::Wide,
                    true) ==
                atari::PlayfieldWidth::Wide,
            "horizontal scrolling must increase fetch width once");

    auto memory =
        std::make_unique<atari::Memory>();

    auto& memoryImage = *memory;

    memoryImage.Write8(
        atari::ScreenMemoryAnalyzer::
            OsDmaControlAddress,
        0x22);

    memoryImage.Write8(
        atari::ScreenMemoryAnalyzer::
            AnticDmaControlAddress,
        0x23);

    const auto discoveredWidths =
        atari::ScreenMemoryAnalyzer::
            DiscoverPlayfieldWidths(
                memoryImage);

    passed &=
        Expect(
            discoveredWidths ==
                std::vector<atari::PlayfieldWidth>{
                    atari::PlayfieldWidth::Normal,
                    atari::PlayfieldWidth::Wide},
            "SDMCTL and DMACTL widths must be discovered and sorted");

    FillRange(
        memoryImage,
        0x4000,
        4096);

    FillRange(
        memoryImage,
        0x6000,
        40);

    atari::ScreenMemoryAnalyzer analyzer;

    const auto displayLists =
        MakeScreenDisplayList();

    const auto analysis =
        analyzer.Analyze(
            memoryImage,
            displayLists,
            {
                atari::PlayfieldWidth::Normal,
                atari::PlayfieldWidth::Disabled,
                atari::PlayfieldWidth::Normal
            });

    passed &=
        Expect(
            analysis.playfieldWidths ==
                std::vector<atari::PlayfieldWidth>{
                    atari::PlayfieldWidth::Normal} &&
            analysis.screens.size() == 1 &&
            analysis.CompleteCount() == 1 &&
            analysis.RowCount() == 3,
            "explicit widths must deduplicate and omit disabled DMA");

    if (analysis.screens.size() != 1 ||
        analysis.screens[0].rows.size() != 3)
    {
        return 1;
    }

    const auto& screen =
        analysis.screens[0];

    passed &=
        Expect(
            screen.displayListEntryPoint == 0x2000 &&
            screen.playfieldWidth ==
                atari::PlayfieldWidth::Normal &&
            screen.nominalScanLineCount == 25 &&
            screen.displayByteCount == 104 &&
            screen.initializedByteCount == 104 &&
            screen.memoryScanWrapCount == 1 &&
            screen.Complete(),
            "screen summary must include nominal geometry and DMA bytes");

    const auto& firstRow = screen.rows[0];
    const auto& secondRow = screen.rows[1];
    const auto& thirdRow = screen.rows[2];

    passed &=
        Expect(
            firstRow.displayListInstructionAddress == 0x2001 &&
            firstRow.firstNominalScanLine == 8 &&
            firstRow.nominalScanLineCount == 8 &&
            firstRow.screenAddress == 0x4FF0 &&
            firstRow.byteCount == 40 &&
            firstRow.bytes.size() == 40 &&
            firstRow.bytes[0] == 0xF0 &&
            firstRow.bytes[15] == 0xFF &&
            firstRow.bytes[16] == 0x00 &&
            firstRow.memoryScanWrapped &&
            firstRow.Complete(),
            "memory-scan counter must wrap inside its 4 KiB bank");

    passed &=
        Expect(
            secondRow.firstNominalScanLine == 16 &&
            secondRow.screenAddress == 0x4018 &&
            secondRow.byteCount == 24 &&
            secondRow.horizontalScroll &&
            !secondRow.memoryScanWrapped &&
            thirdRow.firstNominalScanLine == 24 &&
            thirdRow.nominalScanLineCount == 1 &&
            thirdRow.screenAddress == 0x6000 &&
            thirdRow.byteCount == 40 &&
            thirdRow.loadMemoryScan,
            "sequential memory scan, HSCROL, and later LMS");

    atari::DisplayListAnalysis unresolvedList;
    unresolvedList.entryPoint = 0x3000;
    unresolvedList.stopReason =
        atari::DisplayListStopReason::
            JumpAndWaitForVerticalBlank;

    atari::DisplayListInstruction unresolvedMode;
    unresolvedMode.address = 0x3000;
    unresolvedMode.kind =
        atari::DisplayListInstructionKind::ModeLine;
    unresolvedMode.mode = 2;
    unresolvedList.instructions.push_back(
        unresolvedMode);

    atari::DisplayListAnalysisResult unresolvedInput;
    unresolvedInput.displayLists.push_back(
        unresolvedList);

    const auto unresolved =
        analyzer.Analyze(
            memoryImage,
            unresolvedInput,
            {atari::PlayfieldWidth::Normal});

    passed &=
        Expect(
            unresolved.screens.size() == 1 &&
            unresolved.screens[0].rows.size() == 1 &&
            unresolved.screens[0].unresolvedRowCount == 1 &&
            !unresolved.screens[0].rows[0].AddressResolved() &&
            unresolved.screens[0].rows[0].bytes.empty() &&
            !unresolved.screens[0].Complete(),
            "mode lines before the first LMS must remain unresolved");

    auto project =
        std::make_unique<atari::Project>();

    WriteIntegratedDisplayList(
        project->GetMemory());

    project->GetMemory().Write16(
        atari::DisplayListAnalyzer::
            OsDisplayListPointerAddress,
        0x2000);

    project->GetMemory().Write8(
        atari::ScreenMemoryAnalyzer::
            OsDmaControlAddress,
        0x22);

    FillRange(
        project->GetMemory(),
        0x4000,
        64);

    auto integrated =
        std::make_unique<
            atari::AnalysisEngineResult>(
                atari::AnalysisEngine{}.
                    Analyze(*project));

    passed &=
        Expect(
            integrated->DisplayListCount() == 1 &&
            integrated->ScreenCount() == 1 &&
            integrated->CompleteScreenCount() == 1 &&
            integrated->ScreenRowCount() == 2 &&
            integrated->screens.screens[0].displayByteCount == 64 &&
            integrated->screens.screens[0].nominalScanLineCount == 24 &&
            project->Segments().empty(),
            "AnalysisEngine must expose screens without mutating segments");

    return passed ? 0 : 1;
}
