#pragma once

#include <functional>
#include <memory>
#include <string>

#include <Crisp/PathTracer/Core/VariantMap.hpp>
#include <Crisp/PathTracer/Spectra/Spectrum.hpp>

namespace crisp {
template <typename T>
class Texture {
public:
    Texture(const VariantMap& params = VariantMap()) {
        m_name = params.get<std::string>("name", "");
    }

    virtual ~Texture() {}

    virtual T eval(const glm::vec2& uv) const = 0;

    std::string getName() const {
        return m_name;
    }

protected:
    std::string m_name;
};
} // namespace crisp