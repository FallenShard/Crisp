#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#pragma warning(push)
#pragma warning(disable : 4201) // nameless struct
#pragma warning(disable : 4127)
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/transform.hpp>
#pragma warning(pop)

#include <type_traits>
#include <utility>

namespace glm {
template <std::size_t I, glm::length_t N, typename T, qualifier Q>
constexpr auto& get(glm::vec<N, T, Q>& v) noexcept {
    return v[I];
}

template <std::size_t I, glm::length_t N, typename T, qualifier Q>
constexpr const auto& get(const glm::vec<N, T, Q>& v) noexcept {
    return v[I];
}

template <std::size_t I, glm::length_t N, typename T, qualifier Q>
constexpr auto&& get(glm::vec<N, T, Q>&& v) noexcept { // NOLINT
    return v[I];
}
} // namespace glm

namespace std {
template <glm::length_t N, typename T, glm::qualifier Q>
struct tuple_size<glm::vec<N, T, Q>> : integral_constant<std::size_t, N> {};

template <std::size_t I, glm::length_t N, typename T, glm::qualifier Q>
struct tuple_element<I, glm::vec<N, T, Q>> {
    using type = T;
};
} // namespace std
