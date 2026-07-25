#include <AtariStudio/Formats/XEX/XexLoader.h>

#include <AtariStudio/Core/Memory.h>
#include <AtariStudio/Core/Project.h>
#include <AtariStudio/Core/Segment.h>

#include <fstream>
#include <stdexcept>
#include <utility>

namespace atari
{

    namespace
    {

        [[nodiscard]]
        uint16_t ReadWord(std::istream& stream)
        {
            const int lo = stream.get();
            const int hi = stream.get();

            if (lo == EOF || hi == EOF)
            {
                throw std::runtime_error("Unexpected end of file.");
            }

            return static_cast<uint16_t>(
                static_cast<uint16_t>(lo) |
                (static_cast<uint16_t>(hi) << 8));
        }

        void MarkCodeSegment(
            Project& project,
            uint16_t address,
            const char* name)
        {
            if (address == 0)
            {
                return;
            }

            for (auto& segment : project.Segments())
            {
                if (segment.type == SegmentType::System)
                {
                    continue;
                }

                if (address >= segment.begin &&
                    address <= segment.end)
                {
                    segment.type = SegmentType::Code;

                    if (segment.name.empty())
                    {
                        segment.name = name;
                    }

                    return;
                }
            }
        }

        bool OverlapsExistingSegment(
            const Project& project,
            uint16_t begin,
            uint16_t end)
        {
            for (const auto& existing : project.Segments())
            {
                const bool overlaps =
                    !(end < existing.begin ||
                        begin > existing.end);

                if (overlaps)
                {
                    return true;
                }
            }

            return false;
        }

    } // namespace

    bool XexLoader::Load(
        const std::filesystem::path& filename,
        Project& project)
    {
        project.Clear();
        m_lastError.clear();

        std::ifstream file(
            filename,
            std::ios::binary);

        if (!file)
        {
            SetError("Cannot open file.");
            return false;
        }

        auto& memory = project.GetMemory();

        try
        {
            while (file.peek() != EOF)
            {
                uint16_t start = ReadWord(file);

                //
                // XEX may contain one or more $FFFF markers.
                //
                while (start == 0xFFFF)
                {
                    if (file.peek() == EOF)
                    {
                        return true;
                    }

                    start = ReadWord(file);
                }

                const uint16_t end = ReadWord(file);

                if (end < start)
                {
                    SetError("Invalid XEX segment.");
                    return false;
                }

                Segment segment;

                segment.begin = start;
                segment.end = end;
                segment.type = SegmentType::Unknown;

                //
                // RUNAD
                //
                if (start == 0x02E0 &&
                    end == 0x02E1)
                {
                    segment.type = SegmentType::System;
                    segment.name = "RUNAD";
                }

                //
                // INITAD
                //
                else if (start == 0x02E2 &&
                    end == 0x02E3)
                {
                    segment.type = SegmentType::System;
                    segment.name = "INITAD";
                }

                //
                // Detect overlap with previously loaded segments.
                //
                segment.overlapping =
                    OverlapsExistingSegment(
                        project,
                        segment.begin,
                        segment.end);

                project.AddSegment(segment);

                const uint32_t size =
                    static_cast<uint32_t>(end - start) + 1;

                //
                // Load bytes into Atari memory.
                //
                for (uint32_t i = 0; i < size; ++i)
                {
                    const int value = file.get();

                    if (value == EOF)
                    {
                        SetError("Unexpected end of file.");
                        return false;
                    }

                    memory.Write8(
                        static_cast<uint16_t>(start + i),
                        static_cast<uint8_t>(value));
                }
            }
        }
        catch (const std::exception& ex)
        {
            SetError(ex.what());
            return false;
        }

        //
        // RUNAD
        //
        if (memory.Cell(0x02E0).initialized &&
            memory.Cell(0x02E1).initialized)
        {
            project.SetRunAddress(
                memory.Read16(0x02E0));
        }

        //
        // INITAD
        //
        if (memory.Cell(0x02E2).initialized &&
            memory.Cell(0x02E3).initialized)
        {
            project.SetInitAddress(
                memory.Read16(0x02E2));
        }

        //
        // Mark known executable segments.
        //
        MarkCodeSegment(
            project,
            project.RunAddress(),
            "Main code");

        MarkCodeSegment(
            project,
            project.InitAddress(),
            "Init code");

        return true;
    }

    const std::string& XexLoader::LastError() const noexcept
    {
        return m_lastError;
    }

    void XexLoader::SetError(std::string message)
    {
        m_lastError = std::move(message);
    }

} // namespace atari