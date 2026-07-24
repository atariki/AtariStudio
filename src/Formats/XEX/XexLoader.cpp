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
                throw std::runtime_error("Unexpected end of file.");

            return static_cast<uint16_t>(lo | (hi << 8));
        }

    }

    bool XexLoader::Load(const std::filesystem::path& filename,
        Project& project)
    {
        project.Clear();

        m_lastError.clear();

        std::ifstream file(filename, std::ios::binary);

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
                // Skip any number of FFFF markers
                //
                while (start == 0xFFFF)
                {
                    if (file.peek() == EOF)
                        return true;

                    start = ReadWord(file);
                }

                uint16_t end = ReadWord(file);

                if (end < start)
                {
                    SetError("Invalid XEX segment.");
                    return false;
                }

                Segment segment;
                segment.begin = start;
                segment.end = end;
                segment.type = SegmentType::Unknown;

                project.AddSegment(segment);

                const uint32_t size =
                    static_cast<uint32_t>(end - start + 1);

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
        // Extract RUNAD ($02E0-$02E1)
        //
        if (memory.Cell(0x02E0).initialized &&
            memory.Cell(0x02E1).initialized)
        {
            project.SetRunAddress(memory.Read16(0x02E0));
        }

        //
        // Extract INITAD ($02E2-$02E3)
        //
        if (memory.Cell(0x02E2).initialized &&
            memory.Cell(0x02E3).initialized)
        {
            project.SetInitAddress(memory.Read16(0x02E2));
        }

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

}