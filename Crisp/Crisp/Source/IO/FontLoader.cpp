#define NOMINMAX
#include "FontLoader.hpp"

#include <cmath>
#include <iostream>

namespace crisp
{
    namespace
    {
        const std::string FontPath = "Resources/Fonts/";

        uint32_t ceilPowerOf2(uint32_t value)
        {
            uint32_t result = value - 1;
            result |= result >> 1;
            result |= result >> 2;
            result |= result >> 4;
            result |= result >> 8;
            result |= result >> 16;
            return result + 1;
        }
    }

    FontLoader::FontLoader()
    {
        if (FT_Init_FreeType(&m_context))
            std::cerr << "Failed to initialize FreeType font library!" << std::endl;
    }

    FontLoader::~FontLoader()
    {
        FT_Done_FreeType(m_context);
    }

    std::pair<std::string, std::unique_ptr<Font>> FontLoader::load(const std::string& fontName, int fontPixelSize)
    {
        FT_Face face;
        if (FT_New_Face(m_context, (FontPath + fontName).c_str(), 0, &face))
        {
            std::cerr << "Failed to create new face: " + fontName << std::endl;
            return { "", nullptr };
        }

        FT_Set_Pixel_Sizes(face, 0, fontPixelSize);

        auto dims = getFontAtlasSize(face);

        uint32_t paddedWidth = ceilPowerOf2(dims.first);
        uint32_t paddedHeight = ceilPowerOf2(dims.second);

        std::unique_ptr<Font> font = std::make_unique<Font>();
        font->width = dims.first;
        font->height = dims.second;
        font->textureData.resize(paddedWidth * paddedHeight);
        loadGlyphs(*font, face, paddedWidth, paddedHeight);

        FT_Done_Face(face);

        std::pair<std::string, std::unique_ptr<Font>> result = { getFontKey(fontName, fontPixelSize), std::move(font) };

        return result;
    }

    std::string FontLoader::getFontKey(const std::string&fontName, int fontSize) const
    {
        return fontName.substr(0, fontName.size() - 4) + "-" + std::to_string(fontSize); //.ttf or .otf
    }

    std::pair<uint32_t, uint32_t> FontLoader::getFontAtlasSize(FT_Face fontFace)
    {
        FT_GlyphSlot glyph = fontFace->glyph;

        uint32_t width = 0;
        uint32_t height = 0;

        for (unsigned int i = CharBegin; i < CharEnd; i++)
        {
            if (FT_Load_Char(fontFace, i, FT_LOAD_RENDER))
            {
                std::cerr << "Failed to load character: " << static_cast<char>(i) << std::endl;
                continue;
            }

            width += glyph->bitmap.width;
            height = std::max(height, glyph->bitmap.rows);
        }

        return { width, height };
    }

    void FontLoader::loadGlyphs(Font& font, FT_Face face, uint32_t paddedWidth, uint32_t paddedHeight)
    {
        FT_GlyphSlot glyph = face->glyph;
        int currX = 0;

        float xOffsetScale = static_cast<float>(font.width) / static_cast<float>(paddedWidth);

        for (int i = CharBegin; i < CharEnd; i++)
        {
            if (FT_Load_Char(face, i, FT_LOAD_RENDER))
            {
                std::cerr << "Failed to load character: " << static_cast<char>(i) << std::endl;
                continue;
            }

            updateTexData(font.textureData, glyph->bitmap.buffer, currX, paddedWidth, glyph->bitmap.width, glyph->bitmap.rows);

            font.glyphs[i - CharBegin].atlasOffsetX = static_cast<float>(currX) / static_cast<float>(font.width) * xOffsetScale;
            font.glyphs[i - CharBegin].advanceX     = static_cast<float>(glyph->advance.x >> 6);
            font.glyphs[i - CharBegin].advanceY     = static_cast<float>(glyph->advance.y >> 6);
            font.glyphs[i - CharBegin].bmpWidth     = static_cast<float>(glyph->bitmap.width);
            font.glyphs[i - CharBegin].bmpHeight    = static_cast<float>(glyph->bitmap.rows);
            font.glyphs[i - CharBegin].bmpLeft      = static_cast<float>(glyph->bitmap_left);
            font.glyphs[i - CharBegin].bmpTop       = static_cast<float>(glyph->bitmap_top);

            currX += glyph->bitmap.width;
        }

        font.width = paddedWidth;
        font.height = paddedHeight;
    }

    void FontLoader::updateTexData(std::vector<unsigned char>& texData, unsigned char* data, int xOffset, uint32_t dstWidth, uint32_t width, uint32_t height)
    {
        for (uint32_t y = 0; y < height; y++)
        {
            for (uint32_t x = 0; x < width; x++)
            {
                uint32_t srcIndex = y * width + x;
                uint32_t dstIndex = y * dstWidth + x + xOffset;

                texData[dstIndex] = data[srcIndex];
            }
        }
    }
}