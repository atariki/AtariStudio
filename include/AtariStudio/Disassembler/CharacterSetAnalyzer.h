#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <AtariStudio/Core/Memory.h>
#include <AtariStudio/Core/Types.h>
#include <AtariStudio/Disassembler/DisplayListAnalyzer.h>

namespace atari
{

enum class CharacterSetLayout
{
    Characters64,
    Characters128
};

struct CharacterSetRequest
{
    u16 baseAddress = 0;

    CharacterSetLayout layout =
        CharacterSetLayout::Characters128;
};

struct CharacterGlyph
{
    static constexpr std::size_t RowCount = 8;
    static constexpr std::size_t ColumnCount = 8;

    u8 index = 0;
    u16 address = 0;

    std::array<u8, RowCount> rows{};

    u8 initializedRowMask = 0;

    [[nodiscard]]
    bool Bit(
        std::size_t row,
        std::size_t column) const noexcept
    {
        if (row >= RowCount ||
            column >= ColumnCount)
        {
            return false;
        }

        const auto mask =
            static_cast<u8>(
                0x80U >> column);

        return (rows[row] & mask) != 0;
    }

    [[nodiscard]]
    u8 TwoBitPixel(
        std::size_t row,
        std::size_t column) const noexcept
    {
        if (row >= RowCount ||
            column >= 4)
        {
            return 0;
        }

        const auto shift =
            static_cast<unsigned>(
                6 - column * 2);

        return static_cast<u8>(
            (rows[row] >> shift) & 0x03);
    }

    [[nodiscard]]
    bool RowInitialized(
        std::size_t row) const noexcept
    {
        if (row >= RowCount)
        {
            return false;
        }

        return
            (initializedRowMask &
             static_cast<u8>(1U << row)) != 0;
    }

    [[nodiscard]]
    bool Complete() const noexcept
    {
        return initializedRowMask == 0xFF;
    }
};

struct CharacterSetAnalysis
{
    u16 baseAddress = 0;

    CharacterSetLayout layout =
        CharacterSetLayout::Characters128;

    std::size_t expectedGlyphCount = 0;

    std::vector<CharacterGlyph> glyphs;

    std::size_t initializedByteCount = 0;

    bool addressSpaceTruncated = false;

    [[nodiscard]]
    std::size_t ExpectedByteCount() const noexcept
    {
        return
            expectedGlyphCount *
            CharacterGlyph::RowCount;
    }

    [[nodiscard]]
    bool Complete() const noexcept
    {
        return
            !addressSpaceTruncated &&
            glyphs.size() == expectedGlyphCount &&
            initializedByteCount ==
                ExpectedByteCount();
    }
};

struct CharacterSetAnalysisResult
{
    std::vector<CharacterSetRequest> requests;

    std::vector<CharacterSetAnalysis>
        characterSets;

    [[nodiscard]]
    std::size_t CompleteCount() const noexcept
    {
        return static_cast<std::size_t>(
            std::count_if(
                characterSets.begin(),
                characterSets.end(),
                [](const CharacterSetAnalysis&
                       characterSet)
                {
                    return characterSet.Complete();
                }));
    }

    [[nodiscard]]
    std::size_t GlyphCount() const noexcept
    {
        std::size_t result = 0;

        for (const auto& characterSet :
             characterSets)
        {
            result += characterSet.glyphs.size();
        }

        return result;
    }
};

class CharacterSetAnalyzer
{
public:

    static constexpr u16
        OsCharacterBaseAddress = 0x02F4;

    static constexpr u16
        AnticCharacterBaseAddress = 0xD409;

    [[nodiscard]]
    static std::size_t GlyphCountForLayout(
        CharacterSetLayout layout) noexcept
    {
        return
            layout == CharacterSetLayout::Characters64
                ? 64
                : 128;
    }

    [[nodiscard]]
    static u16 BaseAddressFromChbase(
        u8 chbase,
        CharacterSetLayout layout) noexcept
    {
        const u8 mask =
            layout == CharacterSetLayout::Characters64
                ? 0xFE
                : 0xFC;

        return static_cast<u16>(
            static_cast<u16>(
                chbase & mask) <<
            8);
    }

    [[nodiscard]]
    static std::vector<CharacterSetRequest>
        DiscoverRequests(
            const Memory& memory,
            const DisplayListAnalysisResult&
                displayLists)
    {
        bool uses64Characters = false;
        bool uses128Characters = false;

        for (const auto& displayList :
             displayLists.displayLists)
        {
            for (const auto& instruction :
                 displayList.instructions)
            {
                if (instruction.kind !=
                    DisplayListInstructionKind::
                        ModeLine)
                {
                    continue;
                }

                if (instruction.mode >= 2 &&
                    instruction.mode <= 5)
                {
                    uses128Characters = true;
                }
                else if (instruction.mode >= 6 &&
                         instruction.mode <= 7)
                {
                    uses64Characters = true;
                }
            }
        }

        if (!uses64Characters &&
            !uses128Characters)
        {
            return {};
        }

        std::vector<u8> chbaseValues;

        AddChbaseValue(
            memory,
            OsCharacterBaseAddress,
            chbaseValues);

        AddChbaseValue(
            memory,
            AnticCharacterBaseAddress,
            chbaseValues);

        std::sort(
            chbaseValues.begin(),
            chbaseValues.end());

        chbaseValues.erase(
            std::unique(
                chbaseValues.begin(),
                chbaseValues.end()),
            chbaseValues.end());

        std::vector<CharacterSetRequest> result;

        for (const u8 chbase : chbaseValues)
        {
            if (uses64Characters)
            {
                result.push_back(
                    CharacterSetRequest{
                        BaseAddressFromChbase(
                            chbase,
                            CharacterSetLayout::
                                Characters64),
                        CharacterSetLayout::
                            Characters64});
            }

            if (uses128Characters)
            {
                result.push_back(
                    CharacterSetRequest{
                        BaseAddressFromChbase(
                            chbase,
                            CharacterSetLayout::
                                Characters128),
                        CharacterSetLayout::
                            Characters128});
            }
        }

        SortAndRemoveDuplicateRequests(result);

        return result;
    }

    [[nodiscard]]
    CharacterSetAnalysisResult Analyze(
        const Memory& memory,
        const DisplayListAnalysisResult&
            displayLists) const
    {
        return Analyze(
            memory,
            DiscoverRequests(
                memory,
                displayLists));
    }

    [[nodiscard]]
    CharacterSetAnalysisResult Analyze(
        const Memory& memory,
        std::vector<CharacterSetRequest>
            requests) const
    {
        CharacterSetAnalysisResult result;

        SortAndRemoveDuplicateRequests(requests);

        result.requests = requests;

        for (const auto& request :
             result.requests)
        {
            result.characterSets.push_back(
                AnalyzeOne(
                    memory,
                    request));
        }

        return result;
    }

private:

    static void AddChbaseValue(
        const Memory& memory,
        u16 address,
        std::vector<u8>& values)
    {
        if (memory.Cell(address).initialized)
        {
            values.push_back(
                memory.Read8(address));
        }
    }

    static void SortAndRemoveDuplicateRequests(
        std::vector<CharacterSetRequest>& requests)
    {
        const auto less =
            [](const CharacterSetRequest& left,
               const CharacterSetRequest& right)
            {
                if (left.baseAddress !=
                    right.baseAddress)
                {
                    return
                        left.baseAddress <
                        right.baseAddress;
                }

                return
                    static_cast<int>(left.layout) <
                    static_cast<int>(right.layout);
            };

        std::sort(
            requests.begin(),
            requests.end(),
            less);

        requests.erase(
            std::unique(
                requests.begin(),
                requests.end(),
                [](const CharacterSetRequest& left,
                   const CharacterSetRequest& right)
                {
                    return
                        left.baseAddress ==
                            right.baseAddress &&
                        left.layout == right.layout;
                }),
            requests.end());
    }

    [[nodiscard]]
    static CharacterSetAnalysis AnalyzeOne(
        const Memory& memory,
        const CharacterSetRequest& request)
    {
        CharacterSetAnalysis result;
        result.baseAddress = request.baseAddress;
        result.layout = request.layout;
        result.expectedGlyphCount =
            GlyphCountForLayout(request.layout);

        result.glyphs.reserve(
            result.expectedGlyphCount);

        for (std::size_t glyphIndex = 0;
             glyphIndex < result.expectedGlyphCount;
             ++glyphIndex)
        {
            const std::uint32_t glyphAddress =
                static_cast<std::uint32_t>(
                    request.baseAddress) +
                static_cast<std::uint32_t>(
                    glyphIndex) *
                static_cast<std::uint32_t>(
                    CharacterGlyph::RowCount);

            if (glyphAddress >= MemorySize)
            {
                result.addressSpaceTruncated = true;
                break;
            }

            CharacterGlyph glyph;
            glyph.index =
                static_cast<u8>(glyphIndex);
            glyph.address =
                static_cast<u16>(glyphAddress);

            for (std::size_t row = 0;
                 row < CharacterGlyph::RowCount;
                 ++row)
            {
                const std::uint32_t rowAddress =
                    glyphAddress +
                    static_cast<std::uint32_t>(
                        row);

                if (rowAddress >= MemorySize)
                {
                    result.addressSpaceTruncated = true;
                    break;
                }

                const auto memoryAddress =
                    static_cast<u16>(rowAddress);

                glyph.rows[row] =
                    memory.Read8(memoryAddress);

                if (memory.Cell(memoryAddress).
                        initialized)
                {
                    glyph.initializedRowMask |=
                        static_cast<u8>(1U << row);

                    ++result.initializedByteCount;
                }
            }

            result.glyphs.push_back(glyph);

            if (result.addressSpaceTruncated)
            {
                break;
            }
        }

        return result;
    }
};

} // namespace atari
