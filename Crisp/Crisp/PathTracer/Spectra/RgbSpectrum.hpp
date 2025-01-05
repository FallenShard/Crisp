#pragma once

#include <Crisp/Math/Headers.hpp>

#include <iostream>

namespace crisp {
struct RgbSpectrum {
#pragma warning(push)
#pragma warning(disable : 4201) // nameless struct

    union {
        float values[3]; // NOLINT

        struct {
            float r, g, b;
        };
    };

#pragma warning(pop)

    constexpr RgbSpectrum(float value = 0.0f) // NOLINT
        : r(value)
        , g(value)
        , b(value) {}

    constexpr RgbSpectrum(float red, float green, float blue) // NOLINT
        : r(red)
        , g(green)
        , b(blue) {}

    RgbSpectrum(const glm::vec3& vec); // NOLINT (single-arg)

    float& operator[](int index);
    const float& operator[](int index) const;

    RgbSpectrum& operator+=(const RgbSpectrum& rgbSpectrum);
    RgbSpectrum& operator-=(const RgbSpectrum& rgbSpectrum);
    RgbSpectrum& operator*=(const RgbSpectrum& rgbSpectrum);
    RgbSpectrum& operator/=(const RgbSpectrum& rgbSpectrum);
    RgbSpectrum operator+(const RgbSpectrum& rgbSpectrum) const;
    RgbSpectrum operator-(const RgbSpectrum& rgbSpectrum) const;
    RgbSpectrum operator*(const RgbSpectrum& rgbSpectrum) const;
    RgbSpectrum operator/(const RgbSpectrum& rgbSpectrum) const;

    RgbSpectrum& operator+=(float scalar);
    RgbSpectrum& operator-=(float scalar);
    RgbSpectrum& operator*=(float scalar);
    RgbSpectrum& operator/=(float scalar);
    RgbSpectrum operator+(float scalar) const;
    RgbSpectrum operator-(float scalar) const;
    RgbSpectrum operator*(float scalar) const;
    RgbSpectrum operator/(float scalar) const;

    RgbSpectrum operator-() const;

    float getLuminance() const;
    float maxCoeff() const;

    RgbSpectrum exp() const;
    RgbSpectrum clamp() const;

    bool isValid() const;
    bool isZero() const;
    bool isInfinite() const;
    bool isNaN() const;

    RgbSpectrum toSrgb() const;

    constexpr static RgbSpectrum zero() {
        return {0.0f};
    }
};

RgbSpectrum operator+(float scalar, const RgbSpectrum& rgbSpectrum);
RgbSpectrum operator*(float scalar, const RgbSpectrum& rgbSpectrum);
std::ostream& operator<<(std::ostream& stream, const RgbSpectrum& spec);
} // namespace crisp