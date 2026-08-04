#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>

namespace atari
{

class BinaryReader
{
public:

    BinaryReader() = default;

    explicit BinaryReader(
        const std::filesystem::path& filename);

    [[nodiscard]]
    bool Open(
        const std::filesystem::path& filename);

    void Close() noexcept;

    [[nodiscard]]
    bool IsOpen() const noexcept;

    [[nodiscard]]
    bool Eof() const noexcept;

    [[nodiscard]]
    std::uint8_t ReadU8();

    [[nodiscard]]
    std::uint16_t ReadU16LE();

    [[nodiscard]]
    std::uint32_t ReadU32LE();

    void Read(
        void* buffer,
        std::size_t size);

    void Seek(
        std::size_t offset);

    [[nodiscard]]
    std::size_t Tell() const noexcept;

    [[nodiscard]]
    std::size_t Size() const noexcept;

private:

    std::ifstream m_stream;

    std::size_t m_position = 0;
    std::size_t m_size = 0;
};

} // namespace atari
