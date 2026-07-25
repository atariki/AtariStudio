#include <iostream>

#include <AtariStudio/Core/Memory.h>
#include <AtariStudio/Disassembler/Listing.h>

int main()
{
    std::cout << "=====================================\n";
    std::cout << " AtariStudio Test Application\n";
    std::cout << "=====================================\n\n";

    atari::Memory memory;

    //
    // Test program at $2000:
    //
    // 2000: A9 44      LDA #$44
    // 2002: 8D 00 D0   STA $D000
    // 2005: A2 10      LDX #$10
    // 2007: CA         DEX
    // 2008: D0 FD      BNE $2007
    // 200A: 60         RTS
    //

    memory.Write8(0x2000, 0xA9);
    memory.Write8(0x2001, 0x44);

    memory.Write8(0x2002, 0x8D);
    memory.Write8(0x2003, 0x00);
    memory.Write8(0x2004, 0xD0);

    memory.Write8(0x2005, 0xA2);
    memory.Write8(0x2006, 0x10);

    memory.Write8(0x2007, 0xCA);

    memory.Write8(0x2008, 0xD0);
    memory.Write8(0x2009, 0xFD);

    memory.Write8(0x200A, 0x60);

    atari::Listing listing;

    const auto instructions =
        listing.Build(
            memory,
            0x2000,
            0x200A);

    const auto lines =
        listing.Format(instructions);

    std::cout << "Disassembly:\n\n";

    for (const auto& line : lines)
    {
        std::cout << line << '\n';
    }

    std::cout << "\nPress Enter to exit...";
    std::cin.get();

    return 0;
}