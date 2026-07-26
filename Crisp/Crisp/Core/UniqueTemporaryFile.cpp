#include <Crisp/Core/UniqueTemporaryFile.hpp>

#include <cstdint>
#include <format>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace crisp {
namespace {

constexpr int kMaxCreateAttempts = 64;

std::filesystem::path normalizeExtension(const std::string_view extension) {
    if (extension.empty()) {
        return {};
    }

    const std::filesystem::path path(extension);
    if (path.has_parent_path() || path.has_root_path() || extension == "." || extension == "..") {
        throw std::invalid_argument("UniqueTemporaryFile extension must not contain a path");
    }

    std::string normalized(extension);
    if (normalized.front() != '.') {
        normalized.insert(normalized.begin(), '.');
    }
    return normalized;
}

void validateFilenameComponent(const std::string_view value, const std::string_view componentName) {
    const std::filesystem::path path(value);
    if (path.has_parent_path() || path.has_root_path()) {
        throw std::invalid_argument("UniqueTemporaryFile " + std::string(componentName) + " must not contain a path");
    }
}

std::string createRandomToken() {
    std::random_device random;
    return std::format(
        "{:08x}{:08x}{:08x}{:08x}",
        static_cast<uint32_t>(random()),
        static_cast<uint32_t>(random()),
        static_cast<uint32_t>(random()),
        static_cast<uint32_t>(random()));
}

} // namespace

UniqueTemporaryFile::UniqueTemporaryFile(
    const std::string_view extension, const std::string_view filenamePrefix, const std::string_view filenameSuffix) {
    const auto normalizedExtension = normalizeExtension(extension);
    validateFilenameComponent(filenamePrefix, "filename prefix");
    validateFilenameComponent(filenameSuffix, "filename suffix");

    const auto temporaryDirectory = std::filesystem::temp_directory_path();
    for (int attempt = 0; attempt < kMaxCreateAttempts; ++attempt) {
        const auto token = createRandomToken();
        auto directory = temporaryDirectory / ("crisp-" + token);
        std::error_code error;
        if (!std::filesystem::create_directory(directory, error)) {
            if (error && error != std::errc::file_exists) {
                throw std::filesystem::filesystem_error("Failed to create temporary directory", directory, error);
            }
            continue;
        }

        std::filesystem::path filename(std::string(filenamePrefix) + token + std::string(filenameSuffix));
        filename += normalizedExtension;
        auto path = directory / filename;
        std::ofstream file(path, std::ios::binary | std::ios::out);
        if (!file) {
            std::filesystem::remove_all(directory, error);
            throw std::filesystem::filesystem_error(
                "Failed to create temporary file", path, std::make_error_code(std::errc::io_error));
        }

        m_directory = std::move(directory);
        m_path = std::move(path);
        return;
    }

    throw std::filesystem::filesystem_error(
        "Failed to create a unique temporary directory",
        temporaryDirectory,
        std::make_error_code(std::errc::file_exists));
}

UniqueTemporaryFile::~UniqueTemporaryFile() {
    cleanup();
}

UniqueTemporaryFile::UniqueTemporaryFile(UniqueTemporaryFile&& other) noexcept
    : m_directory(std::move(other.m_directory))
    , m_path(std::move(other.m_path)) {
    other.m_directory.clear();
    other.m_path.clear();
}

UniqueTemporaryFile& UniqueTemporaryFile::operator=(UniqueTemporaryFile&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    cleanup();
    m_directory = std::move(other.m_directory);
    m_path = std::move(other.m_path);
    other.m_directory.clear();
    other.m_path.clear();
    return *this;
}

void UniqueTemporaryFile::cleanup() noexcept {
    if (m_directory.empty()) {
        return;
    }

    std::error_code error;
    std::filesystem::remove_all(m_directory, error);
    m_directory.clear();
    m_path.clear();
}

} // namespace crisp
