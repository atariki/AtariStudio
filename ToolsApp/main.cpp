#include <filesystem>
#include <iomanip>
#include <iostream>
#include <vector>

#include <AtariStudio/Core/Project.h>
#include <AtariStudio/Core/ProjectStatistics.h>
#include <AtariStudio/Disassembler/ControlFlowAnalyzer.h>
#include <AtariStudio/Disassembler/Disassembler.h>
#include <AtariStudio/Formats/XEX/XexLoader.h>

namespace
{

const char* SegmentTypeToString(atari::SegmentType type)
{
    switch (type)
    {
    case atari::SegmentType::Unknown:     return "Unknown";
    case atari::SegmentType::Code:        return "Code";
    case atari::SegmentType::Data:        return "Data";
    case atari::SegmentType::Charset:     return "Charset";
    case atari::SegmentType::Screen:      return "Screen";
    case atari::SegmentType::DisplayList: return "DisplayList";
    case atari::SegmentType::Hardware:    return "Hardware";
    case atari::SegmentType::ZeroPage:    return "ZeroPage";
    case atari::SegmentType::System:      return "System";

    default:
        return "Unknown";
    }
}

void PrintAddress(atari::u16 address)
{
    std::cout
        << '$'
        << std::uppercase
        << std::hex
        << std::setw(4)
        << std::setfill('0')
        << address;
}

void MarkReachedSegments(
    atari::Project& project,
    const atari::ControlFlowAnalysisResult& analysis)
{
    for (auto& segment : project.Segments())
    {
        if (segment.type == atari::SegmentType::System)
        {
            continue;
        }

        bool reached = false;

        for (const auto address :
             analysis.instructionAddresses)
        {
            if (address >= segment.begin &&
                address <= segment.end)
            {
                reached = true;
                break;
            }
        }

        if (!reached)
        {
            continue;
        }

        segment.type = atari::SegmentType::Code;

        if (segment.name.empty())
        {
            segment.name = "Reached code";
        }
    }
}

} // namespace

int main(int argc, char* argv[])
{
    std::cout << "=====================================\n";
    std::cout << " AtariStudio Test Application\n";
    std::cout << "=====================================\n\n";

    if (argc < 2)
    {
        std::cout << "Usage:\n";
        std::cout << "  TestApp <file.xex>\n";
        return 1;
    }

    const std::filesystem::path filename = argv[1];

    atari::Project project;
    atari::XexLoader loader;

    std::cout << "Loading XEX:\n";
    std::cout << filename.string() << "\n\n";

    if (!loader.Load(filename, project))
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
        project.InitAddress() != project.RunAddress())
    {
        entryPoints.push_back(
            project.InitAddress());
    }

    atari::ControlFlowAnalyzer analyzer;

    const auto analysis =
        analyzer.Analyze(
            project.GetMemory(),
            entryPoints);

    //
    // Новый шаг Commit #0026:
    // сегмент становится Code, если анализ потока
    // реально обнаружил в нём инструкции.
    //
    MarkReachedSegments(
        project,
        analysis);

    std::cout << "XEX loaded successfully.\n\n";

    const auto& segments = project.Segments();

    std::cout << "Segments:\n\n";

    for (std::size_t i = 0;
         i < segments.size();
         ++i)
    {
        const auto& segment = segments[i];

        std::cout
            << "  ["
            << std::dec
            << i
            << "] ";

        PrintAddress(segment.begin);

        std::cout << " - ";

        PrintAddress(segment.end);

        std::cout
            << "  "
            << SegmentTypeToString(segment.type)
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
            std::cout << "  [OVERLAP]";
        }

        std::cout << '\n';
    }

    std::cout << "\nRUN address:  ";

    if (project.RunAddress() != 0)
    {
        PrintAddress(project.RunAddress());
    }
    else
    {
        std::cout << "not set";
    }

    std::cout << "\nINIT address: ";

    if (project.InitAddress() != 0)
    {
        PrintAddress(project.InitAddress());
    }
    else
    {
        std::cout << "not set";
    }

    const auto statistics =
        atari::CalculateProjectStatistics(project);

    std::cout << "\n\nXEX Statistics:\n";

    std::cout
        << "  Segments:     "
        << statistics.segmentCount
        << '\n';

    std::cout
        << "  Code:         "
        << statistics.codeSegments
        << '\n';

    std::cout
        << "  System:       "
        << statistics.systemSegments
        << '\n';

    std::cout
        << "  Unknown:      "
        << statistics.unknownSegments
        << '\n';

    std::cout
        << "  Overlapping:  "
        << statistics.overlappingSegments
        << '\n';

    std::cout
        << "  Total bytes:  "
        << statistics.totalBytes
        << '\n';

    std::cout << "\n=====================================\n";
    std::cout << " Control Flow Analysis\n";
    std::cout << "=====================================\n";

    std::cout
        << "Entry points: "
        << analysis.entryPoints.size()
        << '\n';

    for (const auto address :
         analysis.entryPoints)
    {
        std::cout << "  ";
        PrintAddress(address);
        std::cout << '\n';
    }

    std::cout
        << "\nReachable instructions: "
        << std::dec
        << analysis.instructionAddresses.size()
        << "\n\n";

    atari::Disassembler disassembler;

    for (const auto address :
         analysis.instructionAddresses)
    {
        const auto instruction =
            disassembler.Decode(
                project.GetMemory(),
                address);

        PrintAddress(address);

        std::cout << ": ";

        for (std::size_t i = 0;
             i < instruction.length;
             ++i)
        {
            std::cout
                << std::uppercase
                << std::hex
                << std::setw(2)
                << std::setfill('0')
                << static_cast<unsigned>(
                    instruction.bytes[i])
                << ' ';
        }

        for (std::size_t i = instruction.length;
             i < 3;
             ++i)
        {
            std::cout << "   ";
        }

        std::cout
            << "  "
            << instruction.text
            << '\n';
    }

    std::cout << "\nPress Enter to exit...";
    std::cin.get();

    return 0;
}