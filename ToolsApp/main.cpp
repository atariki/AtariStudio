#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <AtariStudio/Atari/AtariSymbols.h>
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
        << std::right
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

    for (const auto address :
         analysis.targetAddresses)
    {
        labels[address] =
            MakeAutomaticLabel(address);
    }

    if (project.RunAddress() != 0)
    {
        labels[project.RunAddress()] =
            "RUN_ENTRY";
    }

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
    case atari::cpu6502::Instruction::BCC:
    case atari::cpu6502::Instruction::BCS:
    case atari::cpu6502::Instruction::BEQ:
    case atari::cpu6502::Instruction::BMI:
    case atari::cpu6502::Instruction::BNE:
    case atari::cpu6502::Instruction::BPL:
    case atari::cpu6502::Instruction::BVC:
    case atari::cpu6502::Instruction::BVS:

        target =
            RelativeTarget(instruction);

        hasTarget = true;
        break;

    case atari::cpu6502::Instruction::JSR:

        target =
            AbsoluteTarget(instruction);

        hasTarget = true;
        break;

    case atari::cpu6502::Instruction::JMP:

        if (instruction.addressMode ==
            atari::cpu6502::AddressMode::Absolute)
        {
            target =
                AbsoluteTarget(instruction);

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

    const auto iterator =
        labels.find(target);

    if (iterator == labels.end())
    {
        return instruction.text;
    }

    return
        instruction.mnemonic +
        " " +
        iterator->second;
}

std::optional<atari::u16> ReferencedAddress(
    const atari::DisassembledInstruction& instruction)
{
    using AddressMode =
        atari::cpu6502::AddressMode;

    switch (instruction.addressMode)
    {
    case AddressMode::ZeroPage:
    case AddressMode::ZeroPageX:
    case AddressMode::ZeroPageY:
    case AddressMode::IndexedIndirect:
    case AddressMode::IndirectIndexed:

        return static_cast<atari::u16>(
            instruction.bytes[1]);

    case AddressMode::Absolute:
    case AddressMode::AbsoluteX:
    case AddressMode::AbsoluteY:
    case AddressMode::Indirect:

        return AbsoluteTarget(
            instruction);

    case AddressMode::Implied:
    case AddressMode::Accumulator:
    case AddressMode::Immediate:
    case AddressMode::Relative:
    default:

        return std::nullopt;
    }
}

std::string_view MakeAtariComment(
    const atari::DisassembledInstruction& instruction)
{
    const auto address =
        ReferencedAddress(instruction);

    if (!address.has_value())
    {
        return {};
    }

    return atari::AtariSymbols::Find(
        address.value());
}

void PrintInstruction(
    const atari::DisassembledInstruction& instruction,
    const std::map<atari::u16, std::string>& labels)
{
    //
    // LABEL
    //
    const auto label =
        labels.find(instruction.address);

    if (label != labels.end())
    {
        std::cout
            << std::left
            << std::setw(12)
            << std::setfill(' ')
            << label->second;
    }
    else
    {
        std::cout
            << std::left
            << std::setw(12)
            << std::setfill(' ')
            << "";
    }

    //
    // ADDRESS
    //
    PrintAddress(
        instruction.address);

    std::cout << "  ";

    //
    // MACHINE CODE
    //
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

    for (std::size_t i = instruction.length;
         i < 3;
         ++i)
    {
        std::cout << "   ";
    }

    //
    // ASSEMBLY
    //
    const std::string instructionText =
        FormatInstructionWithLabels(
            instruction,
            labels);

    std::cout
        << " "
        << std::left
        << std::setw(24)
        << std::setfill(' ')
        << instructionText;

    //
    // Atari OS / hardware symbol comment.
    //
    const std::string_view comment =
        MakeAtariComment(
            instruction);

    if (!comment.empty())
    {
        std::cout
            << " ; "
            << comment;
    }

    std::cout << '\n';
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

    atari::ControlFlowAnalyzer analyzer;

    const auto analysis =
        analyzer.Analyze(
            project.GetMemory(),
            entryPoints);

    MarkReachedSegments(
        project,
        analysis);

    const auto labels =
        BuildLabels(
            project,
            analysis);

    std::cout
        << "XEX loaded successfully.\n\n";

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
        << '\n'
        << "Generated labels:       "
        << labels.size()
        << "\n\n";

    std::cout
        << "LABEL       ADDRESS  BYTES       INSTRUCTION"
        << "\n"
        << "------------------------------------------------------------"
        << "\n";

    atari::Disassembler disassembler;

    for (const auto address :
         analysis.instructionAddresses)
    {
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