#pragma once

#include <array>

#include <AtariStudio/Core/Types.h>

namespace atari
{

    enum class MemoryType
    {
        Unknown,
        RAM,
        ROM,
        Hardware
    };

    struct MemoryCell
    {
        u8 value = 0;

        MemoryType type = MemoryType::RAM;

        bool initialized = false;
        bool executable = false;
        bool readable = true;
        bool writable = true;
    };

    class Memory
    {
    public:

        Memory();

        void Clear();

        [[nodiscard]]
        u8 Read8(u16 address) const;

        void Write8(u16 address, u8 value);

        [[nodiscard]]
        u16 Read16(u16 address) const;

        void Write16(u16 address, u16 value);

        [[nodiscard]]
        const MemoryCell& Cell(u16 address) const;

        [[nodiscard]]
        MemoryCell& Cell(u16 address);

    private:

        std::array<MemoryCell, MemorySize> m_memory;
    };

}