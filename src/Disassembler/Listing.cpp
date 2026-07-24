#include <AtariStudio/Disassembler/Listing.h>

#include <AtariStudio/Core/Memory.h>
#include <AtariStudio/Disassembler/Disassembler.h>

#include <iomanip>
#include <sstream>

namespace atari
{

    std::vector<DisassembledInstruction> Listing::Build(
        const Memory& memory,
        uint16_t startAddress,
        uint16_t endAddress) const
    {
        std::vector<DisassembledInstruction> listing;

        Disassembler disassembler;

        uint16_t pc = startAddress;

        while (pc <= endAddress)
        {
            auto instruction = disassembler.Decode(memory, pc);

            listing.push_back(instruction);

            if (instruction.length == 0)
            {
                ++pc;
            }
            else
            {
                pc += instruction.length;
            }
        }

        return listing;
    }

    std::string Listing::FormatLine(
        const DisassembledInstruction& instruction) const
    {
        std::ostringstream stream;

        stream << std::uppercase
            << std::hex
            << std::setfill('0');

        // Адрес
        stream << std::setw(4)
            << instruction.address
            << ": ";

        // Байты инструкции
        for (std::size_t i = 0; i < instruction.length; ++i)
        {
            stream << std::setw(2)
                << static_cast<unsigned>(instruction.bytes[i])
                << ' ';
        }

        // Выравнивание поля байтов до 3 байт
        for (std::size_t i = instruction.length; i < 3; ++i)
        {
            stream << "   ";
        }

        stream << " ";

        // Текст инструкции
        stream << instruction.text;

        return stream.str();
    }

    std::vector<std::string> Listing::Format(
        const std::vector<DisassembledInstruction>& instructions) const
    {
        std::vector<std::string> result;

        result.reserve(instructions.size());

        for (const auto& instruction : instructions)
        {
            result.push_back(FormatLine(instruction));
        }

        return result;
    }

} // namespace atari