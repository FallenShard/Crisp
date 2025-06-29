#include <Crisp/Image/Io/Exr.hpp>

#include <vector>

#pragma warning(push)
#pragma warning(disable : 4018)
#pragma warning(disable : 4706)
#pragma warning(disable : 4389)
#pragma warning(disable : 4100)
#pragma warning(disable : 6387)
#pragma warning(disable : 6386)
#pragma warning(disable : 6385)
#pragma warning(disable : 6001)
#pragma warning(disable : 6011)
#pragma warning(disable : 26451)
#pragma warning(disable : 26819)
#pragma warning(disable : 26812)
#pragma warning(disable : 26495)
#define TINYEXR_IMPLEMENTATION
#include <tinyexr/tinyexr.h>
#pragma warning(pop)

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Core/Logger.hpp>

namespace crisp {
namespace {
auto logger = spdlog::stdout_color_st("Exr");

class ExrHeaderGuard {
public:
    explicit ExrHeaderGuard(EXRHeader& header)
        : m_header(header) {
        InitEXRHeader(&m_header);
    }

    ~ExrHeaderGuard() {
        FreeEXRHeader(&m_header);
    }

    ExrHeaderGuard(const ExrHeaderGuard&) = delete;
    ExrHeaderGuard& operator=(const ExrHeaderGuard&) = delete;

    ExrHeaderGuard(ExrHeaderGuard&&) = delete;
    ExrHeaderGuard& operator=(ExrHeaderGuard&&) = delete;

private:
    EXRHeader& m_header;
};

class ExrImageGuard {
public:
    explicit ExrImageGuard(EXRImage& image)
        : m_image(image) {
        InitEXRImage(&m_image);
    }

    ~ExrImageGuard() {
        FreeEXRImage(&m_image);
    }

    ExrImageGuard(const ExrImageGuard&) = delete;
    ExrImageGuard& operator=(const ExrImageGuard&) = delete;

    ExrImageGuard(ExrImageGuard&&) = delete;
    ExrImageGuard& operator=(ExrImageGuard&&) = delete;

private:
    EXRImage& m_image;
};
} // namespace

Result<ExrImageData> loadExr(const std::filesystem::path& imagePath) {
    const std::string pathString = imagePath.string();

    // Read EXR version
    EXRVersion version;
    if (ParseEXRVersionFromFile(&version, pathString.c_str()) != TINYEXR_SUCCESS) {
        return resultError("Could not parse version from EXR file: {}", pathString);
    }

    if (version.multipart > 0) { // Unsupported.
        return resultError("Multipart EXR files are not supported: {}", pathString);
    }

    EXRHeader header;
    ExrHeaderGuard headerGuard(header);
    const char* err = nullptr;
    if (ParseEXRHeaderFromFile(&header, &version, pathString.c_str(), &err) != TINYEXR_SUCCESS) {
        return resultError("Failed to parse EXR header from file {}: {}", pathString, err);
    }

    // Request half-floats to be converted to 32bit floats.
    for (int32_t i = 0; i < header.num_channels; ++i) {
        if (header.pixel_types[i] == TINYEXR_PIXELTYPE_HALF) {         // NOLINT
            header.requested_pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT; // NOLINT
        }
    }

    EXRImage image;
    ExrImageGuard imageGuard(image);
    if (LoadEXRImageFromFile(&image, &header, pathString.c_str(), &err) != TINYEXR_SUCCESS) {
        return resultError("Failed to load EXR image from file {}: {}", pathString, err);
    }

    ExrImageData data{};
    data.width = static_cast<uint32_t>(image.width);
    data.height = static_cast<uint32_t>(image.height);
    data.channelCount = static_cast<uint32_t>(image.num_channels);
    data.pixelData.reserve(data.width * data.height * data.channelCount);

    float** pixelPtr = reinterpret_cast<float**>(image.images); // NOLINT
    for (int32_t i = 0; i < image.height; i++) {
        for (int32_t j = 0; j < image.width; j++) {
            const auto pixelIdx = static_cast<uint32_t>(image.width * i + j);

            for (int32_t k = 0; k < image.num_channels; ++k) {
                data.pixelData.push_back(std::max(0.0f, pixelPtr[k][pixelIdx])); // NOLINT
            }
        }
    }

    return data;
}

Result<> saveExr(
    const std::filesystem::path& outputPath,
    const std::span<const float> hdrPixelData,
    const uint32_t width,
    const uint32_t height,
    const FlipAxis flipAxis) {
    EXRHeader header;
    ExrHeaderGuard headerGuard(header);

    EXRImage image;
    ExrImageGuard imageGuard(image);

    // 4 = RGBA channels
    image.num_channels = 4;
    CRISP_CHECK_EQ(hdrPixelData.size(), width * height * image.num_channels);
    std::vector<std::vector<float>> channels(image.num_channels, std::vector<float>(width * height, 0.0f));
    for (int32_t y = 0; y < static_cast<int32_t>(height); ++y) {
        for (int32_t x = 0; x < static_cast<int32_t>(width); ++x) {
            int32_t rowIdx = flipAxis == FlipAxis::Y ? static_cast<int32_t>(height) - 1 - y : y;
            int32_t colIdx = x;

            for (int32_t c = 0; c < image.num_channels; c++) {
                const int32_t channelIdx = image.num_channels - 1 - c;
                // OpenEXR expects (A)BGR
                channels[c][y * width + colIdx] =
                    hdrPixelData[image.num_channels * (rowIdx * width + colIdx) + channelIdx];
            }
        }
    }

    std::vector<float*> channelPtrs(image.num_channels, nullptr);
    for (int32_t c = 0; c < image.num_channels; c++) {
        channelPtrs[c] = channels[c].data();
    }

    image.images = reinterpret_cast<unsigned char**>(channelPtrs.data()); // NOLINT
    image.width = static_cast<int32_t>(width);
    image.height = static_cast<int32_t>(height);

    header.num_channels = image.num_channels;
    header.channels = (EXRChannelInfo*)malloc(sizeof(EXRChannelInfo) * header.num_channels); // NOLINT
    header.channels[0].name[0] = 'A';                                                        // NOLINT
    header.channels[0].name[1] = '\0';                                                       // NOLINT
    header.channels[1].name[0] = 'B';                                                        // NOLINT
    header.channels[1].name[1] = '\0';                                                       // NOLINT
    header.channels[2].name[0] = 'G';                                                        // NOLINT
    header.channels[2].name[1] = '\0';                                                       // NOLINT
    header.channels[3].name[0] = 'R';                                                        // NOLINT
    header.channels[3].name[1] = '\0';                                                       // NOLINT

    header.pixel_types = (int*)malloc(sizeof(int) * header.num_channels);           // NOLINT
    header.requested_pixel_types = (int*)malloc(sizeof(int) * header.num_channels); // NOLINT
    for (int32_t i = 0; i < header.num_channels; i++) {
        header.pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;           // NOLINT pixel type of input image
        header.requested_pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT; // NOLINT pixel type of output in .EXR
    }

    const char* err = nullptr;
    if (SaveEXRImageToFile(&image, &header, outputPath.string().c_str(), &err) != TINYEXR_SUCCESS) {
        return resultError("Failed to save EXR image into file {}: {}", outputPath.string(), err);
    }

    // Prevent double free on the vector memory once the image goes out of scope.
    image.images = nullptr;

    return kResultSuccess;
}

} // namespace crisp
