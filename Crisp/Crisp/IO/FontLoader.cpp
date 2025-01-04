#define NOMINMAX
#include <Crisp/Io/FontLoader.hpp>

#include <Crisp/Core/Logger.hpp>

namespace crisp {
namespace {
CRISP_MAKE_LOGGER_ST("FontLoader");

constexpr uint32_t kPadding = 0;

uint32_t ceilPowerOf2(uint32_t value) {
    uint32_t result = value - 1;
    result |= result >> 1;
    result |= result >> 2;
    result |= result >> 4;
    result |= result >> 8;
    result |= result >> 16;
    return result + 1;
}

std::pair<uint32_t, uint32_t> getFontAtlasSize(FT_Face fontFace) {
    FT_GlyphSlot glyph = fontFace->glyph;

    uint32_t width = 0;
    uint32_t height = 0;

    for (unsigned int i = FontLoader::kCharBegin; i < FontLoader::kCharEnd; i++) {
        if (FT_Load_Char(fontFace, i, FT_LOAD_RENDER)) {
            CRISP_LOGE("Failed to load character: '{}'", static_cast<char>(i));
            continue;
        }

        width += glyph->bitmap.width + kPadding;
        height = std::max(height, glyph->bitmap.rows);
    }

    return {width, height};
}

void updateTexData(
    std::vector<unsigned char>& texData,
    const unsigned char* data,
    const uint32_t xOffset,
    const uint32_t dstWidth,
    const uint32_t width,
    const uint32_t height) {
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint32_t srcIndex = y * width + x;
            uint32_t dstIndex = y * dstWidth + x + xOffset;

            texData[dstIndex] = data[srcIndex]; // NOLINT
        }
    }
}

void loadGlyphs(Font& font, const FT_Face face, const uint32_t paddedWidth, const uint32_t paddedHeight) {
    FT_GlyphSlot glyph = face->glyph;
    uint32_t currX = 0;

    float xOffsetScale = static_cast<float>(font.width) / static_cast<float>(paddedWidth);

    for (int i = FontLoader::kCharBegin; i < FontLoader::kCharEnd; i++) {
        if (FT_Load_Char(face, i, FT_LOAD_RENDER)) {
            CRISP_LOGE("Failed to load character: '{}'", static_cast<char>(i));
            continue;
        }

        updateTexData(
            font.textureData, glyph->bitmap.buffer, currX, paddedWidth, glyph->bitmap.width, glyph->bitmap.rows);

        font.glyphs[i - FontLoader::kCharBegin].atlasOffsetX =
            static_cast<float>(currX) / static_cast<float>(font.width) * xOffsetScale;
        font.glyphs[i - FontLoader::kCharBegin].advanceX = static_cast<float>(glyph->advance.x >> 6);
        font.glyphs[i - FontLoader::kCharBegin].advanceY = static_cast<float>(glyph->advance.y >> 6);
        font.glyphs[i - FontLoader::kCharBegin].bmpWidth = static_cast<float>(glyph->bitmap.width);
        font.glyphs[i - FontLoader::kCharBegin].bmpHeight = static_cast<float>(glyph->bitmap.rows);
        font.glyphs[i - FontLoader::kCharBegin].bmpLeft = static_cast<float>(glyph->bitmap_left);
        font.glyphs[i - FontLoader::kCharBegin].bmpTop = static_cast<float>(glyph->bitmap_top);

        currX += glyph->bitmap.width + kPadding;
    }

    font.width = paddedWidth;
    font.height = paddedHeight;
}

} // namespace

FontLoader::FontLoader() {
    if (FT_Init_FreeType(&m_context)) {
        CRISP_LOGF("Failed to initialize FreeType font library!");
    }
}

FontLoader::~FontLoader() {
    FT_Done_FreeType(m_context);
}

std::unique_ptr<Font> FontLoader::load(const std::filesystem::path& fontPath, int fontPixelSize) const {
    FT_Face face{nullptr};
    if (FT_New_Face(m_context, fontPath.string().c_str(), 0, &face)) {
        spdlog::error("Failed to create new face: {}\n", fontPath.string());
        return nullptr;
    }

    FT_Set_Pixel_Sizes(face, 0, fontPixelSize);

    auto [width, height] = getFontAtlasSize(face);

    uint32_t paddedWidth = ceilPowerOf2(width);
    uint32_t paddedHeight = ceilPowerOf2(height);

    std::unique_ptr<Font> font = std::make_unique<Font>();
    font->width = width;
    font->height = height;
    font->textureData.resize(paddedWidth * paddedHeight);
    loadGlyphs(*font, face, paddedWidth, paddedHeight);

    FT_Done_Face(face);

    font->name = fontPath.filename().string();
    font->pixelSize = fontPixelSize;

    return std::move(font);
}

} // namespace crisp