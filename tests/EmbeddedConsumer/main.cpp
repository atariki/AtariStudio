#include <AtariStudio/Core/Memory.h>
#include <AtariStudio/Disassembler/AnalysisEngine.h>
#include <AtariStudio/Disassembler/Disassembler.h>

#include <memory>

int main()
{
    auto project =
        std::make_unique<atari::Project>();

    project->GetMemory().
        Write8(
            0x2000,
            0x60);

    project->AddSegment(
        atari::Segment{
            0x2000,
            0x2000,
            atari::SegmentType::Code,
            "Embedded consumer"});

    project->SetRunAddress(
        0x2000);

    const auto decoded =
        atari::Disassembler{}.
            Decode(
                project->GetMemory(),
                0x2000);

    const auto result =
        atari::AnalysisEngine{}.
            Analyze(
                *project);

    return
        decoded.text == "RTS" &&
        result.TotalInstructionCount() == 1 &&
        !result.structured.
            generatedTranslationUnit.empty()
            ? 0
            : 1;
}
