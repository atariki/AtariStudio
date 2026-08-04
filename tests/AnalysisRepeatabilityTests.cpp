#include <AtariStudio/Core/Project.h>
#include <AtariStudio/Disassembler/AnalysisEngine.h>
#include <AtariStudio/Formats/XEX/XexLoader.h>

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace
{

struct MemoryInvariant
{
    atari::u8 value = 0;
    atari::MemoryType type =
        atari::MemoryType::Unknown;
    bool initialized = false;
    bool readable = false;
    bool writable = false;
};

bool Expect(
    bool condition,
    const char* message)
{
    if (!condition)
    {
        std::cerr
            << "FAILED: "
            << message
            << '\n';
    }

    return condition;
}

std::vector<MemoryInvariant>
CaptureMemoryInvariants(
    const atari::Memory& memory)
{
    std::vector<MemoryInvariant> result;
    result.reserve(atari::MemorySize);

    for (std::uint32_t address = 0;
         address < atari::MemorySize;
         ++address)
    {
        const auto& cell =
            memory.Cell(
                static_cast<atari::u16>(
                    address));

        result.push_back(
            MemoryInvariant{
                cell.value,
                cell.type,
                cell.initialized,
                cell.readable,
                cell.writable});
    }

    return result;
}

bool MatchesMemoryInvariants(
    const atari::Memory& memory,
    const std::vector<MemoryInvariant>& expected)
{
    if (expected.size() !=
        atari::MemorySize)
    {
        return false;
    }

    for (std::uint32_t address = 0;
         address < atari::MemorySize;
         ++address)
    {
        const auto& cell =
            memory.Cell(
                static_cast<atari::u16>(
                    address));

        const auto& invariant =
            expected[address];

        if (cell.value != invariant.value ||
            cell.type != invariant.type ||
            cell.initialized !=
                invariant.initialized ||
            cell.readable !=
                invariant.readable ||
            cell.writable !=
                invariant.writable)
        {
            return false;
        }
    }

    return true;
}

std::vector<bool> CaptureExecutable(
    const atari::Memory& memory)
{
    std::vector<bool> result(
        atari::MemorySize,
        false);

    for (std::uint32_t address = 0;
         address < atari::MemorySize;
         ++address)
    {
        result[address] =
            memory.Cell(
                static_cast<atari::u16>(
                    address))
                .executable;
    }

    return result;
}

bool SameSegments(
    const std::vector<atari::Segment>& left,
    const std::vector<atari::Segment>& right)
{
    if (left.size() != right.size())
    {
        return false;
    }

    for (std::size_t index = 0;
         index < left.size();
         ++index)
    {
        const auto& leftSegment =
            left[index];

        const auto& rightSegment =
            right[index];

        if (leftSegment.begin !=
                rightSegment.begin ||
            leftSegment.end !=
                rightSegment.end ||
            leftSegment.type !=
                rightSegment.type ||
            leftSegment.name !=
                rightSegment.name ||
            leftSegment.overlapping !=
                rightSegment.overlapping)
        {
            return false;
        }
    }

    return true;
}

std::string SerializeListing(
    const atari::DisassemblyListing& listing)
{
    std::ostringstream stream;

    for (const auto& region :
         listing.Regions())
    {
        stream
            << static_cast<int>(
                   region.type)
            << ':'
            << region.begin
            << ':'
            << region.end
            << '\n';

        for (const auto& row :
             region.rows)
        {
            stream
                << static_cast<int>(
                       row.type)
                << ':'
                << row.address
                << ':'
                << row.label
                << ':';

            for (const atari::u8 byte :
                 row.bytes)
            {
                stream
                    << static_cast<unsigned>(
                           byte)
                    << ',';
            }

            stream
                << ':'
                << row.instruction
                << ':'
                << row.comment
                << '\n';
        }
    }

    return stream.str();
}

std::string SerializeDisplayLists(
    const atari::DisplayListAnalysisResult& analysis)
{
    std::ostringstream stream;

    for (const auto entryPoint :
         analysis.entryPoints)
    {
        stream << "entry:" << entryPoint << '\n';
    }

    for (const auto& displayList :
         analysis.displayLists)
    {
        stream
            << "list:"
            << displayList.entryPoint
            << ':'
            << static_cast<int>(
                   displayList.stopReason)
            << ':';

        if (displayList.stopAddress.has_value())
        {
            stream << displayList.stopAddress.value();
        }

        stream << '\n';

        for (const auto& instruction :
             displayList.instructions)
        {
            stream
                << instruction.address
                << ':'
                << static_cast<unsigned>(
                       instruction.opcode)
                << ':'
                << static_cast<unsigned>(
                       instruction.mode)
                << ':'
                << static_cast<unsigned>(
                       instruction.length)
                << ':'
                << static_cast<int>(
                       instruction.kind)
                << ':'
                << instruction.displayListInterrupt
                << ':'
                << instruction.horizontalScroll
                << ':'
                << instruction.verticalScroll
                << ':'
                << instruction.loadMemoryScan
                << ':'
                << instruction.reservedJumpModifier
                << ':'
                << static_cast<unsigned>(
                       instruction.blankScanLines)
                << ':';

            if (instruction.jumpAddress.has_value())
            {
                stream << instruction.jumpAddress.value();
            }

            stream << ':';

            if (instruction.memoryScanAddress.has_value())
            {
                stream
                    << instruction.memoryScanAddress.value();
            }

            stream << '\n';
        }
    }

    stream << "screen:";

    for (const auto address :
         analysis.screenMemoryAddresses)
    {
        stream << address << ',';
    }

    return stream.str();
}

std::string SerializeCharacterSets(
    const atari::CharacterSetAnalysisResult& analysis)
{
    std::ostringstream stream;

    for (const auto& request :
         analysis.requests)
    {
        stream
            << "request:"
            << request.baseAddress
            << ':'
            << static_cast<int>(request.layout)
            << '\n';
    }

    for (const auto& characterSet :
         analysis.characterSets)
    {
        stream
            << "set:"
            << characterSet.baseAddress
            << ':'
            << static_cast<int>(characterSet.layout)
            << ':'
            << characterSet.expectedGlyphCount
            << ':'
            << characterSet.initializedByteCount
            << ':'
            << characterSet.addressSpaceTruncated
            << '\n';

        for (const auto& glyph :
             characterSet.glyphs)
        {
            stream
                << static_cast<unsigned>(glyph.index)
                << ':'
                << glyph.address
                << ':'
                << static_cast<unsigned>(
                       glyph.initializedRowMask)
                << ':';

            for (const auto row : glyph.rows)
            {
                stream
                    << static_cast<unsigned>(row)
                    << ',';
            }

            stream << '\n';
        }
    }

    return stream.str();
}

bool SameAnalysis(
    const atari::AnalysisEngineResult& left,
    const atari::AnalysisEngineResult& right)
{
    return
        left.controlFlow.entryPoints ==
            right.controlFlow.entryPoints &&
        left.controlFlow.instructionAddresses ==
            right.controlFlow.instructionAddresses &&
        left.controlFlow.targetAddresses ==
            right.controlFlow.targetAddresses &&
        left.metadata.Symbols().Symbols() ==
            right.metadata.Symbols().Symbols() &&
        left.cfgInstructionCount ==
            right.cfgInstructionCount &&
        left.codeIslandInstructionCount ==
            right.codeIslandInstructionCount &&
        left.TotalInstructionCount() ==
            right.TotalInstructionCount() &&
        left.DisplayListCount() ==
            right.DisplayListCount() &&
        left.DisplayListInstructionCount() ==
            right.DisplayListInstructionCount() &&
        SerializeDisplayLists(left.displayLists) ==
            SerializeDisplayLists(right.displayLists) &&
        left.CharacterSetCount() ==
            right.CharacterSetCount() &&
        left.CharacterGlyphCount() ==
            right.CharacterGlyphCount() &&
        SerializeCharacterSets(left.characterSets) ==
            SerializeCharacterSets(right.characterSets) &&
        left.CrossReferenceCount() ==
            right.CrossReferenceCount() &&
        left.RoutineCount() ==
            right.RoutineCount() &&
        left.BasicBlockCount() ==
            right.BasicBlockCount() &&
        left.GraphNodeCount() ==
            right.GraphNodeCount() &&
        left.GraphEdgeCount() ==
            right.GraphEdgeCount() &&
        left.NaturalLoopCount() ==
            right.NaturalLoopCount() &&
        left.StructuredExpressionCount() ==
            right.StructuredExpressionCount() &&
        left.StructuredStatementCount() ==
            right.StructuredStatementCount() &&
        left.ListingRowCount() ==
            right.ListingRowCount() &&
        left.structured.GeneratedCode() ==
            right.structured.GeneratedCode() &&
        left.structured.GeneratedTranslationUnit() ==
            right.structured.GeneratedTranslationUnit() &&
        SerializeListing(left.listing) ==
            SerializeListing(right.listing);
}

} // namespace

int main(
    int argc,
    char* argv[])
{
    if (argc != 2)
    {
        std::cerr
            << "Usage: AnalysisRepeatabilityTests <file.xex>\n";

        return 1;
    }

    auto project =
        std::make_unique<atari::Project>();

    atari::XexLoader loader;

    if (!loader.Load(
            std::filesystem::path{
                argv[1]},
            *project))
    {
        std::cerr
            << "XEX load failed: "
            << loader.LastError()
            << '\n';

        return 1;
    }

    const auto originalSegments =
        project->Segments();

    const auto memoryInvariants =
        CaptureMemoryInvariants(
            project->GetMemory());

    for (std::uint32_t address = 0;
         address < atari::MemorySize;
         ++address)
    {
        project->GetMemory().
            Cell(
                static_cast<atari::u16>(
                    address))
            .executable = true;
    }

    atari::AnalysisEngine engine;

    std::unique_ptr<
        atari::AnalysisEngineResult> first{
            new atari::AnalysisEngineResult(
                engine.Analyze(
                    *project))};

    const auto firstExecutable =
        CaptureExecutable(
            project->GetMemory());

    for (std::uint32_t address = 0;
         address < atari::MemorySize;
         ++address)
    {
        auto& executable =
            project->GetMemory().
                Cell(
                    static_cast<atari::u16>(
                        address))
                .executable;

        executable =
            !executable;
    }

    std::unique_ptr<
        atari::AnalysisEngineResult> second{
            new atari::AnalysisEngineResult(
                engine.Analyze(
                    *project))};

    const auto secondExecutable =
        CaptureExecutable(
            project->GetMemory());

    bool passed = true;

    passed &=
        Expect(
            SameAnalysis(
                *first,
                *second),
            "repeated analysis must produce identical results");

    passed &=
        Expect(
            firstExecutable ==
                secondExecutable,
            "repeated analysis must rebuild identical executable flags");

    passed &=
        Expect(
            MatchesMemoryInvariants(
                project->GetMemory(),
                memoryInvariants),
            "analysis must not mutate memory image or non-analysis metadata");

    passed &=
        Expect(
            SameSegments(
                originalSegments,
                project->Segments()),
            "analysis must not mutate XEX segment metadata");

    return passed ? 0 : 1;
}
