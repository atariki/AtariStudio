#include <AtariStudio/Core/Memory.h>

namespace atari
{

    Memory::Memory()
    {
        Clear();
    }

    void Memory::Clear()
    {
        for (auto& cell : m_memory)
        {
            cell.value = 0;
            cell.type = MemoryType::RAM;
            cell.initialized = false;
            cell.executable = false;
            cell.readable = true;
            cell.writable = true;
        }
    }

    u8 Memory::Read8(u16 address) const
    {
        return m_memory[address].value;
    }

    void Memory::Write8(u16 address, u8 value)
    {
        auto& cell = m_memory[address];

        cell.value = value;
        cell.initialized = true;
    }

    u16 Memory::Read16(u16 address) const
    {
        const u8 lo = Read8(address);
        const u8 hi = Read8(address + 1);

        return static_cast<u16>(lo | (hi << 8));
    }

    void Memory::Write16(u16 address, u16 value)
    {
        Write8(address, static_cast<u8>(value & 0xFF));
        Write8(address + 1, static_cast<u8>(value >> 8));
    }

    const MemoryCell& Memory::Cell(u16 address) const
    {
        return m_memory[address];
    }

    MemoryCell& Memory::Cell(u16 address)
    {
        return m_memory[address];
    }

} // namespace atari