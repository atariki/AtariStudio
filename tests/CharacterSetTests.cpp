#include <AtariStudio/Core/Memory.h>
#include <AtariStudio/Core/Project.h>
#include <AtariStudio/Disassembler/AnalysisEngine.h>
#include <AtariStudio/Disassembler/CharacterSetAnalyzer.h>

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

void FillCharacterSet(
    atari::Memory& memory,
    atari::u16 baseAddress)
{
    for (std::uint32_t offset = 0;
         offset < 1024;
         ++offset)
    {
        memory.Write8(
            static_cast<atari::u16>(
                static_cast<std::uint32_t>(
                    baseAddress) +
                offset),
            static_cast<atari::u8>(offset));
    }
}

atari::DisplayListAnalysisResult
MakeMixedTextDisplayList()
{
    atari::DisplayListInstruction mode2;
    mode2.kind =
        atari::DisplayListInstructionKind::ModeLine;
    mode2.mode = 2;

    atari::DisplayListInstruction mode6;
    mode6.kind =
        atari::DisplayListInstructionKind::ModeLine;
    mode6.mode = 6;

    atari::DisplayListAnalysis displayList;
    displayList.instructions =
        {mode2, mode6};

    atari::DisplayListAnalysisResult result;
    result.displayLists.push_back(
        displayList);

    return result;
}

void WriteMixedTextDisplayList(
    atari::Memory& memory)
{
    memory.Write8(0x2000, 0x02);
    memory.Write8(0x2001, 0x06);
    memory.Write8(0x2002, 0x41);
    memory.Write16(0x2003, 0x2000);
}

} // namespace

int main()
{
    bool passed = true;

    passed &=
        Expect(
            atari::CharacterSetAnalyzer::
                GlyphCountForLayout(
                    atari::CharacterSetLayout::
                        Characters64) == 64 &&
            atari::CharacterSetAnalyzer::
                GlyphCountForLayout(
                    atari::CharacterSetLayout::
                        Characters128) == 128 &&
            atari::CharacterSetAnalyzer::
                BaseAddressFromChbase(
                    0x43,
                    atari::CharacterSetLayout::
                        Characters64) == 0x4200 &&
            atari::CharacterSetAnalyzer::
                BaseAddressFromChbase(
                    0x43,
                    atari::CharacterSetLayout::
                        Characters128) == 0x4000,
            "layout sizes and CHBASE alignment");

    auto memory =
        std::make_unique<atari::Memory>();

    auto& memoryImage = *memory;

    memoryImage.Write8(
        atari::CharacterSetAnalyzer::
            OsCharacterBaseAddress,
        0x42);

    memoryImage.Write8(
        atari::CharacterSetAnalyzer::
            AnticCharacterBaseAddress,
        0x42);

    FillCharacterSet(
        memoryImage,
        0x4000);

    memoryImage.Write8(0x4008, 0x81);

    const auto displayLists =
        MakeMixedTextDisplayList();

    const auto requests =
        atari::CharacterSetAnalyzer::
            DiscoverRequests(
                memoryImage,
                displayLists);

    passed &=
        Expect(
            requests.size() == 2 &&
            requests[0].baseAddress == 0x4000 &&
            requests[0].layout ==
                atari::CharacterSetLayout::
                    Characters128 &&
            requests[1].baseAddress == 0x4200 &&
            requests[1].layout ==
                atari::CharacterSetLayout::
                    Characters64,
            "display-list modes must select both charset layouts");

    atari::CharacterSetAnalyzer analyzer;

    const auto analysis =
        analyzer.Analyze(
            memoryImage,
            displayLists);

    passed &=
        Expect(
            analysis.requests.size() == 2 &&
            analysis.characterSets.size() == 2 &&
            analysis.CompleteCount() == 2 &&
            analysis.GlyphCount() == 192,
            "complete mixed-layout charset summary");

    if (analysis.characterSets.size() != 2 ||
        analysis.characterSets[0].glyphs.size() != 128)
    {
        return 1;
    }

    const auto& fullCharacterSet =
        analysis.characterSets[0];

    const auto& glyph =
        fullCharacterSet.glyphs[1];

    passed &=
        Expect(
            fullCharacterSet.baseAddress == 0x4000 &&
            fullCharacterSet.ExpectedByteCount() == 1024 &&
            fullCharacterSet.initializedByteCount == 1024 &&
            fullCharacterSet.Complete() &&
            glyph.index == 1 &&
            glyph.address == 0x4008 &&
            glyph.rows[0] == 0x81 &&
            glyph.RowInitialized(0) &&
            glyph.Complete(),
            "glyph addresses, row bytes, and initialization state");

    passed &=
        Expect(
            glyph.Bit(0, 0) &&
            glyph.Bit(0, 7) &&
            !glyph.Bit(0, 1) &&
            !glyph.Bit(8, 0) &&
            !glyph.Bit(0, 8) &&
            glyph.TwoBitPixel(0, 0) == 2 &&
            glyph.TwoBitPixel(0, 1) == 0 &&
            glyph.TwoBitPixel(0, 3) == 1 &&
            glyph.TwoBitPixel(0, 4) == 0 &&
            glyph.TwoBitPixel(8, 0) == 0 &&
            !glyph.RowInitialized(8),
            "glyph bit and two-bit pixel orientation and bounds");

    memoryImage.Write8(0x5000, 0xFF);

    const auto incomplete =
        analyzer.Analyze(
            memoryImage,
            {
                atari::CharacterSetRequest{
                    0x5000,
                    atari::CharacterSetLayout::
                        Characters128},
                atari::CharacterSetRequest{
                    0x5000,
                    atari::CharacterSetLayout::
                        Characters128}
            });

    passed &=
        Expect(
            incomplete.requests.size() == 1 &&
            incomplete.characterSets.size() == 1 &&
            incomplete.characterSets[0].glyphs.size() == 128 &&
            incomplete.characterSets[0].initializedByteCount == 1 &&
            !incomplete.characterSets[0].Complete() &&
            !incomplete.characterSets[0].addressSpaceTruncated,
            "explicit requests must deduplicate and retain incomplete sets");

    const auto truncated =
        analyzer.Analyze(
            memoryImage,
            {
                atari::CharacterSetRequest{
                    0xFFFF,
                    atari::CharacterSetLayout::
                        Characters128}
            });

    passed &=
        Expect(
            truncated.characterSets.size() == 1 &&
            truncated.characterSets[0].glyphs.size() == 1 &&
            truncated.characterSets[0].addressSpaceTruncated &&
            !truncated.characterSets[0].Complete(),
            "explicit charset ranges must not wrap past $FFFF");

    atari::DisplayListAnalysisResult graphicsOnly;
    atari::DisplayListAnalysis graphicsList;
    atari::DisplayListInstruction graphicsMode;
    graphicsMode.kind =
        atari::DisplayListInstructionKind::ModeLine;
    graphicsMode.mode = 8;
    graphicsList.instructions.push_back(graphicsMode);
    graphicsOnly.displayLists.push_back(graphicsList);

    passed &=
        Expect(
            atari::CharacterSetAnalyzer::
                DiscoverRequests(
                    memoryImage,
                    graphicsOnly).empty(),
            "bitmap-only display lists must not imply a charset");

    auto project =
        std::make_unique<atari::Project>();

    WriteMixedTextDisplayList(
        project->GetMemory());

    project->GetMemory().Write16(
        atari::DisplayListAnalyzer::
            OsDisplayListPointerAddress,
        0x2000);

    project->GetMemory().Write8(
        atari::CharacterSetAnalyzer::
            OsCharacterBaseAddress,
        0x42);

    FillCharacterSet(
        project->GetMemory(),
        0x4000);

    auto integrated =
        std::make_unique<
            atari::AnalysisEngineResult>(
                atari::AnalysisEngine{}.
                    Analyze(*project));

    passed &=
        Expect(
            integrated->DisplayListCount() == 1 &&
            integrated->CharacterSetCount() == 2 &&
            integrated->CompleteCharacterSetCount() == 2 &&
            integrated->CharacterGlyphCount() == 192 &&
            project->Segments().empty(),
            "AnalysisEngine must expose charsets without mutating segments");

    return passed ? 0 : 1;
}
