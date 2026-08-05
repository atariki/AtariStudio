#include <AtariStudio/Core/Project.h>
#include <AtariStudio/Disassembler/AnalysisEngine.h>
#include <AtariStudio/Disassembler/ScreenPixelRenderer.h>

#include <algorithm>
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

atari::ScreenMemoryRow MakeRow(
    atari::u16 displayListAddress,
    atari::u8 mode,
    atari::u8 screenByte,
    std::size_t scanLineCount)
{
    atari::ScreenMemoryRow row;
    row.displayListInstructionAddress =
        displayListAddress;
    row.mode = mode;
    row.nominalScanLineCount = scanLineCount;
    row.screenAddress =
        static_cast<atari::u16>(
            0x5000 + displayListAddress);
    row.byteCount = 1;
    row.initializedByteCount = 1;
    row.bytes.push_back(screenByte);
    row.initialized.push_back(true);
    return row;
}

atari::CharacterSetAnalysis MakeCharacterSet(
    atari::CharacterSetLayout layout)
{
    atari::CharacterSetAnalysis result;
    result.baseAddress = 0x4000;
    result.layout = layout;
    result.expectedGlyphCount =
        atari::CharacterSetAnalyzer::
            GlyphCountForLayout(layout);
    result.initializedByteCount =
        result.expectedGlyphCount *
        atari::CharacterGlyph::RowCount;
    result.glyphs.resize(
        result.expectedGlyphCount);

    for (std::size_t index = 0;
         index < result.glyphs.size();
         ++index)
    {
        auto& glyph = result.glyphs[index];
        glyph.index = static_cast<atari::u8>(index);
        glyph.address = static_cast<atari::u16>(
            result.baseAddress + index * 8);
        glyph.initializedRowMask = 0xFF;
    }

    return result;
}

void FillRange(
    atari::Memory& memory,
    atari::u16 address,
    std::size_t size)
{
    for (std::size_t offset = 0;
         offset < size;
         ++offset)
    {
        memory.Write8(
            static_cast<atari::u16>(
                address + offset),
            static_cast<atari::u8>(offset));
    }
}

bool PixelValuesEqual(
    const atari::RenderedScreenScanLine& scanLine,
    const std::vector<atari::u8>& values,
    atari::u8 modifier)
{
    if (scanLine.pixels.size() != values.size())
    {
        return false;
    }

    for (std::size_t index = 0;
         index < values.size();
         ++index)
    {
        const auto& pixel = scanLine.pixels[index];

        if (pixel.value != values[index] ||
            pixel.characterModifier != modifier ||
            !pixel.initialized)
        {
            return false;
        }
    }

    return true;
}

} // namespace

int main()
{
    bool passed = true;

    passed &= Expect(
        atari::ScreenPixelRenderer::
            BitsPerPixelForMode(4) == 2 &&
        atari::ScreenPixelRenderer::
            BitsPerPixelForMode(15) == 1 &&
        atari::ScreenPixelRenderer::
            HorizontalPixelScaleForMode(8) == 8 &&
        atari::ScreenPixelRenderer::
            HorizontalPixelScaleForMode(10) == 4 &&
        atari::ScreenPixelRenderer::
            HorizontalPixelScaleForMode(6) == 2 &&
        atari::ScreenPixelRenderer::
            HorizontalPixelScaleForMode(2) == 1,
        "ANTIC mode pixel geometry");

    atari::ScreenMemoryAnalysis screen;
    screen.displayListEntryPoint = 0x2000;
    screen.playfieldWidth =
        atari::PlayfieldWidth::Normal;
    screen.displayListComplete = true;
    screen.rows = {
        MakeRow(0x2000, 8, 0x1B, 8),
        MakeRow(0x2001, 4, 0x81, 8),
        MakeRow(0x2002, 6, 0xC2, 8),
        MakeRow(0x2003, 3, 0xE0, 10),
        MakeRow(0x2004, 5, 0x81, 16)};
    screen.nominalScanLineCount = 50;
    screen.displayByteCount = 5;
    screen.initializedByteCount = 5;

    auto fullCharacterSet =
        MakeCharacterSet(
            atari::CharacterSetLayout::
                Characters128);
    fullCharacterSet.glyphs[1].rows[0] = 0x1B;
    fullCharacterSet.glyphs[96].rows[0] = 0x80;
    fullCharacterSet.glyphs[96].rows[7] = 0x01;

    auto compactCharacterSet =
        MakeCharacterSet(
            atari::CharacterSetLayout::
                Characters64);
    compactCharacterSet.glyphs[2].rows[0] = 0x80;

    atari::ScreenMemoryAnalysisResult screens;
    screens.screens.push_back(screen);

    atari::CharacterSetAnalysisResult characterSets;
    characterSets.characterSets = {
        fullCharacterSet,
        compactCharacterSet};

    const atari::ScreenPixelRenderer renderer;
    const auto rendered =
        renderer.Render(screens, characterSets);

    passed &= Expect(
        rendered.renders.size() == 1 &&
        rendered.CompleteCount() == 1 &&
        rendered.PixelCount() == 272,
        "mixed character and bitmap screen summary");

    if (rendered.renders.size() != 1 ||
        rendered.renders[0].modeLines.size() != 5)
    {
        return 1;
    }

    const auto& render = rendered.renders[0];
    const auto& mode8 = render.modeLines[0];
    const auto& mode4 = render.modeLines[1];
    const auto& mode6 = render.modeLines[2];
    const auto& mode3 = render.modeLines[3];
    const auto& mode5 = render.modeLines[4];

    passed &= Expect(
        mode8.scanLines.size() == 8 &&
        mode8.bitsPerPixel == 2 &&
        mode8.horizontalPixelScale == 8 &&
        PixelValuesEqual(
            mode8.scanLines[0],
            {0, 1, 2, 3},
            0) &&
        PixelValuesEqual(
            mode8.scanLines[7],
            {0, 1, 2, 3},
            0),
        "bitmap bytes must repeat across the mode line height");

    passed &= Expect(
        PixelValuesEqual(
            mode4.scanLines[0],
            {0, 1, 2, 3},
            1) &&
        mode4.bitsPerPixel == 2 &&
        mode4.horizontalPixelScale == 2,
        "mode 4 must preserve two-bit glyph pixels and bit-7 modifier");

    passed &= Expect(
        PixelValuesEqual(
            mode6.scanLines[0],
            {1, 0, 0, 0, 0, 0, 0, 0},
            3),
        "mode 6 must preserve glyph bits and the two-bit color modifier");

    passed &= Expect(
        PixelValuesEqual(
            mode3.scanLines[0],
            {0, 0, 0, 0, 0, 0, 0, 0},
            1) &&
        mode3.scanLines[2].pixels[0].value == 1 &&
        mode3.scanLines[9].pixels[7].value == 1,
        "mode 3 descenders must shift glyph rows down by two scan lines");

    passed &= Expect(
        PixelValuesEqual(
            mode5.scanLines[0],
            {0, 1, 2, 3},
            1) &&
        PixelValuesEqual(
            mode5.scanLines[1],
            {0, 1, 2, 3},
            1),
        "double-height character modes must repeat each glyph row");

    const auto unresolved =
        renderer.Render(
            screens,
            atari::CharacterSetAnalysisResult{});

    passed &= Expect(
        unresolved.renders.size() == 1 &&
        unresolved.CompleteCount() == 0 &&
        unresolved.renders[0].unresolvedModeLineCount == 4 &&
        !unresolved.renders[0].modeLines[1].
            characterSetResolved,
        "missing character sets must retain an incomplete render variant");

    auto overheightScreen = screen;
    overheightScreen.rows = {
        MakeRow(0x2010, 2, 0x01, 9)};
    overheightScreen.nominalScanLineCount = 9;
    overheightScreen.displayByteCount = 1;
    overheightScreen.initializedByteCount = 1;

    atari::ScreenMemoryAnalysisResult overheightScreens;
    overheightScreens.screens.push_back(
        overheightScreen);

    const auto overheight =
        renderer.Render(
            overheightScreens,
            characterSets);

    passed &= Expect(
        overheight.renders.size() == 1 &&
        overheight.renders[0].modeLines[0].
            scanLines.size() == 9 &&
        overheight.renders[0].modeLines[0].
            scanLines[8].InitializedPixelCount() == 0 &&
        !overheight.renders[0].Complete(),
        "externally supplied overheight character rows must stay bounded");

    auto project =
        std::make_unique<atari::Project>();
    auto& memory = project->GetMemory();

    memory.Write16(
        atari::DisplayListAnalyzer::
            OsDisplayListPointerAddress,
        0x2000);
    memory.Write8(
        atari::CharacterSetAnalyzer::
            OsCharacterBaseAddress,
        0x40);
    memory.Write8(
        atari::ScreenMemoryAnalyzer::
            OsDmaControlAddress,
        0x22);

    memory.Write8(0x2000, 0x44);
    memory.Write16(0x2001, 0x5000);
    memory.Write8(0x2003, 0x06);
    memory.Write8(0x2004, 0x41);
    memory.Write16(0x2005, 0x2000);

    FillRange(memory, 0x4000, 1024);
    FillRange(memory, 0x5000, 60);

    const auto integrated =
        std::make_unique<atari::AnalysisEngineResult>(
            atari::AnalysisEngine{}.
                Analyze(*project));

    passed &= Expect(
        integrated->ScreenRenderCount() == 1 &&
        integrated->CompleteScreenRenderCount() == 1 &&
        integrated->RenderedPixelCount() == 2560 &&
        integrated->renderedScreens.renders[0].
            modeLines.size() == 2,
        "AnalysisEngine must expose complete indexed-pixel renders");

    return passed ? 0 : 1;
}
