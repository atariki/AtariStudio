#pragma once

#include <cstdint>

namespace atari::cpu6502
{

    using Opcode = std::uint8_t;

    constexpr Opcode InvalidOpcode = 0xFF;

    [[nodiscard]]
    constexpr bool IsValidOpcode(Opcode opcode) noexcept
    {
        return opcode <= 0xFF;
    }

} // namespace atari::cpu6502