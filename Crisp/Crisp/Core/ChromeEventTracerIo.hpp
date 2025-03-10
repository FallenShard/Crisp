#pragma once

#include <filesystem>

namespace crisp {

void serializeTracedEvents(const std::filesystem::path& outputFile);

} // namespace crisp