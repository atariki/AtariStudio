#include <algorithm>
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
#include <AtariStudio/Disassembler/CodeDataAnalyzer.h>
#include <AtariStudio/Disassembler/CodeIslandAnalyzer.h>
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
            MakeAutomaticLabel(
                address);
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
            RelativeTarget(
                instruction);

        hasTarget = true;
        break;

    case atari::cpu6502::Instruction::JSR:

        target =
            AbsoluteTarget(
                instruction);

        hasTarget = true;
        break;

    case atari::cpu6502::Instruction::JMP:

        if (instruction.addressMode ==
            atari::cpu6502::AddressMode::Absolute)
        {
            target =
                AbsoluteTarget(
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

    const auto iterator =
        labels.find(
            target);

    if (iterator ==
        labels.end())
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
        ReferencedAddress(
            instruction);

    if (!address.has_value())
    {
        return {};
    }

    return atari::AtariSymbols::Find(
        address.value());
}

std::string MakeRelocationTargetComment(
    const atari::DisassembledInstruction& instruction,
    const atari::ControlFlowAnalysisResult& analysis,
    const std::map<atari::u16, std::string>& labels)
{
    bool isControlFlowTarget = false;

    switch (instruction.instruction)
    {
    case atari::cpu6502::Instruction::JSR:

        isControlFlowTarget = true;
        break;

    case atari::cpu6502::Instruction::JMP:

        isControlFlowTarget =
            instruction.addressMode ==
            atari::cpu6502::AddressMode::Absolute;

        break;

    default:
        break;
    }

    if (!isControlFlowTarget)
    {
        return {};
    }

    const atari::u16 runtimeTarget =
        AbsoluteTarget(
            instruction);

    const auto source =
        analysis.relocation.ResolveDestination(
            runtimeTarget);

    if (!source.has_value())
    {
        return {};
    }

    std::string result =
        "-> ";

    const auto label =
        labels.find(
            source.value());

    if (label != labels.end())
    {
        result +=
            label->second;
    }
    else
    {
        result +=
            AddressToString(
                source.value());
    }

    return result;
}

std::string MakeRuntimeAddressComment(
    const atari::DisassembledInstruction& instruction,
    const atari::ControlFlowAnalysisResult& analysis,
    const std::map<atari::u16, std::string>& labels)
{
    //
    // Чтобы не засорять каждую строку,
    // runtime-адрес показываем только
    // у помеченных точек входа/перехода.
    //
    if (labels.find(
            instruction.address) ==
        labels.end())
    {
        return {};
    }

    const auto runtimeAddress =
        analysis.relocation.ResolveSource(
            instruction.address);

    if (!runtimeAddress.has_value())
    {
        return {};
    }

    return
        "runtime " +
        AddressToString(
            runtimeAddress.value());
}

void AppendComment(
    std::vector<std::string>& comments,
    std::string comment)
{
    if (!comment.empty())
    {
        comments.push_back(
            std::move(comment));
    }
}

void PrintInstruction(
    const atari::DisassembledInstruction& instruction,
    const atari::ControlFlowAnalysisResult& analysis,
    const std::map<atari::u16, std::string>& labels)
{
    //
    // LABEL
    //
    const auto label =
        labels.find(
            instruction.address);

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

    for (std::size_t i =
             instruction.length;
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
    // COMMENTS / SYMBOLIC INFORMATION
    //
    std::vector<std::string> comments;

    const std::string_view atariComment =
        MakeAtariComment(
            instruction);

    if (!atariComment.empty())
    {
        comments.emplace_back(
            atariComment);
    }

    AppendComment(
        comments,
        MakeRelocationTargetComment(
            instruction,
            analysis,
            labels));

    AppendComment(
        comments,
        MakeRuntimeAddressComment(
            instruction,
            analysis,
            labels));

    if (!comments.empty())
    {
        std::cout << " ; ";

        for (std::size_t i = 0;
             i < comments.size();
             ++i)
        {
            if (i != 0)
            {
                std::cout << " | ";
            }

            std::cout
                << comments[i];
        }
    }

    std::cout << '\n';
}

void PrintDataRegion(
    const atari::Memory& memory,
    const atari::CodeDataRegion& region,
    const std::map<atari::u16, std::string>& labels)
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

        const auto label =
            labels.find(
                currentAddress);

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
    const std::map<atari::u16, std::string>& labels)
{
    atari::Disassembler disassembler;

    const auto beginIterator =
        std::lower_bound(
            analysis.instructionAddresses.begin(),
            analysis.instructionAddresses.end(),
            region.begin);

    for (auto iterator =
             beginIterator;
         iterator !=
             analysis.instructionAddresses.end();
         ++iterator)
    {
        const atari::u16 address =
            *iterator;

        if (address >
            region.end)
        {
            break;
        }

        const auto instruction =
            disassembler.Decode(
                memory,
                address);

        PrintInstruction(
            instruction,
            analysis,
            labels);
    }
}

void PrintRelocationMap(
    const atari::ControlFlowAnalysisResult& analysis)
{
    std::cout
        << "\nRelocation map:\n";

    if (analysis.relocation.ranges.empty())
    {
        std::cout
            << "  none\n";

        return;
    }

    for (const auto& range :
         analysis.relocation.ranges)
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
    const std::map<atari::u16, std::string>& labels)
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
                labels);
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
                labels);
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

    //
    // Phase 1:
    // normal CFG + relocation.
    //
    atari::ControlFlowAnalyzer
        controlFlowAnalyzer;

    auto analysis =
        controlFlowAnalyzer.Analyze(
            project.GetMemory(),
            entryPoints);

    const std::size_t normalInstructions =
        analysis.instructionAddresses.size();

    //
    // Phase 2:
    // disconnected code islands.
    //
    atari::CodeIslandAnalyzer
        codeIslandAnalyzer;

    codeIslandAnalyzer.Analyze(
        project,
        analysis);

    const std::size_t islandInstructions =
        analysis.instructionAddresses.size() -
        normalInstructions;

    //
    // Labels after all analysis phases.
    //
    const auto labels =
        BuildLabels(
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
        << "  Generated labels:         "
        << labels.size()
        << '\n';

    PrintRelocationMap(
        analysis);

    PrintMixedListing(
        project,
        analysis,
        labels);

    std::cout
        << "\nPress Enter to exit...";

    std::cin.get();

    return 0;
}