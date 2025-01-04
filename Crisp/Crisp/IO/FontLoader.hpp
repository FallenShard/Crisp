#pragma once

#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <ft2build.h>
#include FT_FREETYPE_H

namespace crisp {
struct GlyphInfo {
    float advanceX, advanceY;
    float bmpHeight, bmpWidth;
    float bmpLeft, bmpTop;
    float atlasOffsetX;
};

using GlyphArray = std::array<GlyphInfo, 96>;

struct Font {
    std::vector<unsigned char> textureData;
    uint32_t width;
    uint32_t height;
    GlyphArray glyphs;
    std::string name;
    uint32_t pixelSize;
};

class FontLoader {
public:
    static constexpr unsigned char kCharBegin = 32;
    static constexpr unsigned char kCharEnd = 128;

    FontLoader();
    ~FontLoader();

    FontLoader(const FontLoader&) = delete;
    FontLoader& operator=(const FontLoader&) = delete;

    FontLoader(FontLoader&&) = delete;
    FontLoader& operator=(FontLoader&&) = delete;

    std::unique_ptr<Font> load(const std::filesystem::path& fontPath, int fontPixelSize) const;

private:
    FT_Library m_context{nullptr};
};
} // namespace crisp