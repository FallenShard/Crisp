#define NOMINMAX
#include "RgbSpectrum.hpp"

#include <iostream>

namespace crisp
{
    RgbSpectrum::RgbSpectrum(const glm::vec3& vec)
        : r(vec.r)
        , g(vec.g)
        , b(vec.b)
    {
    }

    float& RgbSpectrum::operator[](int index)
    {
        return values[index];
    }

    const float& RgbSpectrum::operator[](int index) const
    {
        return values[index];
    }

    RgbSpectrum& RgbSpectrum::operator=(const RgbSpectrum & rgbSpectrum)
    {
        r = rgbSpectrum.r;
        g = rgbSpectrum.g;
        b = rgbSpectrum.b;
        return *this;
    }

    RgbSpectrum& RgbSpectrum::operator+=(const RgbSpectrum & rgbSpectrum)
    {
        r += rgbSpectrum.r;
        g += rgbSpectrum.g;
        b += rgbSpectrum.b;
        return *this;
    }

    RgbSpectrum& RgbSpectrum::operator-=(const RgbSpectrum & rgbSpectrum)
    {
        r -= rgbSpectrum.r;
        g -= rgbSpectrum.g;
        b -= rgbSpectrum.b;
        return *this;
    }

    RgbSpectrum& RgbSpectrum::operator*=(const RgbSpectrum & rgbSpectrum)
    {
        r *= rgbSpectrum.r;
        g *= rgbSpectrum.g;
        b *= rgbSpectrum.b;
        return *this;
    }

    RgbSpectrum& RgbSpectrum::operator/=(const RgbSpectrum & rgbSpectrum)
    {
        r /= rgbSpectrum.r;
        g /= rgbSpectrum.g;
        b /= rgbSpectrum.b;
        return *this;
    }

    const RgbSpectrum RgbSpectrum::operator+(const RgbSpectrum & rgbSpectrum) const
    {
        return RgbSpectrum(*this) += rgbSpectrum;
    }

    const RgbSpectrum RgbSpectrum::operator-(const RgbSpectrum & rgbSpectrum) const
    {
        return RgbSpectrum(*this) -= rgbSpectrum;
    }

    const RgbSpectrum RgbSpectrum::operator*(const RgbSpectrum & rgbSpectrum) const
    {
        return RgbSpectrum(*this) *= rgbSpectrum;
    }

    const RgbSpectrum RgbSpectrum::operator/(const RgbSpectrum & rgbSpectrum) const
    {
        return RgbSpectrum(*this) /= rgbSpectrum;
    }

    RgbSpectrum& RgbSpectrum::operator+=(float scalar)
    {
        r += scalar;
        g += scalar;
        b += scalar;
        return *this;
    }

    RgbSpectrum & RgbSpectrum::operator-=(float scalar)
    {
        r -= scalar;
        g -= scalar;
        b -= scalar;
        return *this;
    }

    RgbSpectrum& RgbSpectrum::operator*=(float scalar)
    {
        r *= scalar;
        g *= scalar;
        b *= scalar;
        return *this;
    }

    RgbSpectrum& RgbSpectrum::operator/=(float scalar)
    {
        r /= scalar;
        g /= scalar;
        b /= scalar;
        return *this;
    }

    const RgbSpectrum RgbSpectrum::operator+(float scalar) const
    {
        return RgbSpectrum(*this) += scalar;
    }

    const RgbSpectrum RgbSpectrum::operator-(float scalar) const
    {
        return RgbSpectrum(*this) -= scalar;
    }

    const RgbSpectrum RgbSpectrum::operator*(float scalar) const
    {
        return RgbSpectrum(*this) *= scalar;
    }

    const RgbSpectrum RgbSpectrum::operator/(float scalar) const
    {
        return RgbSpectrum(*this) /= scalar;
    }

    const RgbSpectrum RgbSpectrum::operator-() const
    {
        return {-r, -g, -b};
    }

    float RgbSpectrum::getLuminance() const
    {
        return r * 0.212671f + g * 0.715160f + b * 0.072169f;
    }

    float RgbSpectrum::maxCoeff() const
    {
        return std::max(r, std::max(g, b));
    }

    const RgbSpectrum RgbSpectrum::exp() const
    {
        return { std::exp(r), std::exp(g), std::exp(b) };
    }

    RgbSpectrum RgbSpectrum::clamp() const
    {
        return RgbSpectrum(std::max(r, 0.0f), std::max(g, 0.0f), std::max(b, 0.0f));
    }

    bool RgbSpectrum::isValid() const
    {
        if (r < 0.0f || !std::isfinite(r)) return false;
        if (g < 0.0f || !std::isfinite(g)) return false;
        if (b < 0.0f || !std::isfinite(b)) return false;

        return true;
    }

    bool RgbSpectrum::isZero() const
    {
        return r == 0.0f && g == 0.0f && b == 0.0f;
    }

    bool RgbSpectrum::isInfinite() const
    {
        if (!std::isfinite(r) ||
            !std::isfinite(g) ||
            !std::isfinite(b))
            return true;

        return false;
    }

    bool RgbSpectrum::isNaN() const
    {
        if (std::isnan(r) ||
            std::isnan(g) ||
            std::isnan(b))
            return true;

        return false;
    }

    RgbSpectrum RgbSpectrum::toSrgb() const
    {
        RgbSpectrum result;

        for (int i = 0; i < 3; ++i)
        {
            float val = values[i];
            if (val <= 0.0031308f)
                result[i] = 12.92f * val;
            else
                result[i] = (1.f + 0.055f) * std::pow(val, 1.f / 2.4f) - 0.055f;
        }

        return result;
    }

    const RgbSpectrum operator+(float scalar, const RgbSpectrum& rgbSpectrum)
    {
        return rgbSpectrum + scalar;
    }

    const RgbSpectrum operator*(float scalar, const RgbSpectrum& rgbSpectrum)
    {
        return rgbSpectrum * scalar;
    }

    std::ostream& operator<<(std::ostream& stream, const RgbSpectrum& spec)
    {
        stream << "[ " << spec.r << ", " << spec.g << ", " << spec.b << "]";
        return stream;
    }
}