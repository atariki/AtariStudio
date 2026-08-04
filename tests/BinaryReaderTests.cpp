#include <AtariStudio/Core/BinaryReader.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

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

int RunTests()
{
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path() /
        std::filesystem::path{
            L"Atari Studio BinaryReader \u65E5\u672C\u8A9E"};

    std::error_code removeError;
    std::filesystem::remove_all(
        directory,
        removeError);

    std::filesystem::create_directories(
        directory);

    const std::filesystem::path path =
        directory /
        std::filesystem::path{
            L"input file \u5165\u529B.bin"};

    const std::array<unsigned char, 7> data =
        {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE};

    {
        std::ofstream output(
            path,
            std::ios::binary |
                std::ios::trunc);

        output.write(
            reinterpret_cast<const char*>(
                data.data()),
            static_cast<std::streamsize>(
                data.size()));
    }

    bool passed = true;
    atari::BinaryReader reader;

    passed &=
        Expect(
            reader.Open(path),
            "existing binary file must open");

    passed &=
        Expect(
            reader.IsOpen() &&
            reader.Size() == data.size() &&
            reader.Tell() == 0 &&
            !reader.Eof(),
            "open reader state");

    reader.Read(nullptr, 0);

    passed &=
        Expect(
            reader.Tell() == 0,
            "zero-length read must accept a null buffer");

    bool nullBufferFailed = false;

    try
    {
        reader.Read(nullptr, 1);
    }
    catch (const std::invalid_argument&)
    {
        nullBufferFailed = true;
    }

    passed &=
        Expect(
            nullBufferFailed &&
            reader.Tell() == 0,
            "non-empty read must reject a null buffer");

    passed &=
        Expect(
            reader.ReadU8() == 0x12 &&
            reader.ReadU16LE() == 0x5634 &&
            reader.Tell() == 3,
            "little-endian byte and word reads");

    reader.Seek(3);

    passed &=
        Expect(
            reader.ReadU32LE() == 0xDEBC9A78 &&
            reader.Eof(),
            "little-endian dword read and EOF");

    bool shortReadFailed = false;

    try
    {
        static_cast<void>(
            reader.ReadU8());
    }
    catch (const std::runtime_error&)
    {
        shortReadFailed = true;
    }

    passed &=
        Expect(
            shortReadFailed &&
            reader.Tell() == data.size(),
            "short read must fail without moving position");

    bool invalidSeekFailed = false;

    try
    {
        reader.Seek(
            data.size() + 1);
    }
    catch (const std::out_of_range&)
    {
        invalidSeekFailed = true;
    }

    passed &=
        Expect(
            invalidSeekFailed &&
            reader.Tell() == data.size(),
            "out-of-range seek must fail without moving position");

    reader.Close();

    passed &=
        Expect(
            !reader.IsOpen() &&
            reader.Eof() &&
            reader.Size() == 0 &&
            reader.Tell() == 0,
            "closed reader state");

    bool closedReadFailed = false;

    try
    {
        static_cast<void>(
            reader.ReadU8());
    }
    catch (const std::runtime_error&)
    {
        closedReadFailed = true;
    }

    bool closedSeekFailed = false;

    try
    {
        reader.Seek(0);
    }
    catch (const std::runtime_error&)
    {
        closedSeekFailed = true;
    }

    passed &=
        Expect(
            closedReadFailed &&
            closedSeekFailed,
            "read and seek must reject a closed reader");

    auto missingPath =
        path;

    missingPath +=
        ".missing";

    passed &=
        Expect(
            !reader.Open(
                missingPath) &&
            !reader.IsOpen() &&
            reader.Size() == 0 &&
            reader.Tell() == 0,
            "failed open must leave a closed empty reader");

    bool constructorFailed = false;

    try
    {
        const atari::BinaryReader missingReader(
            missingPath);
        static_cast<void>(missingReader);
    }
    catch (const std::runtime_error&)
    {
        constructorFailed = true;
    }

    passed &=
        Expect(
            constructorFailed,
            "filename constructor must reject a missing file");

    passed &=
        Expect(
            reader.Open(path) &&
            reader.ReadU8() == 0x12,
            "reader must recover after a failed open");

    reader.Close();

    std::filesystem::remove_all(
        directory,
        removeError);

    return passed ? 0 : 1;
}

int main()
{
    try
    {
        return RunTests();
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "UNEXPECTED EXCEPTION: "
            << exception.what()
            << '\n';
    }
    catch (...)
    {
        std::cerr
            << "UNEXPECTED NON-STANDARD EXCEPTION\n";
    }

    return 1;
}
