#pragma once

#include <AtariStudio/Core/Types.h>

namespace atari
{

class Address
{
public:

    constexpr Address() = default;

    constexpr explicit Address(u16 value)
        : m_value(value)
    {
    }

    [[nodiscard]]
    constexpr u16 Value() const
    {
        return m_value;
    }

private:

    u16 m_value = 0;
};

}