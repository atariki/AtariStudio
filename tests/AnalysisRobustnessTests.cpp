#include <AtariStudio/Core/Project.h>
#include <AtariStudio/Core/Types.h>
#include <AtariStudio/Disassembler/AnalysisEngine.h>
#include <AtariStudio/Disassembler/Disassembler.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <memory>
#include <vector>

namespace
{

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

std::uint32_t NextRandom(
    std::uint32_t& state) noexcept
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;

    return state;
}

template<typename T>
bool IsSortedUnique(
    const std::vector<T>& values)
{
    return
        std::is_sorted(
            values.begin(),
            values.end()) &&
        std::adjacent_find(
            values.begin(),
            values.end()) ==
            values.end();
}

bool ValidateResult(
    const atari::Project& project,
    const atari::AnalysisEngineResult& result,
    const atari::Segment& segment)
{
    bool passed = true;

    passed &=
        Expect(
            IsSortedUnique(
                result.controlFlow.
                    instructionAddresses),
            "instruction addresses must be sorted and unique");

    passed &=
        Expect(
            IsSortedUnique(
                result.controlFlow.
                    targetAddresses),
            "target addresses must be sorted and unique");

    passed &=
        Expect(
            result.cfgInstructionCount <=
                result.TotalInstructionCount() &&
            result.codeIslandInstructionCount ==
                result.TotalInstructionCount() -
                    result.cfgInstructionCount,
            "analysis counters must remain internally consistent");

    const auto& memory =
        project.GetMemory();

    atari::Disassembler disassembler;

    for (const auto address :
         result.controlFlow.instructionAddresses)
    {
        passed &=
            Expect(
                memory.Cell(address).initialized,
                "every instruction address must be initialized");

        const auto instruction =
            disassembler.Decode(
                memory,
                address);

        const std::uint32_t instructionEnd =
            static_cast<std::uint32_t>(
                address) +
            instruction.length;

        passed &=
            Expect(
                instruction.length >= 1 &&
                instruction.length <= 3 &&
                instructionEnd <=
                    atari::MemorySize,
                "every analyzed instruction must fit the address space");

        for (std::uint32_t byteAddress =
                 address;
             byteAddress < instructionEnd &&
             byteAddress < atari::MemorySize;
             ++byteAddress)
        {
            const auto& cell =
                memory.Cell(
                    static_cast<atari::u16>(
                        byteAddress));

            passed &=
                Expect(
                    cell.initialized &&
                    cell.executable,
                    "all instruction bytes must be initialized and executable");
        }
    }

    std::uint64_t regionBytes = 0;

    for (const auto& region :
         result.regions)
    {
        passed &=
            Expect(
                region.begin >= segment.begin &&
                region.end <= segment.end &&
                region.end >= region.begin,
                "code/data regions must stay inside their source segment");

        regionBytes += region.Size();
    }

    passed &=
        Expect(
            regionBytes == segment.Size(),
            "code/data regions must cover the segment exactly");

    std::size_t listingRows = 0;

    for (const auto& region :
         result.listing.Regions())
    {
        listingRows +=
            region.rows.size();
    }

    passed &=
        Expect(
            result.listing.Regions().size() ==
                result.regions.size() &&
            listingRows ==
                result.ListingRowCount(),
            "listing regions and row counter must agree");

    passed &=
        Expect(
            !result.structured.
                generatedTranslationUnit.empty(),
            "translation-unit generation must succeed for arbitrary input");

    return passed;
}

bool RunScenario(
    std::size_t scenario)
{
    static constexpr std::uint16_t
        StartAddresses[] =
        {
            0x0001,
            0x0100,
            0x2000,
            0x7F00,
            0xFE00,
            0xFF00,
            0xFFFE,
            0xFFFF
        };

    auto project =
        std::make_unique<atari::Project>();

    const std::uint16_t begin =
        StartAddresses[
            scenario %
            std::size(
                StartAddresses)];

    const std::uint32_t remaining =
        atari::MemorySize -
        static_cast<std::uint32_t>(
            begin);

    const std::uint32_t requestedSize =
        1 +
        static_cast<std::uint32_t>(
            (scenario * 37) % 256);

    const std::uint32_t size =
        std::min(
            remaining,
            requestedSize);

    const std::uint16_t end =
        static_cast<std::uint16_t>(
            static_cast<std::uint32_t>(
                begin) +
            size - 1);

    atari::Segment segment;
    segment.begin = begin;
    segment.end = end;
    segment.type =
        atari::SegmentType::Code;
    segment.name =
        "Robustness input";

    project->AddSegment(segment);
    project->SetRunAddress(begin);

    std::uint32_t randomState =
        0x9E3779B9u ^
        static_cast<std::uint32_t>(
            scenario * 0x45D9F3Bu);

    auto& memory =
        project->GetMemory();

    for (std::uint32_t offset = 0;
         offset < size;
         ++offset)
    {
        memory.Write8(
            static_cast<std::uint16_t>(
                static_cast<std::uint32_t>(
                    begin) +
                offset),
            static_cast<std::uint8_t>(
                NextRandom(
                    randomState)));
    }

    static constexpr std::uint8_t
        EntryOpcodes[] =
        {
            0x00, // BRK
            0x20, // JSR absolute
            0x4C, // JMP absolute
            0x60, // RTS
            0xA9, // LDA immediate
            0xD0, // BNE relative
            0xEA  // NOP
        };

    memory.Write8(
        begin,
        EntryOpcodes[
            scenario %
            std::size(
                EntryOpcodes)]);

    const atari::AnalysisEngine engine;

    const auto first =
        engine.Analyze(
            *project);

    bool passed =
        ValidateResult(
            *project,
            first,
            segment);

    const auto firstExecutable =
        [&]()
        {
            std::vector<bool> flags(
                atari::MemorySize,
                false);

            for (std::size_t address = 0;
                 address < atari::MemorySize;
                 ++address)
            {
                flags[address] =
                    memory.Cell(
                        static_cast<atari::u16>(
                            address))
                        .executable;
            }

            return flags;
        }();

    for (std::size_t address = 0;
         address < atari::MemorySize;
         ++address)
    {
        memory.Cell(
            static_cast<atari::u16>(
                address))
            .executable =
                !firstExecutable[address];
    }

    const auto second =
        engine.Analyze(
            *project);

    passed &=
        ValidateResult(
            *project,
            second,
            segment);

    passed &=
        Expect(
            first.controlFlow.
                instructionAddresses ==
                second.controlFlow.
                    instructionAddresses &&
            first.controlFlow.
                targetAddresses ==
                second.controlFlow.
                    targetAddresses &&
            first.structured.generatedCode ==
                second.structured.generatedCode &&
            first.structured.
                generatedTranslationUnit ==
                second.structured.
                    generatedTranslationUnit,
            "arbitrary-input analysis must be repeatable");

    for (std::size_t address = 0;
         address < atari::MemorySize;
         ++address)
    {
        passed &=
            Expect(
                memory.Cell(
                    static_cast<atari::u16>(
                        address))
                    .executable ==
                    firstExecutable[address],
                "repeat analysis must reconstruct executable flags");
    }

    return passed;
}

} // namespace

int main()
{
    bool passed = true;

    for (std::size_t scenario = 0;
         scenario < 64;
         ++scenario)
    {
        passed &=
            RunScenario(
                scenario);
    }

    return passed ? 0 : 1;
}
