#pragma once

#include <string>
#include <memory>
#include <map>
#include <vector>
#include <array>

#include <ft2build.h>
#include FT_FREETYPE_H

namespace crisp
{
    struct GlyphInfo
    {
        float advanceX, advanceY;
        float bmpHeight, bmpWidth;
        float bmpLeft, bmpTop;
        float atlasOffsetX;
    };
    using GlyphArray = std::array<GlyphInfo, 96>;

    struct Font
    {
        std::vector<unsigned char> textureData;
        uint32_t width;
        uint32_t height;
        GlyphArray glyphs;
    };

    class FontLoader
    {
    public:
        static constexpr unsigned char CharBegin = 32;
        static constexpr unsigned char CharEnd   = 128;

        FontLoader();
        ~FontLoader();

        std::pair<std::string, std::unique_ptr<Font>> load(const std::string& fontName, int fontPixelSize);
        std::string getFontKey(const std::string& fontName, int fontPixelSize) const;
            
    private:
        std::pair<uint32_t, uint32_t> getFontAtlasSize(FT_Face fontFace);
        void loadGlyphs(Font& fontName, FT_Face face, uint32_t paddedWidth, uint32_t paddedHeight);
        void updateTexData(std::vector<unsigned char>& texData, unsigned char* data, int x, uint32_t dstWidth, uint32_t width, uint32_t height);

        FT_Library m_context;
    };
}