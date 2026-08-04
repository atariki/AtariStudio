#include <AtariStudio/Core/Memory.h>

#include <iostream>

namespace
{

bool Expect(
    bool condition,
    const char* message)
{
    if (!condition)
    {
        std::cerr
            << "FAILED: "
            << message
            << '\n';
    }

    return condition;
}

} // namespace

int main()
{
    bool passed = true;
    atari::Memory memory;

    passed &=
        Expect(
            memory.Read8(0x0000) == 0 &&
            memory.Read8(0xFFFF) == 0 &&
            !memory.Cell(0x0000).initialized &&
            !memory.Cell(0xFFFF).initialized,
            "new memory must be zeroed and uninitialized");

    memory.Write16(
        0x1234,
        0xABCD);

    passed &=
        Expect(
            memory.Read8(0x1234) == 0xCD &&
            memory.Read8(0x1235) == 0xAB &&
            memory.Read16(0x1234) == 0xABCD &&
            memory.Cell(0x1234).initialized &&
            memory.Cell(0x1235).initialized,
            "word access must use little-endian byte order");

    memory.Write16(
        0xFFFF,
        0x3412);

    passed &=
        Expect(
            memory.Read8(0xFFFF) == 0x12 &&
            memory.Read8(0x0000) == 0x34 &&
            memory.Read16(0xFFFF) == 0x3412,
            "word access at $FFFF must wrap to $0000");

    auto& changedCell =
        memory.Cell(0x4000);

    changedCell.value = 0xFF;
    changedCell.type =
        atari::MemoryType::Hardware;
    changedCell.initialized = true;
    changedCell.executable = true;
    changedCell.readable = false;
    changedCell.writable = false;

    memory.Clear();

    const auto& clearedCell =
        memory.Cell(0x4000);

    passed &=
        Expect(
            clearedCell.value == 0 &&
            clearedCell.type ==
                atari::MemoryType::RAM &&
            !clearedCell.initialized &&
            !clearedCell.executable &&
            clearedCell.readable &&
            clearedCell.writable,
            "Clear must restore value and all cell metadata");

    return passed ? 0 : 1;
}
