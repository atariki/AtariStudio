#pragma once

#include <cstddef>
#include <cstdint>

#include <AtariStudio/Core/Project.h>

namespace atari
{

struct ProjectStatistics
{
    std::size_t segmentCount = 0;

    std::size_t codeSegments = 0;
    std::size_t dataSegments = 0;
    std::size_t systemSegments = 0;
    std::size_t unknownSegments = 0;

    std::size_t charsetSegments = 0;
    std::size_t screenSegments = 0;
    std::size_t displayListSegments = 0;
    std::size_t hardwareSegments = 0;
    std::size_t zeroPageSegments = 0;

    std::size_t overlappingSegments = 0;

    std::uint64_t totalBytes = 0;
};

[[nodiscard]]
inline ProjectStatistics CalculateProjectStatistics(
    const Project& project) noexcept
{
    ProjectStatistics statistics;

    const auto& segments = project.Segments();

    statistics.segmentCount = segments.size();

    for (const auto& segment : segments)
    {
        statistics.totalBytes += segment.Size();

        if (segment.overlapping)
        {
            ++statistics.overlappingSegments;
        }

        switch (segment.type)
        {
        case SegmentType::Unknown:
            ++statistics.unknownSegments;
            break;

        case SegmentType::Code:
            ++statistics.codeSegments;
            break;

        case SegmentType::Data:
            ++statistics.dataSegments;
            break;

        case SegmentType::Charset:
            ++statistics.charsetSegments;
            break;

        case SegmentType::Screen:
            ++statistics.screenSegments;
            break;

        case SegmentType::DisplayList:
            ++statistics.displayListSegments;
            break;

        case SegmentType::Hardware:
            ++statistics.hardwareSegments;
            break;

        case SegmentType::ZeroPage:
            ++statistics.zeroPageSegments;
            break;

        case SegmentType::System:
            ++statistics.systemSegments;
            break;
        }
    }

    return statistics;
}

} // namespace atari