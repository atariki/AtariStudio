#include <cstdint>
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

void PrintAddress(
    atari::u16 address)
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
    // Automatic labels for branch / JMP / JSR targets.
    //
    for (const auto address :
         analysis.targetAddresses)
    {
        labels[address] =
            MakeAutomaticLabel(address);
    }

    //
    // RUNAD gets a special name.
    //
    if (project.RunAddress() != 0)
    {
        labels[project.RunAddress()] =
            "RUN_ENTRY";
    }

    //
    // INITAD gets a special name.
    //
    if (project.InitAddress() != 0 &&
        project.InitAddress() !=
            project.RunAddress())
    {
        labels[project.InitAddress()] =
            "INIT_ENTRY";
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
            segment.name =
                "Reached code";
        }
    }
}

atari::u16 AbsoluteTarget(
    const atari::DisassembledInstruction& instruction)
{
    return static_cast<atari::u16>(
        static_cast<atari::u16>(
            instruction.bytes[1]) |
        (static_cast<atari::u16>(
            instruction.bytes[2]) << 8));
}

atari::u16 RelativeTarget(
    const atari::DisassembledInstruction& instruction)
{
    const auto offset =
        static_cast<std::int8_t>(
            instruction.bytes[1]);

    const std::int32_t target =
        static_cast<std::int32_t>(
            instruction.address) +
        static_cast<std::int32_t>(
            instruction.length) +
        static_cast<std::int32_t>(
            offset);

    return static_cast<atari::u16>(
        target);
}

std::string FormatInstructionWithLabels(
    const atari::DisassembledInstruction& instruction,
    const std::map<atari::u16, std::string>& labels)
{
    atari::u16 target = 0;
    bool hasTarget = false;

    switch (instruction.instruction)
    {
    //
    // Conditional branches.
    //
    case atari::cpu6502::Instruction::BCC:
    case atari::cpu6502::Instruction::BCS:
    case atari::cpu6502::Instruction::BEQ:
    case atari::cpu6502::Instruction::BMI:
    case atari::cpu6502::Instruction::BNE:
    case atari::cpu6502::Instruction::BPL:
    case atari::cpu6502::Instruction::BVC:
    case atari::cpu6502::Instruction::BVS:

        target = RelativeTarget(
            instruction);

        hasTarget = true;
        break;

    //
    // JSR always uses an absolute address.
    //
    case atari::cpu6502::Instruction::JSR:

        target = AbsoluteTarget(
            instruction);

        hasTarget = true;
        break;

    //
    // Only absolute JMP can be resolved directly.
    //
    case atari::cpu6502::Instruction::JMP:

        if (instruction.addressMode ==
            atari::cpu6502::AddressMode::Absolute)
        {
            target = AbsoluteTarget(
                instruction);

            hasTarget = true;
        }

        break;

    default:
        break;
    }

    if (!hasTarget)
    {
        return instruction.text;
    }

    const auto label =
        labels.find(target);

    //
    // Target does not have a known label.
    //
    if (label == labels.end())
    {
        return instruction.text;
    }

    //
    // Replace numeric operand with symbolic label.
    //
    return
        instruction.mnemonic +
        " " +
        label->second;
}

void PrintInstruction(
    const atari::DisassembledInstruction& instruction,
    const std::map<atari::u16, std::string>& labels)
{
    PrintAddress(
        instruction.address);

    std::cout << ": ";

    //
    // Machine-code bytes.
    //
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
    // Align mnemonic column.
    //
    for (std::size_t i = instruction.length;
         i < 3;
         ++i)
    {
        std::cout << "   ";
    }

    std::cout
        << "  "
        << FormatInstructionWithLabels(
            instruction,
            labels)
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
    // Mark code segments discovered by analysis.
    //
    MarkReachedSegments(
        project,
        analysis);

    //
    // Build symbolic labels.
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

    //
    // RUNAD.
    //
    std::cout
        << "\nRUN address:  ";

    if (project.RunAddress() != 0)
    {
        PrintAddress(
            project.RunAddress());
    }
    else
    {
        std::cout
            << "not set";
    }

    //
    // INITAD.
    //
    std::cout
        << "\nINIT address: ";

    if (project.InitAddress() != 0)
    {
        PrintAddress(
            project.InitAddress());
    }
    else
    {
        std::cout
            << "not set";
    }

    //
    // Project statistics.
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
    // Control-flow information.
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

        PrintInstruction(
            instruction,
            labels);
    }

    std::cout
        << "\nPress Enter to exit...";

    std::cin.get();

    return 0;
}