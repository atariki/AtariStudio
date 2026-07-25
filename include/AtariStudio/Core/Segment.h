#pragma once

#include <string>

#include <AtariStudio/Core/Types.h>

namespace atari
{

enum class SegmentType
{
    Unknown,
    Code,
    Data,
    Charset,
    Screen,
    DisplayList,
    Hardware,
    ZeroPage,
    System
};

struct Segment
{
    u16 begin = 0;
    u16 end = 0;

    SegmentType type = SegmentType::Unknown;

    std::string name;

    bool overlapping = false;

    [[nodiscard]]
    u32 Size() const noexcept
    {
        if (end < begin)
        {
            return 0;
        }

        return static_cast<u32>(end) -
               static_cast<u32>(begin) + 1;
    }
};

} // namespace atari