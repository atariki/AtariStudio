#pragma once

#include <cstdint>
#include <vector>

#include <AtariStudio/Core/Project.h>
#include <AtariStudio/Core/Types.h>

namespace atari
{

enum class CodeDataRegionType
{
    Code,
    Data
};

struct CodeDataRegion
{
    u16 begin = 0;
    u16 end = 0;

    CodeDataRegionType type =
        CodeDataRegionType::Data;

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
};

class CodeDataAnalyzer
{
public:

    [[nodiscard]]
    std::vector<CodeDataRegion> Analyze(
        const Project& project) const
    {
        std::vector<CodeDataRegion>
            regions;

        const auto& memory =
            project.GetMemory();

        for (const auto& segment :
             project.Segments())
        {
            if (segment.type ==
                SegmentType::System)
            {
                continue;
            }

            if (segment.end <
                segment.begin)
            {
                continue;
            }

            const auto typeAt =
                [&](u16 address)
                {
                    return
                        memory.Cell(address).
                            executable
                            ? CodeDataRegionType::
                                Code
                            : CodeDataRegionType::
                                Data;
                };

            CodeDataRegion current;

            current.begin =
                segment.begin;

            current.end =
                segment.begin;

            current.type =
                typeAt(
                    segment.begin);

            for (std::uint32_t address =
                     static_cast<
                         std::uint32_t>(
                             segment.begin) +
                     1;
                 address <=
                     static_cast<
                         std::uint32_t>(
                             segment.end);
                 ++address)
            {
                const auto currentAddress =
                    static_cast<u16>(
                        address);

                const auto type =
                    typeAt(
                        currentAddress);

                if (type ==
                    current.type)
                {
                    current.end =
                        currentAddress;

                    continue;
                }

                regions.push_back(
                    current);

                current.begin =
                    currentAddress;

                current.end =
                    currentAddress;

                current.type =
                    type;
            }

            regions.push_back(
                current);
        }

        return regions;
    }
};

} // namespace atari