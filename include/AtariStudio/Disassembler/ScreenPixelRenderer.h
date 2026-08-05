#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

#include <AtariStudio/Core/Types.h>
#include <AtariStudio/Disassembler/CharacterSetAnalyzer.h>
#include <AtariStudio/Disassembler/ScreenMemoryAnalyzer.h>

namespace atari
{

struct IndexedScreenPixel
{
    u8 value = 0;
    u8 characterModifier = 0;
    bool initialized = false;
};

struct RenderedScreenScanLine
{
    std::size_t nominalScanLine = 0;

    std::vector<IndexedScreenPixel> pixels;

    [[nodiscard]]
    std::size_t InitializedPixelCount() const noexcept
    {
        return static_cast<std::size_t>(
            std::count_if(
                pixels.begin(),
                pixels.end(),
                [](const IndexedScreenPixel& pixel)
                {
                    return pixel.initialized;
                }));
    }

    [[nodiscard]]
    bool Complete() const noexcept
    {
        return
            !pixels.empty() &&
            InitializedPixelCount() == pixels.size();
    }
};

struct RenderedScreenModeLine
{
    u16 displayListInstructionAddress = 0;
    u8 mode = 0;

    std::size_t horizontalPixelScale = 1;
    std::size_t bitsPerPixel = 1;

    bool characterSetRequired = false;
    bool characterSetResolved = false;
    bool sourceRowResolved = false;

    std::vector<RenderedScreenScanLine>
        scanLines;

    [[nodiscard]]
    std::size_t PixelCount() const noexcept
    {
        std::size_t result = 0;

        for (const auto& scanLine : scanLines)
        {
            result += scanLine.pixels.size();
        }

        return result;
    }

    [[nodiscard]]
    std::size_t InitializedPixelCount() const noexcept
    {
        std::size_t result = 0;

        for (const auto& scanLine : scanLines)
        {
            result +=
                scanLine.InitializedPixelCount();
        }

        return result;
    }

    [[nodiscard]]
    bool Complete() const noexcept
    {
        return
            sourceRowResolved &&
            (!characterSetRequired ||
             characterSetResolved) &&
            !scanLines.empty() &&
            std::all_of(
                scanLines.begin(),
                scanLines.end(),
                [](const RenderedScreenScanLine&
                       scanLine)
                {
                    return scanLine.Complete();
                });
    }
};

struct ScreenPixelRender
{
    u16 displayListEntryPoint = 0;

    PlayfieldWidth playfieldWidth =
        PlayfieldWidth::Disabled;

    std::optional<u16> characterSet64Base;
    std::optional<u16> characterSet128Base;

    std::vector<RenderedScreenModeLine>
        modeLines;

    std::size_t unresolvedModeLineCount = 0;
    bool sourceScreenComplete = false;

    [[nodiscard]]
    std::size_t PixelCount() const noexcept
    {
        std::size_t result = 0;

        for (const auto& modeLine : modeLines)
        {
            result += modeLine.PixelCount();
        }

        return result;
    }

    [[nodiscard]]
    std::size_t InitializedPixelCount() const noexcept
    {
        std::size_t result = 0;

        for (const auto& modeLine : modeLines)
        {
            result +=
                modeLine.InitializedPixelCount();
        }

        return result;
    }

    [[nodiscard]]
    bool Complete() const noexcept
    {
        return
            sourceScreenComplete &&
            unresolvedModeLineCount == 0 &&
            !modeLines.empty() &&
            std::all_of(
                modeLines.begin(),
                modeLines.end(),
                [](const RenderedScreenModeLine&
                       modeLine)
                {
                    return modeLine.Complete();
                });
    }
};

struct ScreenPixelRenderResult
{
    std::vector<ScreenPixelRender> renders;

    [[nodiscard]]
    std::size_t CompleteCount() const noexcept
    {
        return static_cast<std::size_t>(
            std::count_if(
                renders.begin(),
                renders.end(),
                [](const ScreenPixelRender& render)
                {
                    return render.Complete();
                }));
    }

    [[nodiscard]]
    std::size_t PixelCount() const noexcept
    {
        std::size_t result = 0;

        for (const auto& render : renders)
        {
            result += render.PixelCount();
        }

        return result;
    }
};

class ScreenPixelRenderer
{
public:

    [[nodiscard]]
    static std::size_t BitsPerPixelForMode(
        u8 mode) noexcept
    {
        switch (mode)
        {
        case 4:
        case 5:
        case 8:
        case 10:
        case 13:
        case 14:
            return 2;

        default:
            return 1;
        }
    }

    [[nodiscard]]
    static std::size_t HorizontalPixelScaleForMode(
        u8 mode) noexcept
    {
        switch (mode)
        {
        case 2:
        case 3:
        case 15:
            return 1;

        case 4:
        case 5:
        case 6:
        case 7:
        case 11:
        case 12:
        case 13:
        case 14:
            return 2;

        case 9:
        case 10:
            return 4;

        case 8:
            return 8;

        default:
            return 1;
        }
    }

    [[nodiscard]]
    ScreenPixelRenderResult Render(
        const ScreenMemoryAnalysisResult& screens,
        const CharacterSetAnalysisResult&
            characterSets) const
    {
        ScreenPixelRenderResult result;

        for (const auto& screen : screens.screens)
        {
            const bool needs64 =
                RequiresLayout(
                    screen,
                    CharacterSetLayout::Characters64);

            const bool needs128 =
                RequiresLayout(
                    screen,
                    CharacterSetLayout::Characters128);

            auto candidates64 =
                FindCandidates(
                    characterSets,
                    CharacterSetLayout::Characters64,
                    needs64);

            auto candidates128 =
                FindCandidates(
                    characterSets,
                    CharacterSetLayout::Characters128,
                    needs128);

            for (const auto* characterSet64 :
                 candidates64)
            {
                for (const auto* characterSet128 :
                     candidates128)
                {
                    result.renders.push_back(
                        RenderOne(
                            screen,
                            characterSet64,
                            characterSet128));
                }
            }
        }

        return result;
    }

private:

    [[nodiscard]]
    static bool IsCharacterMode(u8 mode) noexcept
    {
        return mode >= 2 && mode <= 7;
    }

    [[nodiscard]]
    static bool RequiresLayout(
        const ScreenMemoryAnalysis& screen,
        CharacterSetLayout layout) noexcept
    {
        return std::any_of(
            screen.rows.begin(),
            screen.rows.end(),
            [layout](const ScreenMemoryRow& row)
            {
                if (layout ==
                    CharacterSetLayout::Characters64)
                {
                    return row.mode >= 6 && row.mode <= 7;
                }

                return row.mode >= 2 && row.mode <= 5;
            });
    }

    [[nodiscard]]
    static std::vector<const CharacterSetAnalysis*>
        FindCandidates(
            const CharacterSetAnalysisResult&
                characterSets,
            CharacterSetLayout layout,
            bool required)
    {
        std::vector<const CharacterSetAnalysis*> result;

        if (required)
        {
            for (const auto& characterSet :
                 characterSets.characterSets)
            {
                if (characterSet.layout == layout)
                {
                    result.push_back(&characterSet);
                }
            }
        }

        if (result.empty())
        {
            result.push_back(nullptr);
        }

        return result;
    }

    [[nodiscard]]
    static const CharacterGlyph* FindGlyph(
        const CharacterSetAnalysis* characterSet,
        u8 glyphIndex) noexcept
    {
        if (characterSet == nullptr ||
            glyphIndex >= characterSet->glyphs.size())
        {
            return nullptr;
        }

        return &characterSet->glyphs[glyphIndex];
    }

    [[nodiscard]]
    static u8 SourceByte(
        const ScreenMemoryRow& row,
        std::size_t index) noexcept
    {
        if (index >= row.bytes.size())
        {
            return 0;
        }

        return row.bytes[index];
    }

    [[nodiscard]]
    static bool SourceByteInitialized(
        const ScreenMemoryRow& row,
        std::size_t index) noexcept
    {
        return
            index < row.initialized.size() &&
            row.initialized[index];
    }

    static void AppendOneBitPixels(
        RenderedScreenScanLine& output,
        u8 byte,
        u8 characterModifier,
        bool initialized)
    {
        for (std::size_t column = 0;
             column < 8;
             ++column)
        {
            const auto shift =
                static_cast<unsigned>(7 - column);

            output.pixels.push_back(
                IndexedScreenPixel{
                    static_cast<u8>(
                        (byte >> shift) & 0x01),
                    characterModifier,
                    initialized});
        }
    }

    static void AppendTwoBitPixels(
        RenderedScreenScanLine& output,
        u8 byte,
        u8 characterModifier,
        bool initialized)
    {
        for (std::size_t column = 0;
             column < 4;
             ++column)
        {
            const auto shift =
                static_cast<unsigned>(
                    6 - column * 2);

            output.pixels.push_back(
                IndexedScreenPixel{
                    static_cast<u8>(
                        (byte >> shift) & 0x03),
                    characterModifier,
                    initialized});
        }
    }

    [[nodiscard]]
    static std::optional<std::size_t>
        CharacterGlyphRow(
            u8 mode,
            u8 glyphIndex,
            std::size_t scanLine) noexcept
    {
        if (mode == 3)
        {
            const bool descender =
                glyphIndex >= 96;

            if (descender)
            {
                if (scanLine < 2 || scanLine >= 10)
                {
                    return std::nullopt;
                }

                return scanLine - 2;
            }

            if (scanLine >= 8)
            {
                return std::nullopt;
            }

            return scanLine;
        }

        if (mode == 5 || mode == 7)
        {
            return scanLine / 2;
        }

        return scanLine;
    }

    static void DecodeBitmapScanLine(
        const ScreenMemoryRow& row,
        RenderedScreenScanLine& output)
    {
        const bool twoBit =
            BitsPerPixelForMode(row.mode) == 2;

        for (std::size_t index = 0;
             index < row.byteCount;
             ++index)
        {
            const u8 byte = SourceByte(row, index);
            const bool initialized =
                SourceByteInitialized(row, index);

            if (twoBit)
            {
                AppendTwoBitPixels(
                    output,
                    byte,
                    0,
                    initialized);
            }
            else
            {
                AppendOneBitPixels(
                    output,
                    byte,
                    0,
                    initialized);
            }
        }
    }

    static void DecodeCharacterScanLine(
        const ScreenMemoryRow& row,
        const CharacterSetAnalysis* characterSet,
        std::size_t scanLine,
        RenderedScreenScanLine& output)
    {
        const bool twoBit =
            row.mode == 4 || row.mode == 5;

        const u8 glyphMask =
            row.mode >= 6
                ? 0x3F
                : 0x7F;

        for (std::size_t index = 0;
             index < row.byteCount;
             ++index)
        {
            const u8 screenByte =
                SourceByte(row, index);

            const bool screenByteInitialized =
                SourceByteInitialized(row, index);

            const u8 glyphIndex =
                static_cast<u8>(
                    screenByte & glyphMask);

            const u8 characterModifier =
                row.mode >= 6
                    ? static_cast<u8>(
                          (screenByte >> 6) & 0x03)
                    : static_cast<u8>(
                          (screenByte >> 7) & 0x01);

            const auto glyphRow =
                CharacterGlyphRow(
                    row.mode,
                    glyphIndex,
                    scanLine);

            if (!glyphRow.has_value())
            {
                if (twoBit)
                {
                    AppendTwoBitPixels(
                        output,
                        0,
                        characterModifier,
                        screenByteInitialized);
                }
                else
                {
                    AppendOneBitPixels(
                        output,
                        0,
                        characterModifier,
                        screenByteInitialized);
                }

                continue;
            }

            const auto* glyph =
                FindGlyph(
                    characterSet,
                    glyphIndex);

            const bool glyphRowInRange =
                glyphRow.value() <
                CharacterGlyph::RowCount;

            const bool glyphInitialized =
                glyph != nullptr &&
                glyphRowInRange &&
                glyph->RowInitialized(
                    glyphRow.value());

            const u8 glyphByte =
                glyph != nullptr && glyphRowInRange
                    ? glyph->rows[glyphRow.value()]
                    : 0;

            const bool initialized =
                screenByteInitialized &&
                glyphInitialized;

            if (twoBit)
            {
                AppendTwoBitPixels(
                    output,
                    glyphByte,
                    characterModifier,
                    initialized);
            }
            else
            {
                AppendOneBitPixels(
                    output,
                    glyphByte,
                    characterModifier,
                    initialized);
            }
        }
    }

    [[nodiscard]]
    static RenderedScreenModeLine RenderModeLine(
        const ScreenMemoryRow& row,
        const CharacterSetAnalysis* characterSet64,
        const CharacterSetAnalysis* characterSet128)
    {
        RenderedScreenModeLine result;
        result.displayListInstructionAddress =
            row.displayListInstructionAddress;
        result.mode = row.mode;
        result.horizontalPixelScale =
            HorizontalPixelScaleForMode(row.mode);
        result.bitsPerPixel =
            BitsPerPixelForMode(row.mode);
        result.characterSetRequired =
            IsCharacterMode(row.mode);
        result.sourceRowResolved =
            row.AddressResolved();

        const CharacterSetAnalysis* characterSet =
            nullptr;

        if (row.mode >= 2 && row.mode <= 5)
        {
            characterSet = characterSet128;
        }
        else if (row.mode >= 6 && row.mode <= 7)
        {
            characterSet = characterSet64;
        }

        result.characterSetResolved =
            !result.characterSetRequired ||
            characterSet != nullptr;

        result.scanLines.reserve(
            row.nominalScanLineCount);

        for (std::size_t scanLine = 0;
             scanLine < row.nominalScanLineCount;
             ++scanLine)
        {
            RenderedScreenScanLine output;
            output.nominalScanLine =
                row.firstNominalScanLine + scanLine;

            if (result.characterSetRequired)
            {
                DecodeCharacterScanLine(
                    row,
                    characterSet,
                    scanLine,
                    output);
            }
            else
            {
                DecodeBitmapScanLine(
                    row,
                    output);
            }

            result.scanLines.push_back(
                std::move(output));
        }

        return result;
    }

    [[nodiscard]]
    static ScreenPixelRender RenderOne(
        const ScreenMemoryAnalysis& screen,
        const CharacterSetAnalysis* characterSet64,
        const CharacterSetAnalysis* characterSet128)
    {
        ScreenPixelRender result;
        result.displayListEntryPoint =
            screen.displayListEntryPoint;
        result.playfieldWidth =
            screen.playfieldWidth;
        result.sourceScreenComplete =
            screen.Complete();

        if (characterSet64 != nullptr)
        {
            result.characterSet64Base =
                characterSet64->baseAddress;
        }

        if (characterSet128 != nullptr)
        {
            result.characterSet128Base =
                characterSet128->baseAddress;
        }

        result.modeLines.reserve(
            screen.rows.size());

        for (const auto& row : screen.rows)
        {
            auto modeLine =
                RenderModeLine(
                    row,
                    characterSet64,
                    characterSet128);

            if (!modeLine.sourceRowResolved ||
                (modeLine.characterSetRequired &&
                 !modeLine.characterSetResolved))
            {
                ++result.unresolvedModeLineCount;
            }

            result.modeLines.push_back(
                std::move(modeLine));
        }

        return result;
    }
};

} // namespace atari
