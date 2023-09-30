#pragma once

#include <filesystem>

namespace crisp {
class OpenEXRReader {
public:
    OpenEXRReader();
    ~OpenEXRReader();

    bool read(const std::filesystem::path& imagePath, std::vector<float>& data, uint32_t& width, uint32_t& height);
};
} // namespace crisp
