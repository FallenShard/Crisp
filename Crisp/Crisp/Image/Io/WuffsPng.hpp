#pragma once

#include <span>

#include <Crisp/Core/Result.hpp>
#include <Crisp/Image/Image.hpp>

namespace crisp {

Result<Image> loadPngWithWuffs(std::span<const uint8_t> imageFileContent);

} // namespace crisp
