#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace atari
{

struct XexSegment
{
    std::uint16_t start = 0;
    std::uint16_t end = 0;

    std::vector<std::uint8_t> data;

    [[nodiscard]]
    std::size_t Size() const noexcept
    {
        return data.size();
    }
};

class XexFile
{
public:

    XexFile() = default;

    void Clear();

    [[nodiscard]]
    bool Empty() const noexcept;

    void SetFilename(
        const std::filesystem::path& filename);

    [[nodiscard]]
    const std::filesystem::path& Filename() const noexcept;

    void AddSegment(
        const XexSegment& segment);

    [[nodiscard]]
    const std::vector<XexSegment>& Segments() const noexcept;

    [[nodiscard]]
    std::vector<XexSegment>& Segments() noexcept;

    void SetRunAddress(
        std::uint16_t address);

    [[nodiscard]]
    std::uint16_t RunAddress() const noexcept;

    void SetInitAddress(
        std::uint16_t address);

    [[nodiscard]]
    std::uint16_t InitAddress() const noexcept;

    [[nodiscard]]
    std::size_t Size() const noexcept;

private:

    std::filesystem::path m_filename;

    std::vector<XexSegment> m_segments;

    std::uint16_t m_runAddress = 0;
    std::uint16_t m_initAddress = 0;
};

} // namespace atari