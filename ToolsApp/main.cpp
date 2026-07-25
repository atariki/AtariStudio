#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <AtariStudio/Core/Project.h>
#include <AtariStudio/Core/ProjectStatistics.h>
#include <AtariStudio/Disassembler/CodeDataAnalyzer.h>
#include <AtariStudio/Disassembler/CodeIslandAnalyzer.h>
#include <AtariStudio/Disassembler/ControlFlowAnalyzer.h>
#include <AtariStudio/Disassembler/Disassembler.h>
#include <AtariStudio/Disassembler/DisassemblyMetadata.h>
#include <AtariStudio/Formats/XEX/XexLoader.h>

namespace
{

const char* SegmentTypeToString(
    atari::SegmentType type)
{
    switch (type)
    {
    case atari::SegmentType::Unknown:
        return "Unknown";

    case atari::SegmentType::Code:
        return "Code";

    case atari::SegmentType::Data:
        return "Data";

    case atari::SegmentType::Charset:
        return "Charset";

    case atari::SegmentType::Screen:
        return "Screen";

    case atari::SegmentType::DisplayList:
        return "DisplayList";

    case atari::SegmentType::Hardware:
        return "Hardware";

    case atari::SegmentType::ZeroPage:
        return "ZeroPage";

    case atari::SegmentType::System:
        return "System";

    default:
        return "Unknown";
    }
}

void PrintAddress(
    atari::u16 address)
{
    std::cout
        << '$'
        << std::uppercase
        << std::hex
        << std::right
        << std::setw(4)
        << std::setfill('0')
        << address;
}

std::string AddressToString(
    atari::u16 address)
{
    std::ostringstream stream;

    stream
        << '$'
        << std::uppercase
        << std::hex
        << std::setw(4)
        << std::setfill('0')
        << address;

    return stream.str();
}

void PrintInstruction(
    const atari::DisassembledInstruction& instruction,
    const atari::DisassemblyMetadata& metadata)
{
    const std::string* label =
        metadata.Symbols().Find(
            instruction.address);

    if (label != nullptr)
    {
        std::cout
            << std::left
            << std::setw(12)
            << std::setfill(' ')
            << *label;
    }
    else
    {
        std::cout
            << std::left
            << std::setw(12)
            << std::setfill(' ')
            << "";
    }

    PrintAddress(
        instruction.address);

    std::cout << "  ";

    for (std::size_t i = 0;
         i < instruction.length;
         ++i)
    {
        std::cout
            << std::uppercase
            << std::hex
            << std::right
            << std::setw(2)
            << std::setfill('0')
            << static_cast<unsigned>(
                instruction.bytes[i])
            << ' ';
    }

    for (std::size_t i =
             instruction.length;
         i < 3;
         ++i)
    {
        std::cout << "   ";
    }

    const std::string instructionText =
        metadata.FormatInstruction(
            instruction);

    std::cout
        << " "
        << std::left
        << std::setw(24)
        << std::setfill(' ')
        << instructionText;

    const std::string comment =
        metadata.BuildComment(
            instruction);

    if (!comment.empty())
    {
        std::cout
            << " ; "
            << comment;
    }

    std::cout << '\n';
}

void PrintDataRegion(
    const atari::Memory& memory,
    const atari::CodeDataRegion& region,
    const atari::DisassemblyMetadata& metadata)
{
    constexpr std::uint32_t bytesPerLine = 8;

    std::uint32_t address =
        region.begin;

    const std::uint32_t end =
        region.end;

    while (address <= end)
    {
        const auto currentAddress =
            static_cast<atari::u16>(
                address);

        const std::string* label =
            metadata.Symbols().Find(
                currentAddress);

        if (label != nullptr)
        {
            std::cout
                << std::left
                << std::setw(12)
                << std::setfill(' ')
                << *label;
        }
        else
        {
            std::cout
                << std::left
                << std::setw(12)
                << std::setfill(' ')
                << "";
        }

        PrintAddress(
            currentAddress);

        std::cout << "  ";

        for (std::uint32_t i = 0;
             i < bytesPerLine;
             ++i)
        {
            const std::uint32_t byteAddress =
                address + i;

            if (byteAddress > end ||
                byteAddress > 0xFFFF)
            {
                break;
            }

            const auto value =
                memory.Read8(
                    static_cast<atari::u16>(
                        byteAddress));

            std::cout
                << std::uppercase
                << std::hex
                << std::right
                << std::setw(2)
                << std::setfill('0')
                << static_cast<unsigned>(
                    value)
                << ' ';
        }

        std::cout << '\n';

        if (address + bytesPerLine >
            0xFFFF)
        {
            break;
        }

        address +=
            bytesPerLine;
    }
}

void PrintCodeRegion(
    const atari::Memory& memory,
    const atari::CodeDataRegion& region,
    const atari::ControlFlowAnalysisResult& analysis,
    const atari::DisassemblyMetadata& metadata)
{
    atari::Disassembler disassembler;

    const auto beginIterator =
        std::lower_bound(
            analysis.instructionAddresses.begin(),
            analysis.instructionAddresses.end(),
            region.begin);

    for (auto iterator = beginIterator;
         iterator !=
             analysis.instructionAddresses.end();
         ++iterator)
    {
        const atari::u16 address =
            *iterator;

        if (address > region.end)
        {
            break;
        }

        const auto instruction =
            disassembler.Decode(
                memory,
                address);

        PrintInstruction(
            instruction,
            metadata);
    }
}

void PrintRelocationMap(
    const atari::DisassemblyMetadata& metadata)
{
    std::cout
        << "\nRelocation map:\n";

    const auto& relocation =
        metadata.Relocation();

    if (relocation.ranges.empty())
    {
        std::cout
            << "  none\n";

        return;
    }

    for (const auto& range :
         relocation.ranges)
    {
        const std::uint32_t sourceEnd =
            static_cast<std::uint32_t>(
                range.sourceBegin) +
            range.size - 1;

        const std::uint32_t destinationEnd =
            static_cast<std::uint32_t>(
                range.destinationBegin) +
            range.size - 1;

        std::cout << "  ";

        PrintAddress(
            range.sourceBegin);

        std::cout << " - ";

        PrintAddress(
            static_cast<atari::u16>(
                sourceEnd));

        std::cout << "  ->  ";

        PrintAddress(
            range.destinationBegin);

        std::cout << " - ";

        PrintAddress(
            static_cast<atari::u16>(
                destinationEnd));

        std::cout
            << "  ("
            << std::dec
            << range.size
            << " bytes)\n";
    }
}

void PrintMixedListing(
    const atari::Project& project,
    const atari::ControlFlowAnalysisResult& analysis,
    const atari::DisassemblyMetadata& metadata)
{
    atari::CodeDataAnalyzer analyzer;

    const auto regions =
        analyzer.Analyze(
            project);

    const auto& memory =
        project.GetMemory();

    std::cout
        << "\n=====================================\n"
        << " Code / Data Listing\n"
        << "=====================================\n\n";

    std::cout
        << "LABEL       ADDRESS  BYTES       INSTRUCTION\n"
        << "---------------------------------------------------------------------\n";

    for (const auto& region :
         regions)
    {
        std::cout << '\n';

        if (region.type ==
            atari::CodeDataRegionType::Code)
        {
            std::cout
                << "; CODE "
                << AddressToString(
                    region.begin)
                << " - "
                << AddressToString(
                    region.end)
                << '\n';

            PrintCodeRegion(
                memory,
                region,
                analysis,
                metadata);
        }
        else
        {
            std::cout
                << "; DATA "
                << AddressToString(
                    region.begin)
                << " - "
                << AddressToString(
                    region.end)
                << " ("
                << std::dec
                << region.Size()
                << " bytes)"
                << '\n';

            PrintDataRegion(
                memory,
                region,
                metadata);
        }
    }
}

} // namespace

int main(
    int argc,
    char* argv[])
{
    std::cout
        << "=====================================\n"
        << " AtariStudio Test Application\n"
        << "=====================================\n\n";

    if (argc < 2)
    {
        std::cout
            << "Usage:\n"
            << "  TestApp <file.xex>\n";

        return 1;
    }

    const std::filesystem::path filename =
        argv[1];

    atari::Project project;
    atari::XexLoader loader;

    std::cout
        << "Loading XEX:\n"
        << filename.string()
        << "\n\n";

    if (!loader.Load(
            filename,
            project))
    {
        std::cerr
            << "XEX load failed.\n"
            << "Error: "
            << loader.LastError()
            << '\n';

        return 1;
    }

    std::vector<atari::u16> entryPoints;

    if (project.RunAddress() != 0)
    {
        entryPoints.push_back(
            project.RunAddress());
    }

    if (project.InitAddress() != 0 &&
        project.InitAddress() !=
            project.RunAddress())
    {
        entryPoints.push_back(
            project.InitAddress());
    }

    atari::ControlFlowAnalyzer
        controlFlowAnalyzer;

    auto analysis =
        controlFlowAnalyzer.Analyze(
            project.GetMemory(),
            entryPoints);

    const std::size_t normalInstructions =
        analysis.instructionAddresses.size();

    atari::CodeIslandAnalyzer
        codeIslandAnalyzer;

    codeIslandAnalyzer.Analyze(
        project,
        analysis);

    const std::size_t islandInstructions =
        analysis.instructionAddresses.size() -
        normalInstructions;

    atari::DisassemblyMetadata metadata;

    metadata.Build(
        project,
        analysis);

    std::cout
        << "XEX loaded successfully.\n\n";

    const auto& segments =
        project.Segments();

    std::cout
        << "Segments:\n\n";

    for (std::size_t i = 0;
         i < segments.size();
         ++i)
    {
        const auto& segment =
            segments[i];

        std::cout
            << "  ["
            << std::dec
            << i
            << "] ";

        PrintAddress(
            segment.begin);

        std::cout << " - ";

        PrintAddress(
            segment.end);

        std::cout
            << "  "
            << SegmentTypeToString(
                segment.type)
            << "  "
            << std::dec
            << segment.Size()
            << " bytes";

        if (!segment.name.empty())
        {
            std::cout
                << "  "
                << segment.name;
        }

        if (segment.overlapping)
        {
            std::cout
                << "  [OVERLAP]";
        }

        std::cout << '\n';
    }

    std::cout
        << "\nRUN address:  ";

    if (project.RunAddress() != 0)
    {
        PrintAddress(
            project.RunAddress());
    }
    else
    {
        std::cout << "not set";
    }

    std::cout
        << "\nINIT address: ";

    if (project.InitAddress() != 0)
    {
        PrintAddress(
            project.InitAddress());
    }
    else
    {
        std::cout << "not set";
    }

    const auto statistics =
        atari::CalculateProjectStatistics(
            project);

    std::cout
        << "\n\nXEX Statistics:\n"
        << "  Segments:     "
        << statistics.segmentCount
        << '\n'
        << "  Code:         "
        << statistics.codeSegments
        << '\n'
        << "  System:       "
        << statistics.systemSegments
        << '\n'
        << "  Unknown:      "
        << statistics.unknownSegments
        << '\n'
        << "  Overlapping:  "
        << statistics.overlappingSegments
        << '\n'
        << "  Total bytes:  "
        << statistics.totalBytes
        << '\n';

    std::cout
        << "\nAnalysis:\n"
        << "  Entry points:             "
        << analysis.entryPoints.size()
        << '\n'
        << "  CFG instructions:         "
        << normalInstructions
        << '\n'
        << "  Code-island instructions: "
        << islandInstructions
        << '\n'
        << "  Total instructions:       "
        << analysis.instructionAddresses.size()
        << '\n'
        << "  Cross references:         "
        << metadata.CrossReferences().
            references.size()
        << '\n'
        << "  Symbols:                  "
        << metadata.Symbols().Size()
        << '\n';

    PrintRelocationMap(
        metadata);

    PrintMixedListing(
        project,
        analysis,
        metadata);

    std::cout
        << "\nPress Enter to exit...";

    std::cin.get();

    return 0;
}