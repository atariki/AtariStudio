#pragma once

#include <string>
#include <vector>

#include "XexSegment.h"

namespace atari
{

    class XexFile
    {
    public:

        bool Load(const std::string& filename);

        [[nodiscard]]
        const std::vector<XexSegment>& Segments() const;

        [[nodiscard]]
        u16 RunAddress() const;

        [[nodiscard]]
        u16 InitAddress() const;

    private:

        std::vector<XexSegment> m_segments;

        u16 m_runAddress = 0;
        u16 m_initAddress = 0;
    };

}

class XexFile
{
public:

    struct Segment
    {
        uint16_t start;
        uint16_t end;

        uint16_t Size() const;
    };

    ...
};