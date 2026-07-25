#include <iostream>

#include <AtariStudio/Core/Memory.h>
#include <AtariStudio/Disassembler/Disassembler.h>
#include <AtariStudio/Disassembler/Listing.h>

int main()
{
    std::cout << "=====================================\n";
    std::cout << " AtariStudio Test Application\n";
    std::cout << "=====================================\n\n";

    atari::Memory memory;

    std::cout << "Memory successfully created.\n";

    atari::Listing listing;

    std::cout << "Listing engine successfully created.\n";

    atari::Disassembler disassembler;

    std::cout << "Disassembler successfully created.\n";

    std::cout << "\nInitialization completed successfully.\n";

    std::cout << "\nInitialization completed successfully.\n";

    std::cout << "\nPress Enter to exit...";
    std::cin.get();

    return 0;
}