#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <AtariStudio/Core/Project.h>
#include <AtariStudio/Core/Types.h>
#include <AtariStudio/Disassembler/CodeDataAnalyzer.h>
#include <AtariStudio/Disassembler/ControlFlowAnalyzer.h>
#include <AtariStudio/Disassembler/Disassembler.h>
#include <AtariStudio/Disassembler/DisassemblyMetadata.h>

namespace atari
{

enum class DisassemblyListingRowType
{
    Code,
    Data
};

struct DisassemblyListingRow
{
    DisassemblyListingRowType type =
        DisassemblyListingRowType::Data;

    //
    // Address of the first byte represented
    // by this listing row.
    //
    u16 address = 0;

    //
    // Label assigned to this address.
    //
    std::string label;

    //
    // Machine-code or data bytes.
    //
    std::vector<u8> bytes;

    //
    // Formatted assembly instruction.
    //
    // Empty for DATA rows.
    //
    std::string instruction;

    //
    // Atari symbols, relocation information,
    // runtime address and XREF.
    //
    std::string comment;

    [[nodiscard]]
    bool IsCode() const noexcept
    {
        return
            type ==
            DisassemblyListingRowType::Code;
    }

    [[nodiscard]]
    bool IsData() const noexcept
    {
        return
            type ==
            DisassemblyListingRowType::Data;
    }
};

struct DisassemblyListingRegion
{
    DisassemblyListingRowType type =
        DisassemblyListingRowType::Data;

    u16 begin = 0;
    u16 end = 0;

    std::vector<DisassemblyListingRow> rows;

    [[nodiscard]]
    std::uint32_t Size() const noexcept
    {
        if (end < begin)
        {
            return 0;
        }

        return
            static_cast<std::uint32_t>(end) -
            static_cast<std::uint32_t>(begin) +
            1;
    }

    [[nodiscard]]
    bool IsCode() const noexcept
    {
        return
            type ==
            DisassemblyListingRowType::Code;
    }

    [[nodiscard]]
    bool IsData() const noexcept
    {
        return
            type ==
            DisassemblyListingRowType::Data;
    }
};

class DisassemblyListing
{
public:

    void Clear()
    {
        m_regions.clear();
        m_rowCount = 0;
    }

    void Build(
        const Project& project,
        const ControlFlowAnalysisResult& controlFlow,
        const DisassemblyMetadata& metadata,
        const std::vector<CodeDataRegion>& regions)
    {
        Clear();

        const auto& memory =
            project.GetMemory();

        for (const auto& sourceRegion :
             regions)
        {
            DisassemblyListingRegion region;

            region.begin =
                sourceRegion.begin;

            region.end =
                sourceRegion.end;

            if (sourceRegion.type ==
                CodeDataRegionType::Code)
            {
                region.type =
                    DisassemblyListingRowType::Code;

                BuildCodeRegion(
                    memory,
                    controlFlow,
                    metadata,
                    sourceRegion,
                    region);
            }
            else
            {
                region.type =
                    DisassemblyListingRowType::Data;

                BuildDataRegion(
                    memory,
                    metadata,
                    sourceRegion,
                    region);
            }

            m_rowCount +=
                region.rows.size();

            m_regions.push_back(
                std::move(region));
        }
    }

    [[nodiscard]]
    const std::vector<DisassemblyListingRegion>&
    Regions() const noexcept
    {
        return m_regions;
    }

    [[nodiscard]]
    std::size_t RowCount() const noexcept
    {
        return m_rowCount;
    }

    [[nodiscard]]
    bool Empty() const noexcept
    {
        return m_regions.empty();
    }

private:

    static void BuildCodeRegion(
        const Memory& memory,
        const ControlFlowAnalysisResult& controlFlow,
        const DisassemblyMetadata& metadata,
        const CodeDataRegion& sourceRegion,
        DisassemblyListingRegion& targetRegion)
    {
        Disassembler disassembler;

        const auto& addresses =
            controlFlow.instructionAddresses;

        const auto beginIterator =
            std::lower_bound(
                addresses.begin(),
                addresses.end(),
                sourceRegion.begin);

        for (auto iterator = beginIterator;
             iterator != addresses.end();
             ++iterator)
        {
            const u16 address =
                *iterator;

            if (address >
                sourceRegion.end)
            {
                break;
            }

            const auto instruction =
                disassembler.Decode(
                    memory,
                    address);

            if (instruction.length == 0)
            {
                continue;
            }

            DisassemblyListingRow row;

            row.type =
                DisassemblyListingRowType::Code;

            row.address =
                address;

            //
            // Label.
            //
            if (const std::string* label =
                    metadata.Symbols().Find(
                        address);
                label != nullptr)
            {
                row.label =
                    *label;
            }

            //
            // Bytes.
            //
            row.bytes.reserve(
                instruction.length);

            for (std::size_t i = 0;
                 i < instruction.length;
                 ++i)
            {
                row.bytes.push_back(
                    instruction.bytes[i]);
            }

            //
            // Assembly text with labels.
            //
            row.instruction =
                metadata.FormatInstruction(
                    instruction);

            //
            // Atari symbols, relocation,
            // runtime address and XREF.
            //
            row.comment =
                metadata.BuildComment(
                    instruction);

            targetRegion.rows.push_back(
                std::move(row));
        }
    }

    static void BuildDataRegion(
        const Memory& memory,
        const DisassemblyMetadata& metadata,
        const CodeDataRegion& sourceRegion,
        DisassemblyListingRegion& targetRegion)
    {
        constexpr std::uint32_t
            BytesPerRow = 8;

        std::uint32_t address =
            sourceRegion.begin;

        const std::uint32_t end =
            sourceRegion.end;

        while (address <= end)
        {
            DisassemblyListingRow row;

            row.type =
                DisassemblyListingRowType::Data;

            row.address =
                static_cast<u16>(
                    address);

            //
            // A DATA address may also receive
            // a symbol in future analysis modes.
            //
            if (const std::string* label =
                    metadata.Symbols().Find(
                        row.address);
                label != nullptr)
            {
                row.label =
                    *label;
            }

            for (std::uint32_t i = 0;
                 i < BytesPerRow;
                 ++i)
            {
                const std::uint32_t
                    byteAddress =
                        address + i;

                if (byteAddress > end ||
                    byteAddress > 0xFFFF)
                {
                    break;
                }

                row.bytes.push_back(
                    memory.Read8(
                        static_cast<u16>(
                            byteAddress)));
            }

            targetRegion.rows.push_back(
                std::move(row));

            if (address + BytesPerRow >
                0xFFFF)
            {
                break;
            }

            address +=
                BytesPerRow;
        }
    }

    std::vector<DisassemblyListingRegion>
        m_regions;

    std::size_t m_rowCount = 0;
};

} // namespace atari