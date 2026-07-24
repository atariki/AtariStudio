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
        ZeroPage
    };

    struct Segment
    {
        u16 begin = 0;
        u16 end = 0;

        SegmentType type = SegmentType::Unknown;

        std::string name;
    };

}