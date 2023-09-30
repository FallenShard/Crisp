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

    std::unique_ptr<Font> load(const std::filesystem::path& fontPath, int fontPixelSize) const;

private:
    std::pair<uint32_t, uint32_t> getFontAtlasSize(FT_Face fontFace) const;
    void loadGlyphs(Font& fontName, FT_Face face, uint32_t paddedWidth, uint32_t paddedHeight) const;
    void updateTexData(
        std::vector<unsigned char>& texData,
        unsigned char* data,
        int x,
        uint32_t dstWidth,
        uint32_t width,
        uint32_t height) const;

    FT_Library m_context;
};
} // namespace crisp