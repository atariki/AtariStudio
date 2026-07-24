#pragma once

#include <cstdint>

namespace atari::cpu6502
{

/// <summary>
/// 6502 addressing modes.
/// </summary>
enum class AddressMode : std::uint8_t
{
    Implied,            // IMP
    Accumulator,        // A
    Immediate,          // #$44

    ZeroPage,           // $44
    ZeroPageX,          // $44,X
    ZeroPageY,          // $44,Y

    Relative,           // branch

    Absolute,           // $4400
    AbsoluteX,          // $4400,X
    AbsoluteY,          // $4400,Y

    Indirect,           // ($4400)

    IndexedIndirect,    // ($44,X)

    IndirectIndexed     // ($44),Y
};

}