#define NOMINMAX
#include "FontLoader.hpp"

#include <cmath>
#include <iostream>

namespace crisp
{
    namespace
    {
        const std::string FontPath = "Resources/Fonts/";
    }

    FontLoader::FontLoader()
    {
        if (FT_Init_FreeType(&m_context))
            std::cerr << "Failed to initialize FreeType font library!" << std::endl;
    }

    FontLoader::~FontLoader()
    {
        for (auto& font : m_fontMap)
            FT_Done_Face(font.second.face);
        FT_Done_FreeType(m_context);
    }

    void FontLoader::load(const std::string& fontName, int fontPixelSize)
    {
        FT_Face face;
        if (FT_New_Face(m_context, (FontPath + fontName).c_str(), 0, &face))
        {
            std::cerr << "Failed to create new face: " + fontName << std::endl;
            return;
        }

        FT_Set_Pixel_Sizes(face, 0, fontPixelSize);

        auto dims = getFontAtlasSize(face);

        GLuint fontTex;
        glGenTextures(1, &fontTex);
        glBindTexture(GL_TEXTURE_2D, fontTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, dims.first, dims.second, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        Font font;
        font.face = face;
        font.width = dims.first;
        font.height = dims.second;
        font.textureId = fontTex;

        std::string fontKeyName = getFontKey(fontName, fontPixelSize);

        loadGlyphs(font);
        m_fontMap[fontKeyName] = font;
    }

    const Font& FontLoader::getFont(const std::string& fontName, int fontSize) const
    {
        return m_fontMap.at(getFontKey(fontName, fontSize));
    }

    std::string FontLoader::getFontKey(const std::string&fontName, int fontSize) const
    {
        return fontName.substr(0, fontName.size() - 4) + "-" + std::to_string(fontSize); //.ttf or .otf
    }

    std::pair<int, int> FontLoader::getFontAtlasSize(FT_Face fontFace)
    {
        FT_GlyphSlot glyph = fontFace->glyph;

        unsigned int width = 0;
        unsigned int height = 0;

        for (int i = 0; i < 128; i++)
        {
            if (FT_Load_Char(fontFace, i, FT_LOAD_RENDER))
            {
                std::cerr << "Failed to load character: " << static_cast<char>(i) << std::endl;
                continue;
            }

            width += glyph->bitmap.width;
            height = std::max(height, glyph->bitmap.rows);
        }

        return std::make_pair(static_cast<int>(width), static_cast<int>(height));
    }

    void FontLoader::loadGlyphs(Font& font)
    {
        FT_GlyphSlot glyph = font.face->glyph;
        int currX = 0;

        for (int i = 0; i < 128; i++)
        {
            if (FT_Load_Char(font.face, i, FT_LOAD_RENDER))
            {
                std::cerr << "Failed to load character: " << static_cast<char>(i) << std::endl;
                continue;
            }

            glTexSubImage2D(GL_TEXTURE_2D, 0, currX, 0, glyph->bitmap.width, glyph->bitmap.rows, GL_RED, GL_UNSIGNED_BYTE, glyph->bitmap.buffer);

            font.glyphs[i].atlasOffsetX = static_cast<float>(currX) / static_cast<float>(font.width);
            font.glyphs[i].advanceX = (float)(glyph->advance.x >> 6);
            font.glyphs[i].advanceY = (float)(glyph->advance.y >> 6);
            font.glyphs[i].bmpWidth = (float)(glyph->bitmap.width);
            font.glyphs[i].bmpHeight = (float)(glyph->bitmap.rows);
            font.glyphs[i].bmpLeft = (float)(glyph->bitmap_left);
            font.glyphs[i].bmpTop = (float)(glyph->bitmap_top);

            currX += glyph->bitmap.width;
        }
    }
}