#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <AtariStudio/Core/Memory.h>
#include <AtariStudio/Core/Types.h>
#include <AtariStudio/Disassembler/DisplayListAnalyzer.h>

namespace atari
{

enum class PlayfieldWidth : u8
{
    Disabled = 0,
    Narrow = 1,
    Normal = 2,
    Wide = 3
};

struct ScreenMemoryRow
{
    u16 displayListInstructionAddress = 0;
    u8 mode = 0;

    std::size_t firstNominalScanLine = 0;
    std::size_t nominalScanLineCount = 0;

    std::optional<u16> screenAddress;

    std::size_t byteCount = 0;
    std::size_t initializedByteCount = 0;

    std::vector<u8> bytes;
    std::vector<bool> initialized;

    bool horizontalScroll = false;
    bool verticalScroll = false;
    bool loadMemoryScan = false;
    bool memoryScanWrapped = false;

    [[nodiscard]]
    bool AddressResolved() const noexcept
    {
        return screenAddress.has_value();
    }

    [[nodiscard]]
    bool Complete() const noexcept
    {
        return
            AddressResolved() &&
            bytes.size() == byteCount &&
            initialized.size() == byteCount &&
            initializedByteCount == byteCount;
    }
};

struct ScreenMemoryAnalysis
{
    u16 displayListEntryPoint = 0;

    PlayfieldWidth playfieldWidth =
        PlayfieldWidth::Disabled;

    std::vector<ScreenMemoryRow> rows;

    std::size_t nominalScanLineCount = 0;
    std::size_t displayByteCount = 0;
    std::size_t initializedByteCount = 0;
    std::size_t unresolvedRowCount = 0;
    std::size_t memoryScanWrapCount = 0;

    bool displayListComplete = false;

    [[nodiscard]]
    bool Complete() const noexcept
    {
        return
            displayListComplete &&
            unresolvedRowCount == 0 &&
            initializedByteCount ==
                displayByteCount;
    }
};

struct ScreenMemoryAnalysisResult
{
    std::vector<PlayfieldWidth> playfieldWidths;

    std::vector<ScreenMemoryAnalysis> screens;

    [[nodiscard]]
    std::size_t CompleteCount() const noexcept
    {
        return static_cast<std::size_t>(
            std::count_if(
                screens.begin(),
                screens.end(),
                [](const ScreenMemoryAnalysis& screen)
                {
                    return screen.Complete();
                }));
    }

    [[nodiscard]]
    std::size_t RowCount() const noexcept
    {
        std::size_t result = 0;

        for (const auto& screen : screens)
        {
            result += screen.rows.size();
        }

        return result;
    }
};

class ScreenMemoryAnalyzer
{
public:

    static constexpr u16
        OsDmaControlAddress = 0x022F;

    static constexpr u16
        AnticDmaControlAddress = 0xD400;

    [[nodiscard]]
    static std::size_t NominalScanLinesForMode(
        u8 mode) noexcept
    {
        switch (mode)
        {
        case 2:
            return 8;

        case 3:
            return 10;

        case 4:
            return 8;

        case 5:
            return 16;

        case 6:
            return 8;

        case 7:
            return 16;

        case 8:
            return 8;

        case 9:
        case 10:
            return 4;

        case 11:
        case 13:
            return 2;

        case 12:
        case 14:
        case 15:
            return 1;

        default:
            return 0;
        }
    }

    [[nodiscard]]
    static PlayfieldWidth EffectiveFetchWidth(
        PlayfieldWidth playfieldWidth,
        bool horizontalScroll) noexcept
    {
        if (!horizontalScroll)
        {
            return playfieldWidth;
        }

        switch (playfieldWidth)
        {
        case PlayfieldWidth::Narrow:
            return PlayfieldWidth::Normal;

        case PlayfieldWidth::Normal:
            return PlayfieldWidth::Wide;

        default:
            return playfieldWidth;
        }
    }

    [[nodiscard]]
    static std::size_t BytesPerModeLine(
        u8 mode,
        PlayfieldWidth playfieldWidth,
        bool horizontalScroll = false) noexcept
    {
        const auto fetchWidth =
            EffectiveFetchWidth(
                playfieldWidth,
                horizontalScroll);

        std::size_t widthIndex = 0;

        switch (fetchWidth)
        {
        case PlayfieldWidth::Narrow:
            widthIndex = 0;
            break;

        case PlayfieldWidth::Normal:
            widthIndex = 1;
            break;

        case PlayfieldWidth::Wide:
            widthIndex = 2;
            break;

        default:
            return 0;
        }

        if ((mode >= 2 && mode <= 5) ||
            (mode >= 13 && mode <= 15))
        {
            static constexpr std::size_t widths[] =
                {32, 40, 48};

            return widths[widthIndex];
        }

        if ((mode >= 6 && mode <= 7) ||
            (mode >= 10 && mode <= 12))
        {
            static constexpr std::size_t widths[] =
                {16, 20, 24};

            return widths[widthIndex];
        }

        if (mode >= 8 && mode <= 9)
        {
            static constexpr std::size_t widths[] =
                {8, 10, 12};

            return widths[widthIndex];
        }

        return 0;
    }

    [[nodiscard]]
    static std::vector<PlayfieldWidth>
        DiscoverPlayfieldWidths(
            const Memory& memory)
    {
        std::vector<PlayfieldWidth> result;

        AddPlayfieldWidth(
            memory,
            OsDmaControlAddress,
            result);

        AddPlayfieldWidth(
            memory,
            AnticDmaControlAddress,
            result);

        SortAndRemoveDuplicateWidths(result);

        return result;
    }

    [[nodiscard]]
    ScreenMemoryAnalysisResult Analyze(
        const Memory& memory,
        const DisplayListAnalysisResult&
            displayLists) const
    {
        return Analyze(
            memory,
            displayLists,
            DiscoverPlayfieldWidths(memory));
    }

    [[nodiscard]]
    ScreenMemoryAnalysisResult Analyze(
        const Memory& memory,
        const DisplayListAnalysisResult&
            displayLists,
        std::vector<PlayfieldWidth>
            playfieldWidths) const
    {
        ScreenMemoryAnalysisResult result;

        playfieldWidths.erase(
            std::remove(
                playfieldWidths.begin(),
                playfieldWidths.end(),
                PlayfieldWidth::Disabled),
            playfieldWidths.end());

        SortAndRemoveDuplicateWidths(
            playfieldWidths);

        result.playfieldWidths =
            playfieldWidths;

        for (const auto& displayList :
             displayLists.displayLists)
        {
            for (const auto playfieldWidth :
                 result.playfieldWidths)
            {
                result.screens.push_back(
                    AnalyzeOne(
                        memory,
                        displayList,
                        playfieldWidth));
            }
        }

        return result;
    }

private:

    static void AddPlayfieldWidth(
        const Memory& memory,
        u16 address,
        std::vector<PlayfieldWidth>& widths)
    {
        if (!memory.Cell(address).initialized)
        {
            return;
        }

        const auto width =
            static_cast<PlayfieldWidth>(
                memory.Read8(address) & 0x03);

        if (width != PlayfieldWidth::Disabled)
        {
            widths.push_back(width);
        }
    }

    static void SortAndRemoveDuplicateWidths(
        std::vector<PlayfieldWidth>& widths)
    {
        std::sort(
            widths.begin(),
            widths.end(),
            [](PlayfieldWidth left,
               PlayfieldWidth right)
            {
                return
                    static_cast<int>(left) <
                    static_cast<int>(right);
            });

        widths.erase(
            std::unique(
                widths.begin(),
                widths.end()),
            widths.end());
    }

    [[nodiscard]]
    static ScreenMemoryAnalysis AnalyzeOne(
        const Memory& memory,
        const DisplayListAnalysis& displayList,
        PlayfieldWidth playfieldWidth)
    {
        ScreenMemoryAnalysis result;
        result.displayListEntryPoint =
            displayList.entryPoint;
        result.playfieldWidth = playfieldWidth;
        result.displayListComplete =
            displayList.Complete();

        std::optional<u16> memoryScanAddress;
        std::size_t nominalScanLine = 0;

        for (const auto& instruction :
             displayList.instructions)
        {
            if (instruction.kind ==
                DisplayListInstructionKind::Blank)
            {
                nominalScanLine +=
                    instruction.blankScanLines;

                continue;
            }

            if (instruction.kind !=
                DisplayListInstructionKind::ModeLine)
            {
                continue;
            }

            ScreenMemoryRow row;
            row.displayListInstructionAddress =
                instruction.address;
            row.mode = instruction.mode;
            row.firstNominalScanLine =
                nominalScanLine;
            row.nominalScanLineCount =
                NominalScanLinesForMode(
                    instruction.mode);
            row.horizontalScroll =
                instruction.horizontalScroll;
            row.verticalScroll =
                instruction.verticalScroll;
            row.loadMemoryScan =
                instruction.loadMemoryScan;
            row.byteCount =
                BytesPerModeLine(
                    instruction.mode,
                    playfieldWidth,
                    instruction.horizontalScroll);

            nominalScanLine +=
                row.nominalScanLineCount;

            if (instruction.memoryScanAddress.
                    has_value())
            {
                memoryScanAddress =
                    instruction.memoryScanAddress;
            }

            if (!memoryScanAddress.has_value())
            {
                ++result.unresolvedRowCount;
                result.displayByteCount += row.byteCount;
                result.rows.push_back(row);
                continue;
            }

            row.screenAddress = memoryScanAddress;
            row.bytes.reserve(row.byteCount);
            row.initialized.reserve(row.byteCount);

            const std::uint32_t bank =
                static_cast<std::uint32_t>(
                    memoryScanAddress.value()) &
                0xF000U;

            const std::uint32_t offset =
                static_cast<std::uint32_t>(
                    memoryScanAddress.value()) &
                0x0FFFU;

            row.memoryScanWrapped =
                offset +
                    static_cast<std::uint32_t>(
                        row.byteCount) >=
                0x1000U;

            for (std::size_t index = 0;
                 index < row.byteCount;
                 ++index)
            {
                const std::uint32_t wrappedOffset =
                    (offset +
                     static_cast<std::uint32_t>(
                         index)) &
                    0x0FFFU;

                const auto address =
                    static_cast<u16>(
                        bank | wrappedOffset);

                row.bytes.push_back(
                    memory.Read8(address));

                const bool initialized =
                    memory.Cell(address).initialized;

                row.initialized.push_back(
                    initialized);

                if (initialized)
                {
                    ++row.initializedByteCount;
                    ++result.initializedByteCount;
                }
            }

            if (row.memoryScanWrapped)
            {
                ++result.memoryScanWrapCount;
            }

            const std::uint32_t nextOffset =
                (offset +
                 static_cast<std::uint32_t>(
                     row.byteCount)) &
                0x0FFFU;

            memoryScanAddress =
                static_cast<u16>(
                    bank | nextOffset);

            result.displayByteCount += row.byteCount;
            result.rows.push_back(row);
        }

        result.nominalScanLineCount =
            nominalScanLine;

        return result;
    }
};

} // namespace atari
