#include <AtariStudio/Core/BinaryReader.h>

#include <array>
#include <limits>
#include <stdexcept>

namespace atari
{

BinaryReader::BinaryReader(
    const std::filesystem::path& filename)
{
    if (!Open(filename))
    {
        throw std::runtime_error(
            "Cannot open binary file.");
    }
}

bool BinaryReader::Open(
    const std::filesystem::path& filename)
{
    Close();

    m_stream.open(
        filename,
        std::ios::binary |
            std::ios::ate);

    if (!m_stream)
    {
        Close();
        return false;
    }

    const std::streampos end =
        m_stream.tellg();

    if (end < 0 ||
        static_cast<std::uintmax_t>(end) >
            std::numeric_limits<std::size_t>::max())
    {
        Close();
        return false;
    }

    m_size =
        static_cast<std::size_t>(end);

    m_stream.seekg(
        0,
        std::ios::beg);

    if (!m_stream)
    {
        Close();
        return false;
    }

    return true;
}

void BinaryReader::Close() noexcept
{
    if (m_stream.is_open())
    {
        m_stream.close();
    }

    m_stream.clear();
    m_position = 0;
    m_size = 0;
}

bool BinaryReader::IsOpen() const noexcept
{
    return m_stream.is_open();
}

bool BinaryReader::Eof() const noexcept
{
    return
        !IsOpen() ||
        m_position >= m_size;
}

std::uint8_t BinaryReader::ReadU8()
{
    std::uint8_t value = 0;

    Read(
        &value,
        sizeof(value));

    return value;
}

std::uint16_t BinaryReader::ReadU16LE()
{
    std::array<std::uint8_t, 2> bytes{};

    Read(
        bytes.data(),
        bytes.size());

    return
        static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(
                bytes[0]) |
            (static_cast<std::uint16_t>(
                bytes[1]) << 8));
}

std::uint32_t BinaryReader::ReadU32LE()
{
    std::array<std::uint8_t, 4> bytes{};

    Read(
        bytes.data(),
        bytes.size());

    return
        static_cast<std::uint32_t>(
            bytes[0]) |
        (static_cast<std::uint32_t>(
            bytes[1]) << 8) |
        (static_cast<std::uint32_t>(
            bytes[2]) << 16) |
        (static_cast<std::uint32_t>(
            bytes[3]) << 24);
}

void BinaryReader::Read(
    void* buffer,
    std::size_t size)
{
    if (!IsOpen())
    {
        throw std::runtime_error(
            "Binary file is not open.");
    }

    if (size != 0 &&
        buffer == nullptr)
    {
        throw std::invalid_argument(
            "Binary read buffer is null.");
    }

    if (size > m_size - m_position)
    {
        throw std::runtime_error(
            "Unexpected end of binary file.");
    }

    if (size == 0)
    {
        return;
    }

    if (size >
        static_cast<std::size_t>(
            std::numeric_limits<
                std::streamsize>::max()))
    {
        throw std::length_error(
            "Binary read is too large.");
    }

    m_stream.read(
        static_cast<char*>(buffer),
        static_cast<std::streamsize>(
            size));

    if (!m_stream)
    {
        throw std::runtime_error(
            "Cannot read binary file.");
    }

    m_position += size;
}

void BinaryReader::Seek(
    std::size_t offset)
{
    if (!IsOpen())
    {
        throw std::runtime_error(
            "Binary file is not open.");
    }

    if (offset > m_size)
    {
        throw std::out_of_range(
            "Binary seek is outside the file.");
    }

    if (offset >
        static_cast<std::size_t>(
            std::numeric_limits<
                std::streamoff>::max()))
    {
        throw std::out_of_range(
            "Binary seek offset is too large.");
    }

    m_stream.clear();
    m_stream.seekg(
        static_cast<std::streamoff>(
            offset),
        std::ios::beg);

    if (!m_stream)
    {
        throw std::runtime_error(
            "Cannot seek binary file.");
    }

    m_position = offset;
}

std::size_t BinaryReader::Tell() const noexcept
{
    return m_position;
}

std::size_t BinaryReader::Size() const noexcept
{
    return m_size;
}

} // namespace atari
