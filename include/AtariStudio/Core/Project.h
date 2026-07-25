#pragma once

#include <vector>

#include <AtariStudio/Core/Memory.h>
#include <AtariStudio/Core/Segment.h>

namespace atari
{

    class Project
    {
    public:

        Project() = default;

        //----- Memory -------------------------------------------------------------

        [[nodiscard]]
        Memory& GetMemory() noexcept
        {
            return m_memory;
        }

        [[nodiscard]]
        const Memory& GetMemory() const noexcept
        {
            return m_memory;
        }

        //----- Segments -----------------------------------------------------------

        void AddSegment(const Segment& segment)
        {
            m_segments.push_back(segment);
        }

        [[nodiscard]]
        const std::vector<Segment>& Segments() const noexcept
        {
            return m_segments;
        }

        [[nodiscard]]
        std::vector<Segment>& Segments() noexcept
        {
            return m_segments;
        }

        //----- RUNAD --------------------------------------------------------------

        void SetRunAddress(uint16_t address) noexcept
        {
            m_runAddress = address;
        }

        [[nodiscard]]
        uint16_t RunAddress() const noexcept
        {
            return m_runAddress;
        }

        //----- INITAD -------------------------------------------------------------

        void SetInitAddress(uint16_t address) noexcept
        {
            m_initAddress = address;
        }

        [[nodiscard]]
        uint16_t InitAddress() const noexcept
        {
            return m_initAddress;
        }

        //----- Project ------------------------------------------------------------

        void Clear()
        {
            m_memory.Clear();
            m_segments.clear();
            m_runAddress = 0;
            m_initAddress = 0;
        }

    private:

        Memory m_memory;

        std::vector<Segment> m_segments;

        uint16_t m_runAddress = 0;
        uint16_t m_initAddress = 0;
    };

} // namespace atari