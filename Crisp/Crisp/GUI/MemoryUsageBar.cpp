#include "MemoryUsageBar.hpp"

#include <iomanip>
#include <sstream>

#include <Crisp/Gui/Form.hpp>
#include <Crisp/Gui/Label.hpp>
#include <Crisp/Gui/StopWatch.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>

namespace crisp::gui {
namespace {
glm::vec4 interpolateColor(float t) {
    if (t > 0.5f) {
        return {1.0f, 2.0f * (1.0f - t), 0.0f, 1.0f};
    } else {
        return {t / 0.5f, 1.0f, 0.0f, 1.0f};
    }
}
} // namespace

MemoryUsageBar::MemoryUsageBar(Form* parentForm)
    : Panel(parentForm) {
    setId("memoryUsageBar");
    setAnchor(Anchor::BottomLeft);
    setOrigin(Origin::BottomLeft);
    setPosition({0, 0});
    setSizeHint({0, 20});
    setPadding({3, 3});
    setColor(glm::vec4(0.15f, 0.15f, 0.15f, 1.0f));
    setHorizontalSizingPolicy(SizingPolicy::FillParent);

    auto bufferLabel = std::make_unique<gui::Label>(parentForm, "");
    bufferLabel->setId("bufferMemoryLabel");
    bufferLabel->setAnchor(Anchor::CenterLeft);
    bufferLabel->setOrigin(Origin::CenterLeft);
    m_bufferMemoryUsageLabel = bufferLabel.get();
    addControl(std::move(bufferLabel));

    auto imageLabel = std::make_unique<gui::Label>(parentForm, "");
    imageLabel->setId("imageMemoryLabel");
    imageLabel->setAnchor(Anchor::CenterLeft);
    imageLabel->setOrigin(Origin::CenterLeft);
    imageLabel->setPosition({200, 0});
    m_imageMemoryUsageLabel = imageLabel.get();
    addControl(std::move(imageLabel));

    auto stagingLabel = std::make_unique<gui::Label>(parentForm, "");
    stagingLabel->setId("stagingMemoryLabel");
    stagingLabel->setAnchor(Anchor::CenterLeft);
    stagingLabel->setOrigin(Origin::CenterLeft);
    stagingLabel->setPosition({400, 0});
    m_stagingMemoryUsageLabel = stagingLabel.get();
    addControl(std::move(stagingLabel));

    m_stopWatch = std::make_unique<StopWatch>(2.0);
    m_stopWatch->triggered += [this]() {
        auto metrics = m_renderSystem->getRenderer().getDevice().getMemoryAllocator().getDeviceMemoryUsage();

        auto transformToMb = [](uint64_t bytes) {
            uint64_t megaBytes = bytes >> 20;
            uint64_t remainder = (bytes & ((1 << 20) - 1)) > 0;
            return std::to_string(megaBytes + remainder);
        };

        std::ostringstream bufferMemStream;
        bufferMemStream << "Buffer Memory: " << transformToMb(metrics.bufferMemoryUsed) << " / "
                        << std::to_string(metrics.bufferMemorySize >> 20) << " MB";
        m_bufferMemoryUsageLabel->setText(bufferMemStream.str());
        m_bufferMemoryUsageLabel->setColor(interpolateColor(
            static_cast<float>(metrics.bufferMemoryUsed) / static_cast<float>(metrics.bufferMemorySize)));

        std::ostringstream imageMemStream;
        imageMemStream << "Image Memory: " << std::to_string(metrics.imageMemoryUsed >> 20) << " / "
                       << std::to_string(metrics.imageMemorySize >> 20) << " MB";
        m_imageMemoryUsageLabel->setText(imageMemStream.str());
        m_imageMemoryUsageLabel->setColor(interpolateColor(
            static_cast<float>(metrics.imageMemoryUsed) / static_cast<float>(metrics.imageMemorySize)));

        std::ostringstream stagingMemStream;
        stagingMemStream << "Staging Memory: " << std::to_string(metrics.stagingMemoryUsed >> 20) << " / "
                         << std::to_string(metrics.stagingMemorySize >> 20) << " MB";
        m_stagingMemoryUsageLabel->setText(stagingMemStream.str());
        m_stagingMemoryUsageLabel->setColor(interpolateColor(
            static_cast<float>(metrics.stagingMemoryUsed) / static_cast<float>(metrics.stagingMemorySize)));
    };

    m_form->addStopWatch(m_stopWatch.get());
}

MemoryUsageBar::~MemoryUsageBar() {
    m_form->removeStopWatch(m_stopWatch.get());
}
} // namespace crisp::gui