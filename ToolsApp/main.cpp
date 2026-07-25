#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include <AtariStudio/Core/Project.h>
#include <AtariStudio/Core/ProjectStatistics.h>
#include <AtariStudio/Disassembler/AnalysisEngine.h>
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

void PrintAddressList(
    const std::vector<atari::u16>& addresses)
{
    if (addresses.empty())
    {
        std::cout << "none";
        return;
    }

    for (std::size_t i = 0;
         i < addresses.size();
         ++i)
    {
        if (i != 0)
        {
            std::cout << ", ";
        }

        PrintAddress(
            addresses[i]);
    }
}

void PrintRoutines(
    const atari::AnalysisEngineResult& analysis)
{
    std::cout
        << "\n=====================================\n"
        << " Routines\n"
        << "=====================================\n";

    if (analysis.routines.routines.empty())
    {
        std::cout
            << "\nNo routines detected.\n";

        return;
    }

    for (const auto& routine :
         analysis.routines.routines)
    {
        std::cout
            << "\n"
            << routine.name
            << "  ";

        PrintAddress(
            routine.entryAddress);

        std::cout << " - ";

        PrintAddress(
            routine.endAddress);

        std::cout
            << "  "
            << std::dec
            << routine.InstructionCount()
            << " instructions";

        if (routine.projectEntryPoint)
        {
            std::cout
                << "  [ENTRY]";
        }

        std::cout
            << "\n  callers:      ";

        PrintAddressList(
            routine.callers);

        std::cout
            << "\n  tail callers: ";

        PrintAddressList(
            routine.tailCallers);

        std::cout
            << "\n  callees:      ";

        if (routine.callees.empty())
        {
            std::cout << "none";
        }
        else
        {
            for (std::size_t i = 0;
                 i < routine.callees.size();
                 ++i)
            {
                const auto& callee =
                    routine.callees[i];

                if (i != 0)
                {
                    std::cout << ", ";
                }

                if (callee.type ==
                    atari::RoutineCalleeType::TailJump)
                {
                    std::cout << "JMP ";
                }
                else
                {
                    std::cout << "JSR ";
                }

                PrintAddress(
                    callee.targetAddress);

                if (callee.relocated)
                {
                    std::cout
                        << " [runtime ";

                    PrintAddress(
                        callee.encodedTarget);

                    std::cout << ']';
                }
            }
        }

        std::cout << '\n';
    }
}

void PrintCodeRow(
    const atari::DisassemblyListingRow& row)
{
    std::cout
        << std::left
        << std::setw(12)
        << std::setfill(' ')
        << row.label;

    PrintAddress(
        row.address);

    std::cout << "  ";

    for (const atari::u8 value :
         row.bytes)
    {
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

    for (std::size_t i =
             row.bytes.size();
         i < 3;
         ++i)
    {
        std::cout << "   ";
    }

    std::cout
        << " "
        << std::left
        << std::setw(24)
        << std::setfill(' ')
        << row.instruction;

    if (!row.comment.empty())
    {
        std::cout
            << " ; "
            << row.comment;
    }

    std::cout << '\n';
}

void PrintDataRow(
    const atari::DisassemblyListingRow& row)
{
    std::cout
        << std::left
        << std::setw(12)
        << std::setfill(' ')
        << row.label;

    PrintAddress(
        row.address);

    std::cout << "  ";

    for (const atari::u8 value :
         row.bytes)
    {
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
}

void PrintListing(
    const atari::AnalysisEngineResult& analysis)
{
    std::cout
        << "\n=====================================\n"
        << " Code / Data Listing\n"
        << "=====================================\n\n";

    std::cout
        << "LABEL       ADDRESS  BYTES       INSTRUCTION\n"
        << "---------------------------------------------------------------------\n";

    for (const auto& region :
         analysis.listing.Regions())
    {
        std::cout << '\n';

        if (region.IsCode())
        {
            std::cout
                << "; CODE "
                << AddressToString(
                    region.begin)
                << " - "
                << AddressToString(
                    region.end)
                << '\n';
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
        }

        for (const auto& row :
             region.rows)
        {
            if (row.IsCode())
            {
                PrintCodeRow(
                    row);
            }
            else
            {
                PrintDataRow(
                    row);
            }
        }
    }
}

void PrintRelocationMap(
    const atari::AnalysisEngineResult& analysis)
{
    std::cout
        << "\nRelocation map:\n";

    const auto& relocation =
        analysis.metadata.Relocation();

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

    atari::AnalysisEngine
        analysisEngine;

    const auto analysis =
        analysisEngine.Analyze(
            project);

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
        << analysis.controlFlow.
            entryPoints.size()
        << '\n'
        << "  CFG instructions:         "
        << analysis.cfgInstructionCount
        << '\n'
        << "  Code-island instructions: "
        << analysis.codeIslandInstructionCount
        << '\n'
        << "  Total instructions:       "
        << analysis.TotalInstructionCount()
        << '\n'
        << "  Cross references:         "
        << analysis.CrossReferenceCount()
        << '\n'
        << "  Symbols:                  "
        << analysis.SymbolCount()
        << '\n'
        << "  Code/Data regions:        "
        << analysis.regions.size()
        << '\n'
        << "  Routines:                 "
        << analysis.RoutineCount()
        << '\n'
        << "  Listing rows:             "
        << analysis.ListingRowCount()
        << '\n';

    PrintRelocationMap(
        analysis);

    PrintRoutines(
        analysis);

    PrintListing(
        analysis);

    std::cout
        << "\nPress Enter to exit...";

    std::cin.get();

    return 0;
}