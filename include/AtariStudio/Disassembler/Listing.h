#pragma once

#include <vector>

#include <string>

#include <AtariStudio/Disassembler/DisassembledInstruction.h>

namespace atari
{

    class Memory;
    class Disassembler;

    class Listing
    {
    public:

        [[nodiscard]]
        std::vector<DisassembledInstruction> Build(
            const Memory& memory,
            uint16_t startAddress,
            uint16_t endAddress) const;

        [[nodiscard]]
        std::string FormatLine(
            const DisassembledInstruction& instruction) const;

        [[nodiscard]]
        std::vector<std::string> Format(
            const std::vector<DisassembledInstruction>& instructions) const;
    };

} // namespace atari