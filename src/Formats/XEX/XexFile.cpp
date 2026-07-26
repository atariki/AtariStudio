#include <AtariStudio/Formats/XEX/XexFile.h>

namespace atari
{

void XexFile::Clear()
{
    m_filename.clear();
    m_segments.clear();

    m_runAddress = 0;
    m_initAddress = 0;
}

bool XexFile::Empty() const noexcept
{
    return m_segments.empty();
}

void XexFile::SetFilename(
    const std::filesystem::path& filename)
{
    m_filename = filename;
}

const std::filesystem::path&
XexFile::Filename() const noexcept
{
    return m_filename;
}

void XexFile::AddSegment(
    const XexSegment& segment)
{
    m_segments.push_back(
        segment);
}

const std::vector<XexSegment>&
XexFile::Segments() const noexcept
{
    return m_segments;
}

std::vector<XexSegment>&
XexFile::Segments() noexcept
{
    return m_segments;
}

void XexFile::SetRunAddress(
    std::uint16_t address)
{
    m_runAddress = address;
}

std::uint16_t
XexFile::RunAddress() const noexcept
{
    return m_runAddress;
}

void XexFile::SetInitAddress(
    std::uint16_t address)
{
    m_initAddress = address;
}

std::uint16_t
XexFile::InitAddress() const noexcept
{
    return m_initAddress;
}

std::size_t
XexFile::Size() const noexcept
{
    std::size_t total = 0;

    for (const auto& segment :
         m_segments)
    {
        total +=
            segment.data.size();
    }

    return total;
}

} // namespace atari