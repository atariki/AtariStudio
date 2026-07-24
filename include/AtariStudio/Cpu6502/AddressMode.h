#pragma once

#include <cstdint>

namespace atari::cpu6502
{

    enum class AddressMode : std::uint8_t
    {
        Implied,
        Accumulator,

        Immediate,

        ZeroPage,
        ZeroPageX,
        ZeroPageY,

        Relative,

        Absolute,
        AbsoluteX,
        AbsoluteY,

        Indirect,

        IndexedIndirect,   // ($44,X)
        IndirectIndexed    // ($44),Y
    };

} // namespace atari::cpu6502