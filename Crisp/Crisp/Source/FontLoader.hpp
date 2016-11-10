#pragma once

#include <string>
#include <memory>
#include <map>
#include <array>

#include <glad/glad.h>
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
    using GlyphArray = std::array<GlyphInfo, 128>;

    struct Font
    {
        FT_Face face;
        GLuint textureId;
        GLsizei width;
        GLsizei height;
        GlyphArray glyphs;
    };

    class FontLoader
    {
    public:
        FontLoader();
        ~FontLoader();

        void load(const std::string& fontName, int fontPixelSize);
        const Font& getFont(const std::string& fontName, int fontPixelSize) const;

    private:
        std::string getFontKey(const std::string& fontName, int fontPixelSize) const;
        std::pair<int, int> getFontAtlasSize(FT_Face fontFace);
        void loadGlyphs(Font& fontName);

        FT_Library m_context;

        std::map<std::string, Font> m_fontMap;
    };
}