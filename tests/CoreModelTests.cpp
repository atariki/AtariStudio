#include <AtariStudio/Core/Address.h>
#include <AtariStudio/Core/Project.h>
#include <AtariStudio/Core/ProjectStatistics.h>
#include <AtariStudio/Core/Result.h>
#include <AtariStudio/Core/Segment.h>
#include <AtariStudio/Formats/XEX/XexFile.h>

#include <filesystem>
#include <iostream>
#include <string>

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

} // namespace

int main()
{
    bool passed = true;

    passed &=
        Expect(
            atari::Address{}.Value() == 0 &&
            atari::Address{0xABCD}.Value() ==
                0xABCD,
            "Address must preserve its 16-bit value");

    const atari::Segment fullRange{
        0x0000,
        0xFFFF};

    const atari::Segment reversedRange{
        0x2000,
        0x1FFF};

    passed &=
        Expect(
            fullRange.Size() == 65536 &&
            reversedRange.Size() == 0,
            "Segment size must handle full and reversed ranges");

    atari::Project project;

    project.AddSegment(
        atari::Segment{
            0x0000,
            0x0000,
            atari::SegmentType::Unknown,
            {},
            true});

    project.AddSegment(
        atari::Segment{
            0x0010,
            0x0011,
            atari::SegmentType::Code});

    project.AddSegment(
        atari::Segment{
            0x0020,
            0x0022,
            atari::SegmentType::Data});

    project.AddSegment(
        atari::Segment{
            0x0030,
            0x0033,
            atari::SegmentType::Charset});

    project.AddSegment(
        atari::Segment{
            0x0040,
            0x0044,
            atari::SegmentType::Screen});

    project.AddSegment(
        atari::Segment{
            0x0050,
            0x0055,
            atari::SegmentType::DisplayList});

    project.AddSegment(
        atari::Segment{
            0x0060,
            0x0066,
            atari::SegmentType::Hardware});

    project.AddSegment(
        atari::Segment{
            0x0070,
            0x0077,
            atari::SegmentType::ZeroPage});

    project.AddSegment(
        atari::Segment{
            0x0000,
            0xFFFF,
            atari::SegmentType::System,
            "Complete image",
            true});

    const auto statistics =
        atari::CalculateProjectStatistics(
            project);

    passed &=
        Expect(
            statistics.segmentCount == 9 &&
            statistics.codeSegments == 1 &&
            statistics.dataSegments == 1 &&
            statistics.systemSegments == 1 &&
            statistics.unknownSegments == 1 &&
            statistics.charsetSegments == 1 &&
            statistics.screenSegments == 1 &&
            statistics.displayListSegments == 1 &&
            statistics.hardwareSegments == 1 &&
            statistics.zeroPageSegments == 1 &&
            statistics.overlappingSegments == 2 &&
            statistics.totalBytes == 65572,
            "project statistics must count every segment type and byte");

    atari::XexFile xexFile;

    passed &=
        Expect(
            xexFile.Empty() &&
            xexFile.Filename().empty() &&
            xexFile.Size() == 0 &&
            xexFile.RunAddress() == 0 &&
            xexFile.InitAddress() == 0,
            "new XexFile state");

    xexFile.SetFilename(
        std::filesystem::path{
            "sample.xex"});

    xexFile.AddSegment(
        atari::XexSegment{
            0x1000,
            0x1002,
            {0xAA, 0xBB, 0xCC}});

    xexFile.AddSegment(
        atari::XexSegment{
            0x2000,
            0x2001,
            {0x11, 0x22}});

    xexFile.SetRunAddress(0x1000);
    xexFile.SetInitAddress(0x2000);

    passed &=
        Expect(
            !xexFile.Empty() &&
            xexFile.Filename() ==
                std::filesystem::path{
                    "sample.xex"} &&
            xexFile.Segments().size() == 2 &&
            xexFile.Segments()[0].Size() == 3 &&
            xexFile.Size() == 5 &&
            xexFile.RunAddress() == 0x1000 &&
            xexFile.InitAddress() == 0x2000,
            "populated XexFile state");

    xexFile.Segments()[0].data[1] =
        0x5A;

    passed &=
        Expect(
            xexFile.Segments()[0].data[1] ==
                0x5A,
            "mutable XexFile segment access");

    xexFile.Clear();

    passed &=
        Expect(
            xexFile.Empty() &&
            xexFile.Filename().empty() &&
            xexFile.Size() == 0 &&
            xexFile.RunAddress() == 0 &&
            xexFile.InitAddress() == 0,
            "XexFile Clear must reset all observable state");

    const atari::Result defaultResult;
    const auto success =
        atari::Result::Success();
    const auto failure =
        atari::Result::Failure(
            "failure message");

    passed &=
        Expect(
            defaultResult.Succeeded() &&
            static_cast<bool>(
                defaultResult) &&
            success.Succeeded() &&
            !success.Failed() &&
            success.Message().empty() &&
            failure.Failed() &&
            !failure.Succeeded() &&
            !static_cast<bool>(
                failure) &&
            failure.Message() ==
                "failure message",
            "Result success and failure contracts");

    return passed ? 0 : 1;
}
