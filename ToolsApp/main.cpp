#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <AtariStudio/Core/Project.h>
#include <AtariStudio/Core/ProjectStatistics.h>
#include <AtariStudio/Disassembler/ControlFlowAnalyzer.h>
#include <AtariStudio/Disassembler/Disassembler.h>
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

std::string MakeAutomaticLabel(
    atari::u16 address)
{
    std::ostringstream stream;

    stream
        << 'L'
        << std::uppercase
        << std::hex
        << std::setw(4)
        << std::setfill('0')
        << address;

    return stream.str();
}

std::map<atari::u16, std::string> BuildLabels(
    const atari::Project& project,
    const atari::ControlFlowAnalysisResult& analysis)
{
    std::map<atari::u16, std::string> labels;

    //
    // Сначала создаём автоматические метки
    // для всех целей переходов.
    //
    for (const auto address :
         analysis.targetAddresses)
    {
        labels[address] =
            MakeAutomaticLabel(address);
    }

    //
    // RUNAD имеет специальное имя.
    //
    if (project.RunAddress() != 0)
    {
        labels[project.RunAddress()] =
            "RUN_ENTRY";
    }

    //
    // INITAD также получает специальное имя.
    //
    if (project.InitAddress() != 0)
    {
        //
        // Если INITAD совпадает с RUNAD,
        // RUN_ENTRY оставляем приоритетным.
        //
        if (project.InitAddress() !=
            project.RunAddress())
        {
            labels[project.InitAddress()] =
                "INIT_ENTRY";
        }
    }

    return labels;
}

void MarkReachedSegments(
    atari::Project& project,
    const atari::ControlFlowAnalysisResult& analysis)
{
    for (auto& segment :
         project.Segments())
    {
        if (segment.type ==
            atari::SegmentType::System)
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

        segment.type =
            atari::SegmentType::Code;

        if (segment.name.empty())
        {
            segment.name = "Reached code";
        }
    }
}

void PrintInstruction(
    const atari::DisassembledInstruction& instruction)
{
    PrintAddress(instruction.address);

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

    //
    // Выравниваем колонку.
    //
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

    //
    // Entry points.
    //
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

    //
    // Control-flow analysis.
    //
    atari::ControlFlowAnalyzer analyzer;

    const auto analysis =
        analyzer.Analyze(
            project.GetMemory(),
            entryPoints);

    //
    // Кодовые сегменты.
    //
    MarkReachedSegments(
        project,
        analysis);

    //
    // Метки.
    //
    const auto labels =
        BuildLabels(
            project,
            analysis);

    std::cout
        << "XEX loaded successfully.\n\n";

    //
    // Segment list.
    //
    const auto& segments =
        project.Segments();

    std::cout << "Segments:\n\n";

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

        PrintAddress(segment.begin);

        std::cout << " - ";

        PrintAddress(segment.end);

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
            std::cout << "  [OVERLAP]";
        }

        std::cout << '\n';
    }

    //
    // RUNAD / INITAD.
    //
    std::cout << "\nRUN address:  ";

    if (project.RunAddress() != 0)
    {
        PrintAddress(
            project.RunAddress());
    }
    else
    {
        std::cout << "not set";
    }

    std::cout << "\nINIT address: ";

    if (project.InitAddress() != 0)
    {
        PrintAddress(
            project.InitAddress());
    }
    else
    {
        std::cout << "not set";
    }

    //
    // Statistics.
    //
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

    //
    // Analysis information.
    //
    std::cout
        << "\n=====================================\n"
        << " Control Flow Analysis\n"
        << "=====================================\n";

    std::cout
        << "Entry points: "
        << analysis.entryPoints.size()
        << '\n';

    for (const auto address :
         analysis.entryPoints)
    {
        std::cout << "  ";

        PrintAddress(address);

        const auto label =
            labels.find(address);

        if (label != labels.end())
        {
            std::cout
                << "  "
                << label->second;
        }

        std::cout << '\n';
    }

    std::cout
        << "\nReachable instructions: "
        << std::dec
        << analysis.instructionAddresses.size()
        << '\n';

    std::cout
        << "Generated labels:       "
        << labels.size()
        << "\n\n";

    //
    // Reverse-engineering listing.
    //
    atari::Disassembler disassembler;

    for (const auto address :
         analysis.instructionAddresses)
    {
        //
        // Если текущий адрес имеет метку,
        // выводим её перед инструкцией.
        //
        const auto label =
            labels.find(address);

        if (label != labels.end())
        {
            std::cout
                << '\n'
                << label->second
                << ":\n";
        }

        const auto instruction =
            disassembler.Decode(
                project.GetMemory(),
                address);

        PrintInstruction(instruction);
    }

    std::cout
        << "\nPress Enter to exit...";

    std::cin.get();

    return 0;
}