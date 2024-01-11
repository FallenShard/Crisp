#include <Crisp/PathTracer/Textures/UVTexture.hpp>

namespace crisp {
UVTexture::UVTexture(const VariantMap& params)
    : Texture<Spectrum>(params) {}

Spectrum UVTexture::eval(const glm::vec2& uv) const {
    return Spectrum(uv.x, uv.y, 0.0f);
}
} // namespace crisp