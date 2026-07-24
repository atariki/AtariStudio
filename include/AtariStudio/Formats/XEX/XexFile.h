#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace atari
{

    struct Segment
    {
        uint16_t start = 0;
        uint16_t end = 0;

        std::vector<uint8_t> data;

        [[nodiscard]]
        uint16_t Size() const noexcept
        {
            return static_cast<uint16_t>(data.size());
        }
    };

    class XexFile
    {
    public:

        XexFile() = default;

        void Clear();

        [[nodiscard]]
        bool Empty() const noexcept;

        void SetFilename(const std::filesystem::path& filename);

        [[nodiscard]]
        const std::filesystem::path& Filename() const noexcept;

        void AddSegment(const Segment& segment);

        [[nodiscard]]
        const std::vector<Segment>& Segments() const noexcept;

        [[nodiscard]]
        std::vector<Segment>& Segments() noexcept;

        void SetRunAddress(uint16_t address);

        [[nodiscard]]
        uint16_t RunAddress() const noexcept;

        void SetInitAddress(uint16_t address);

        [[nodiscard]]
        uint16_t InitAddress() const noexcept;

        [[nodiscard]]
        size_t Size() const noexcept;

    private:

        std::filesystem::path m_filename;

        std::vector<Segment> m_segments;

        uint16_t m_runAddress = 0;

        uint16_t m_initAddress = 0;
    };

}