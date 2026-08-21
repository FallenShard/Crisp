#include <Crisp/Image/Io/WuffsPng.hpp>

#include <cstdlib>
#include <limits>
#include <memory>
#include <vector>

#if defined(_MSC_VER)
#pragma warning(push, 0)
#endif
#define WUFFS_IMPLEMENTATION
#define WUFFS_CONFIG__MODULE__PNG
#include <wuffs-v0.3.c>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace crisp {
namespace {

const char* getWuffsErrorMessage(const wuffs_base__status status) {
    return status.repr != nullptr ? status.repr : "unknown Wuffs error";
}

} // namespace

Result<Image> loadPngWithWuffs(const std::span<const uint8_t> imageFileContent) {
    using DecoderPtr = std::unique_ptr<wuffs_png__decoder, decltype(&std::free)>;
    DecoderPtr decoder{wuffs_png__decoder__alloc(), &std::free};
    if (!decoder) {
        return resultError("Failed to allocate Wuffs PNG decoder.");
    }

    auto source = wuffs_base__ptr_u8__reader(
        const_cast<uint8_t*>(imageFileContent.data()), imageFileContent.size(), true); // NOLINT
    wuffs_base__image_config imageConfig{};
    auto status = wuffs_png__decoder__decode_image_config(decoder.get(), &imageConfig, &source);
    if (status.repr != nullptr) {
        return resultError("Failed to read PNG image configuration with Wuffs: {}.", getWuffsErrorMessage(status));
    }

    const uint32_t width = wuffs_base__pixel_config__width(&imageConfig.pixcfg);
    const uint32_t height = wuffs_base__pixel_config__height(&imageConfig.pixcfg);
    constexpr uint32_t kChannelCount = 4;
    const uint64_t pixelCount = static_cast<uint64_t>(width) * height;
    if (pixelCount > std::numeric_limits<size_t>::max() / static_cast<size_t>(kChannelCount)) {
        return resultError("Wuffs PNG dimensions {}x{} exceed addressable memory.", width, height);
    }

    wuffs_base__pixel_config pixelConfig{};
    wuffs_base__pixel_config__set(
        &pixelConfig, WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL, WUFFS_BASE__PIXEL_SUBSAMPLING__NONE, width, height);

    std::vector<uint8_t> pixelData(static_cast<size_t>(pixelCount) * kChannelCount);

    wuffs_base__pixel_buffer pixelBuffer{};
    status = wuffs_base__pixel_buffer__set_from_slice(
        &pixelBuffer, &pixelConfig, wuffs_base__make_slice_u8(pixelData.data(), pixelData.size()));
    if (status.repr != nullptr) {
        return resultError("Failed to configure Wuffs PNG output: {}.", getWuffsErrorMessage(status));
    }

    const uint64_t workBufferSize = wuffs_png__decoder__workbuf_len(decoder.get()).max_incl;
    if (workBufferSize > std::numeric_limits<size_t>::max()) {
        return resultError("Wuffs PNG work buffer exceeds addressable memory.");
    }
    std::vector<uint8_t> workBuffer(static_cast<size_t>(workBufferSize));

    status = wuffs_png__decoder__decode_frame(
        decoder.get(),
        &pixelBuffer,
        &source,
        WUFFS_BASE__PIXEL_BLEND__SRC,
        wuffs_base__make_slice_u8(workBuffer.data(), workBuffer.size()),
        nullptr);
    if (status.repr != nullptr) {
        return resultError("Failed to decode PNG image with Wuffs: {}.", getWuffsErrorMessage(status));
    }

    return Image(std::move(pixelData), width, height, kChannelCount, kChannelCount);
}

} // namespace crisp
