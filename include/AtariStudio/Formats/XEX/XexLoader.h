#pragma once

#include <filesystem>
#include <string>

namespace atari
{

    class Project;

    ///
    /// Загрузчик Atari XEX-файлов.
    /// Загружает сегменты непосредственно в Project/Memory.
    ///
    class XexLoader
    {
    public:

        XexLoader() = default;

        [[nodiscard]]
        bool Load(
            const std::filesystem::path& filename,
            Project& project);

        [[nodiscard]]
        const std::string& LastError() const noexcept;

    private:

        void SetError(std::string message);

        std::string m_lastError;
    };

} // namespace atari