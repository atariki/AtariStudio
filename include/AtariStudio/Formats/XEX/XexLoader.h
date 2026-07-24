#pragma once

#include <filesystem>

#include <AtariStudio/Core/Result.h>
#include <AtariStudio/Formats/XEX/XexFile.h>

namespace atari
{

    class BinaryReader;

    ///
    /// Загрузчик файлов Atari XEX.
    /// Читает XEX-файл и заполняет объект XexFile.
    ///
    class XexLoader
    {
    public:

        XexLoader() = default;

        [[nodiscard]]
        Result Load(const std::filesystem::path& filename,
            XexFile& file);

    private:

        [[nodiscard]]
        Result ReadSegments(BinaryReader& reader,
            XexFile& file);

        [[nodiscard]]
        Result ReadSegment(BinaryReader& reader,
            XexFile& file);

        void DetectSpecialAddresses(XexFile& file);
    };

} // namespace atari