#pragma once

#include <filesystem>
#include <string_view>

namespace crisp {

class UniqueTemporaryFile {
public:
    explicit UniqueTemporaryFile(
        std::string_view extension = {}, std::string_view filenamePrefix = {}, std::string_view filenameSuffix = {});
    ~UniqueTemporaryFile();

    UniqueTemporaryFile(const UniqueTemporaryFile&) = delete;
    UniqueTemporaryFile& operator=(const UniqueTemporaryFile&) = delete;

    UniqueTemporaryFile(UniqueTemporaryFile&& other) noexcept;
    UniqueTemporaryFile& operator=(UniqueTemporaryFile&& other) noexcept;

    const std::filesystem::path& getPath() const {
        return m_path;
    }

private:
    void cleanup() noexcept;

    std::filesystem::path m_directory;
    std::filesystem::path m_path;
};

} // namespace crisp
