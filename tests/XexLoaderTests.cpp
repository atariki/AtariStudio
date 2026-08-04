#include <AtariStudio/Core/Project.h>
#include <AtariStudio/Formats/XEX/XexLoader.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
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

void AppendWord(
    std::vector<std::uint8_t>& bytes,
    std::uint16_t value)
{
    bytes.push_back(
        static_cast<std::uint8_t>(
            value & 0xFF));

    bytes.push_back(
        static_cast<std::uint8_t>(
            value >> 8));
}

void AppendSegment(
    std::vector<std::uint8_t>& bytes,
    std::uint16_t start,
    std::uint16_t end,
    const std::vector<std::uint8_t>& data)
{
    AppendWord(bytes, start);
    AppendWord(bytes, end);
    bytes.insert(
        bytes.end(),
        data.begin(),
        data.end());
}

bool WriteFile(
    const std::filesystem::path& path,
    const std::vector<std::uint8_t>& bytes)
{
    std::ofstream stream(
        path,
        std::ios::binary |
            std::ios::trunc);

    if (!stream)
    {
        return false;
    }

    stream.write(
        reinterpret_cast<const char*>(
            bytes.data()),
        static_cast<std::streamsize>(
            bytes.size()));

    return stream.good();
}

bool IsConsistentLoadedProject(
    const atari::Project& project)
{
    if (project.Segments().empty())
    {
        return false;
    }

    const auto& memory =
        project.GetMemory();

    for (const auto& segment :
         project.Segments())
    {
        if (segment.end < segment.begin ||
            !memory.Cell(segment.begin).initialized ||
            !memory.Cell(segment.end).initialized)
        {
            return false;
        }
    }

    return true;
}

} // namespace

int main()
{
    bool passed = true;

    const auto temporaryDirectory =
        std::filesystem::temp_directory_path();

    const auto validPath =
        temporaryDirectory /
        "atari_studio_xex_loader_valid_test.xex";

    std::vector<std::uint8_t> validBytes;
    AppendWord(validBytes, 0xFFFF);
    AppendWord(validBytes, 0xFFFF);
    AppendSegment(
        validBytes,
        0x2000,
        0x2002,
        {0xAA, 0xBB, 0xCC});
    AppendWord(validBytes, 0xFFFF);
    AppendSegment(
        validBytes,
        0x2001,
        0x2003,
        {0x11, 0x22, 0x33});
    AppendSegment(
        validBytes,
        0x02E0,
        0x02E1,
        {0x01, 0x20});
    AppendSegment(
        validBytes,
        0x02E2,
        0x02E3,
        {0x02, 0x20});

    passed &=
        Expect(
            WriteFile(
                validPath,
                validBytes),
            "valid test XEX must be created");

    atari::Project project;
    atari::XexLoader loader;

    passed &=
        Expect(
            loader.Load(
                validPath,
                project),
            "valid XEX must load");

    const auto& segments =
        project.Segments();

    passed &=
        Expect(
            loader.LastError().empty() &&
            segments.size() == 4 &&
            project.RunAddress() == 0x2001 &&
            project.InitAddress() == 0x2002,
            "loader must recover all segments and vectors");

    passed &=
        Expect(
            project.GetMemory().
                Read8(0x2000) == 0xAA &&
            project.GetMemory().
                Read8(0x2001) == 0x11 &&
            project.GetMemory().
                Read8(0x2002) == 0x22 &&
            project.GetMemory().
                Read8(0x2003) == 0x33,
            "later overlapping segments must win");

    passed &=
        Expect(
            segments.size() == 4 &&
            !segments[0].overlapping &&
            segments[1].overlapping &&
            segments[1].type ==
                atari::SegmentType::Code &&
            segments[2].type ==
                atari::SegmentType::System &&
            segments[2].name == "RUNAD" &&
            segments[3].type ==
                atari::SegmentType::System &&
            segments[3].name == "INITAD",
            "overlap and system/code segment metadata");

    const auto fullRangePath =
        temporaryDirectory /
        "atari_studio_xex_loader_full_range_test.xex";

    std::vector<std::uint8_t> fullRangeBytes;
    fullRangeBytes.reserve(6 + 65536);
    AppendWord(fullRangeBytes, 0xFFFF);
    AppendWord(fullRangeBytes, 0x0000);
    AppendWord(fullRangeBytes, 0xFFFF);

    for (std::uint32_t address = 0;
         address <= 0xFFFF;
         ++address)
    {
        fullRangeBytes.push_back(
            static_cast<std::uint8_t>(
                address & 0xFF));
    }

    passed &=
        Expect(
            WriteFile(
                fullRangePath,
                fullRangeBytes),
            "full-range test XEX must be created");

    passed &=
        Expect(
            loader.Load(
                fullRangePath,
                project) &&
            project.Segments().size() == 1 &&
            project.Segments()[0].Size() == 65536 &&
            project.GetMemory().
                Cell(0x0000).initialized &&
            project.GetMemory().
                Cell(0xFFFF).initialized &&
            project.GetMemory().
                Read8(0x1234) == 0x34 &&
            project.GetMemory().
                Read8(0xFFFF) == 0xFF,
            "a segment covering the complete address space must load");

    struct InvalidCase
    {
        const char* filename;
        std::vector<std::uint8_t> bytes;
        const char* expectedError;
    };

    const std::vector<InvalidCase> invalidCases =
        {
            {
                "atari_studio_xex_loader_empty_test.xex",
                {},
                "XEX file contains no segments."
            },
            {
                "atari_studio_xex_loader_marker_test.xex",
                {0xFF, 0xFF},
                "XEX file contains no segments."
            },
            {
                "atari_studio_xex_loader_header_test.xex",
                {0xFF},
                "Unexpected end of file."
            },
            {
                "atari_studio_xex_loader_end_test.xex",
                {0xFF, 0xFF, 0x00, 0x20},
                "Unexpected end of file."
            },
            {
                "atari_studio_xex_loader_range_test.xex",
                {
                    0xFF, 0xFF,
                    0x01, 0x20,
                    0x00, 0x20
                },
                "Invalid XEX segment."
            },
            {
                "atari_studio_xex_loader_payload_test.xex",
                {
                    0xFF, 0xFF,
                    0x00, 0x20,
                    0x01, 0x20,
                    0xAA
                },
                "Unexpected end of file."
            }
        };

    for (const auto& invalidCase :
         invalidCases)
    {
        const auto path =
            temporaryDirectory /
            invalidCase.filename;

        passed &=
            Expect(
                WriteFile(
                    path,
                    invalidCase.bytes),
                "invalid test XEX must be created");

        project.GetMemory().
            Write8(0x3456, 0xA5);
        project.AddSegment(
            atari::Segment{
                0x3456,
                0x3456});
        project.SetRunAddress(0x3456);
        project.SetInitAddress(0x3456);

        passed &=
            Expect(
                !loader.Load(
                    path,
                    project),
                "malformed XEX must fail");

        passed &=
            Expect(
                loader.LastError() ==
                    invalidCase.expectedError &&
                project.Segments().empty() &&
                project.RunAddress() == 0 &&
                project.InitAddress() == 0 &&
                !project.GetMemory().
                    Cell(0x3456).initialized,
                "failed load must report the error and clear Project");

        std::error_code removeError;
        std::filesystem::remove(
            path,
            removeError);
    }

    const auto randomizedPath =
        temporaryDirectory /
        "atari_studio_xex_loader_randomized_test.xex";

    std::uint32_t randomState =
        0xC001D00D;

    for (std::size_t caseIndex = 0;
         caseIndex < 512;
         ++caseIndex)
    {
        std::vector<std::uint8_t> bytes(
            caseIndex % 65);

        for (auto& byte : bytes)
        {
            randomState =
                randomState * 1664525U +
                1013904223U;

            byte =
                static_cast<std::uint8_t>(
                    randomState >> 24);
        }

        if (bytes.size() >= 2 &&
            caseIndex % 3 == 0)
        {
            bytes[0] = 0xFF;
            bytes[1] = 0xFF;
        }

        passed &=
            Expect(
                WriteFile(
                    randomizedPath,
                    bytes),
                "randomized XEX must be created");

        project.GetMemory().
            Write8(0x3456, 0xA5);
        project.AddSegment(
            atari::Segment{
                0x3456,
                0x3456});
        project.SetRunAddress(0x3456);
        project.SetInitAddress(0x3456);

        const bool loaded =
            loader.Load(
                randomizedPath,
                project);

        if (loaded)
        {
            passed &=
                Expect(
                    loader.LastError().empty() &&
                    IsConsistentLoadedProject(
                        project),
                    "accepted randomized XEX must produce "
                    "a consistent Project");
        }
        else
        {
            passed &=
                Expect(
                    !loader.LastError().empty() &&
                    project.Segments().empty() &&
                    project.RunAddress() == 0 &&
                    project.InitAddress() == 0 &&
                    !project.GetMemory().
                        Cell(0x3456).initialized,
                    "rejected randomized XEX must report "
                    "an error and leave a cleared Project");
        }
    }

    project.GetMemory().
        Write8(0x4567, 0x5A);

    const auto missingPath =
        temporaryDirectory /
        "atari_studio_xex_loader_missing_test.xex";

    std::error_code removeError;
    std::filesystem::remove(
        missingPath,
        removeError);

    passed &=
        Expect(
            !loader.Load(
                missingPath,
                project) &&
            loader.LastError() ==
                "Cannot open file." &&
            project.Segments().empty() &&
            !project.GetMemory().
                Cell(0x4567).initialized,
            "missing file must report an error and clear Project");

    std::filesystem::remove(
        validPath,
        removeError);
    std::filesystem::remove(
        fullRangePath,
        removeError);
    std::filesystem::remove(
        randomizedPath,
        removeError);

    return passed ? 0 : 1;
}
