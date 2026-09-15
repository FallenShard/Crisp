#include <Crisp/Lights/EnvironmentLightIo.hpp>

#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <fstream>
#include <string_view>

#include <Crisp/Image/Io/Utils.hpp>

namespace crisp {
namespace {

void skipWhitespace(const char*& cursor, const char* end) {
    while (cursor != end && std::isspace(static_cast<unsigned char>(*cursor))) {
        ++cursor;
    }
}

Result<glm::vec3> parseDiffuseIrradianceShCoefficient(
    const std::string_view line, const std::filesystem::path& path, const size_t lineNumber) {
    const char* cursor = line.data();
    const char* const end = cursor + line.size();

    const auto consume = [&cursor, end](const char expected) {
        skipWhitespace(cursor, end);
        if (cursor == end || *cursor != expected) {
            return false;
        }
        ++cursor;
        return true;
    };

    if (!consume('(')) {
        return resultError("Expected '(' in diffuse-irradiance SH coefficient at {}:{}", path.string(), lineNumber);
    }

    std::array<float, kDiffuseIrradianceShChannelCount> channels{};
    for (size_t channel = 0; channel < channels.size(); ++channel) {
        skipWhitespace(cursor, end);
        const auto [valueEnd, error] = std::from_chars(cursor, end, channels[channel]);
        if (error != std::errc{}) {
            return resultError(
                "Invalid diffuse-irradiance SH coefficient component at {}:{}", path.string(), lineNumber);
        }
        cursor = valueEnd;
        if (!consume(channel + 1 < channels.size() ? ',' : ')')) {
            return resultError(
                "Invalid diffuse-irradiance SH coefficient delimiter at {}:{}", path.string(), lineNumber);
        }
    }

    if (!consume(';')) {
        return resultError("Expected ';' after diffuse-irradiance SH coefficient at {}:{}", path.string(), lineNumber);
    }
    skipWhitespace(cursor, end);
    if (cursor != end && std::string_view(cursor, static_cast<size_t>(end - cursor)).find("//") != 0) {
        return resultError(
            "Unexpected text after diffuse-irradiance SH coefficient at {}:{}", path.string(), lineNumber);
    }

    return glm::vec3(channels[0], channels[1], channels[2]);
}

Result<DiffuseIrradianceSh> loadDiffuseIrradianceSh(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        return resultError("Failed to open diffuse-irradiance SH coefficients: {}", path.string());
    }

    DiffuseIrradianceSh coefficients{};
    std::string line;
    size_t coefficientIndex = 0;
    size_t lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        const auto firstCharacter = line.find_first_not_of(" \t\r\n");
        if (firstCharacter == std::string::npos || line.compare(firstCharacter, 2, "//") == 0) {
            continue;
        }
        if (coefficientIndex >= kDiffuseIrradianceShCoefficientCount) {
            return resultError("Too many diffuse-irradiance SH coefficients in {}", path.string());
        }
        CRISP_TRY(const auto coefficient, parseDiffuseIrradianceShCoefficient(line, path, lineNumber));
        if (!std::isfinite(coefficient.x) || !std::isfinite(coefficient.y) || !std::isfinite(coefficient.z)) {
            return resultError("Non-finite diffuse-irradiance SH coefficient in {}", path.string());
        }
        const size_t channelOffset = coefficientIndex * kDiffuseIrradianceShChannelCount;
        coefficients[channelOffset + 0] = coefficient.x;
        coefficients[channelOffset + 1] = coefficient.y;
        coefficients[channelOffset + 2] = coefficient.z;
        ++coefficientIndex;
    }

    if (coefficientIndex != kDiffuseIrradianceShCoefficientCount) {
        return resultError(
            "Expected {} diffuse-irradiance SH coefficients in {}, found {}",
            kDiffuseIrradianceShCoefficientCount,
            path.string(),
            coefficientIndex);
    }
    return coefficients;
}

} // namespace

Result<ImageBasedLightingData> loadImageBasedLightingData(const std::filesystem::path& environmentMapDir) {
    const auto envMapName{environmentMapDir.stem().string()};
    ImageBasedLightingData data{};

    constexpr uint32_t kRequestedChannels{4};
    data.equirectangularEnvironmentMap =
        loadImage(environmentMapDir / fmt::format("{}.hdr", envMapName), kRequestedChannels, FlipAxis::None).unwrap();

    CRISP_TRY(data.diffuseIrradianceSh, loadDiffuseIrradianceSh(environmentMapDir / "sh.txt"));

    constexpr uint32_t kReflectionMipMapLevels{9};
    for (uint32_t i = 0; i < kReflectionMipMapLevels; ++i) {
        const uint32_t width = (1 << (kReflectionMipMapLevels - i)) * 4;
        const uint32_t height = (1 << (kReflectionMipMapLevels - i)) * 3;
        const std::filesystem::path reflMapPath{
            environmentMapDir / fmt::format("{}_rad_{}_{}x{}.hdr", envMapName, i, width, height)};
        data.specularReflectanceMapMipLevels.emplace_back(loadCubeMapFacesFromHCrossImage(reflMapPath));
    }
    return data;
}

} // namespace crisp
