#pragma once

#include <filesystem>
#include <string>

namespace atari
{
    class Project;

    /**
     * @brief Loads Atari XEX executable files into a Project.
     *
     * XEXLoader reads an Atari executable from disk, validates its
     * structure and copies all segments into the project's memory image.
     *
     * The loader also extracts INITAD ($02E2/$02E3) and RUNAD ($02E0/$02E1)
     * when they are present.
     */
    class XexLoader
    {
    public:

        XexLoader() = default;
        ~XexLoader() = default;

        XexLoader(const XexLoader&) = delete;
        XexLoader& operator=(const XexLoader&) = delete;

        /**
         * @brief Load an XEX file.
         *
         * @param filename Path to XEX file.
         * @param project Project that receives loaded memory.
         *
         * @return true on success.
         * @return false on error.
         */
        [[nodiscard]]
        bool Load(const std::filesystem::path& filename,
            Project& project);

        /**
         * @brief Returns textual description of the last error.
         */
        [[nodiscard]]
        const std::string& LastError() const noexcept;

    private:

        void SetError(std::string message);

        std::string m_lastError;
    };

} // namespace atari