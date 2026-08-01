#include <Crisp/Image/Io/Exr.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include <ImfChannelList.h>
#include <ImfFrameBuffer.h>
#include <ImfHeader.h>
#include <ImfInputPart.h>
#include <ImfMultiPartInputFile.h>
#include <ImfOutputFile.h>
#include <ImfPartType.h>
#include <ImfTiledInputPart.h>

namespace crisp {
namespace {
namespace openexr = OPENEXR_IMF_NAMESPACE;

constexpr uint32_t kRgbaChannelCount = 4;

std::string pathToUtf8(const std::filesystem::path& path) {
    const auto utf8Path = path.u8string();
    return {reinterpret_cast<const char*>(utf8Path.data()), utf8Path.size()};
}

std::vector<std::string> getChannelNames(const openexr::ChannelList& channels) {
    std::vector<std::string> channelNames;

    constexpr std::array<std::string_view, kRgbaChannelCount> preferredOrder = {"R", "G", "B", "A"};
    for (const auto name : preferredOrder) {
        if (channels.findChannel(name.data()) != nullptr) {
            channelNames.emplace_back(name);
        }
    }

    for (auto channel = channels.begin(); channel != channels.end(); ++channel) {
        const std::string_view name(channel.name());
        if (std::ranges::find(preferredOrder, name) == preferredOrder.end()) {
            channelNames.emplace_back(name);
        }
    }

    return channelNames;
}

Result<ExrImageData> loadSinglePartExr(openexr::MultiPartInputFile& inputFile, const std::string& path) {
    const auto& header = inputFile.header(0);
    const auto& dataWindow = header.dataWindow();
    const int64_t width = static_cast<int64_t>(dataWindow.max.x) - dataWindow.min.x + 1;
    const int64_t height = static_cast<int64_t>(dataWindow.max.y) - dataWindow.min.y + 1;
    if (width <= 0 || height <= 0 || width > std::numeric_limits<uint32_t>::max() ||
        height > std::numeric_limits<uint32_t>::max()) {
        return resultError("EXR file has invalid dimensions: {}", path);
    }

    const auto channelNames = getChannelNames(header.channels());
    if (channelNames.empty() || channelNames.size() > std::numeric_limits<uint32_t>::max()) {
        return resultError("EXR file has an invalid channel count: {}", path);
    }

    for (const auto& channelName : channelNames) {
        const auto* channel = header.channels().findChannel(channelName);
        if (channel == nullptr || channel->xSampling != 1 || channel->ySampling != 1) {
            return resultError("Subsampled EXR channels are not supported: {}", path);
        }
    }

    const size_t channelCount = channelNames.size();
    const size_t imageWidth = static_cast<size_t>(width);
    const size_t imageHeight = static_cast<size_t>(height);
    if (imageHeight > std::numeric_limits<size_t>::max() / imageWidth) {
        return resultError("EXR image is too large to load: {}", path);
    }

    const size_t pixelCount = imageWidth * imageHeight;
    if (channelCount > std::numeric_limits<size_t>::max() / pixelCount) {
        return resultError("EXR image is too large to load: {}", path);
    }

    ExrImageData data{};
    data.width = static_cast<uint32_t>(width);
    data.height = static_cast<uint32_t>(height);
    data.channelCount = static_cast<uint32_t>(channelCount);
    data.pixelData.resize(pixelCount * channelCount);

    const size_t xStride = channelCount * sizeof(float);
    const size_t yStride = static_cast<size_t>(width) * xStride;
    openexr::FrameBuffer frameBuffer;
    for (size_t channelIndex = 0; channelIndex < channelCount; ++channelIndex) {
        frameBuffer.insert(
            channelNames[channelIndex],
            openexr::Slice::Make(openexr::FLOAT, data.pixelData.data() + channelIndex, dataWindow, xStride, yStride));
    }

    const std::string partType =
        header.hasType() ? header.type() : (header.hasTileDescription() ? openexr::TILEDIMAGE : openexr::SCANLINEIMAGE);
    if (partType == openexr::SCANLINEIMAGE) {
        openexr::InputPart inputPart(inputFile, 0);
        if (!inputPart.isComplete()) {
            return resultError("EXR file is incomplete: {}", path);
        }
        inputPart.setFrameBuffer(frameBuffer);
        inputPart.readPixels(dataWindow.min.y, dataWindow.max.y);
    } else if (partType == openexr::TILEDIMAGE) {
        openexr::TiledInputPart inputPart(inputFile, 0);
        if (!inputPart.isComplete()) {
            return resultError("EXR file is incomplete: {}", path);
        }
        inputPart.setFrameBuffer(frameBuffer);
        inputPart.readTiles(0, inputPart.numXTiles(0) - 1, 0, inputPart.numYTiles(0) - 1, 0);
    } else {
        return resultError("Unsupported EXR part type '{}' in file: {}", partType, path);
    }

    for (float& value : data.pixelData) {
        value = std::max(0.0f, value);
    }

    return data;
}
} // namespace

Result<ExrImageData> loadExr(const std::filesystem::path& imagePath) {
    const std::string path = pathToUtf8(imagePath);

    try {
        openexr::MultiPartInputFile inputFile(path.c_str());
        if (inputFile.parts() != 1) {
            return resultError("Multipart EXR files are not supported: {}", path);
        }

        return loadSinglePartExr(inputFile, path);
    } catch (const std::exception& exception) {
        return resultError("Failed to load EXR file {}: {}", path, exception.what());
    }
}

Result<> saveExr(
    const std::filesystem::path& outputPath,
    const std::span<const float> hdrPixelData,
    const uint32_t width,
    const uint32_t height,
    const FlipAxis flipAxis) {
    if (width == 0 || height == 0 || width > static_cast<uint32_t>(std::numeric_limits<int>::max()) ||
        height > static_cast<uint32_t>(std::numeric_limits<int>::max())) {
        return resultError("Invalid EXR output dimensions {}x{} for {}", width, height, pathToUtf8(outputPath));
    }

    const uint64_t expectedValueCount = static_cast<uint64_t>(width) * static_cast<uint64_t>(height) * kRgbaChannelCount;
    if (expectedValueCount > std::numeric_limits<size_t>::max() || hdrPixelData.size() != expectedValueCount) {
        return resultError(
            "Invalid RGBA data for EXR output {}: {}x{} requires {} values, received {}",
            pathToUtf8(outputPath),
            width,
            height,
            expectedValueCount,
            hdrPixelData.size());
    }

    std::vector<float> flippedPixelData;
    std::span<const float> pixelData = hdrPixelData;
    if (flipAxis == FlipAxis::Y) {
        flippedPixelData.resize(hdrPixelData.size());
        const size_t rowValueCount = static_cast<size_t>(width) * kRgbaChannelCount;
        for (uint32_t y = 0; y < height; ++y) {
            const auto source = hdrPixelData.begin() + static_cast<size_t>(height - 1 - y) * rowValueCount;
            std::ranges::copy_n(
                source, rowValueCount, flippedPixelData.begin() + static_cast<size_t>(y) * rowValueCount);
        }
        pixelData = flippedPixelData;
    }

    const std::string path = pathToUtf8(outputPath);
    try {
        openexr::Header header(static_cast<int>(width), static_cast<int>(height));
        header.channels().insert("R", openexr::Channel(openexr::FLOAT));
        header.channels().insert("G", openexr::Channel(openexr::FLOAT));
        header.channels().insert("B", openexr::Channel(openexr::FLOAT));
        header.channels().insert("A", openexr::Channel(openexr::FLOAT));

        const size_t xStride = kRgbaChannelCount * sizeof(float);
        const size_t yStride = static_cast<size_t>(width) * xStride;
        openexr::FrameBuffer frameBuffer;
        constexpr std::array<std::string_view, kRgbaChannelCount> channelNames = {"R", "G", "B", "A"};
        for (size_t channelIndex = 0; channelIndex < channelNames.size(); ++channelIndex) {
            frameBuffer.insert(
                channelNames[channelIndex].data(),
                openexr::Slice::Make(
                    openexr::FLOAT, pixelData.data() + channelIndex, header.dataWindow(), xStride, yStride));
        }

        openexr::OutputFile outputFile(path.c_str(), header);
        outputFile.setFrameBuffer(frameBuffer);
        outputFile.writePixels(static_cast<int>(height));
    } catch (const std::exception& exception) {
        return resultError("Failed to save EXR file {}: {}", path, exception.what());
    }

    return kResultSuccess;
}

} // namespace crisp
