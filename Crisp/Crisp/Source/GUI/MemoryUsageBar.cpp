#include "MemoryUsageBar.hpp"

#include <sstream>
#include <iomanip>

#include "Form.hpp"
#include "Label.hpp"
#include "StopWatch.hpp"

namespace crisp::gui
{
    namespace
    {
        glm::vec4 interpolateColor(float t)
        {
            if (t > 0.5f)
            {
                return { 1.0f, 2.0f * (1.0f - t), 0.0f, 1.0f };
            }
            else
            {
                return { t / 0.5f, 1.0f, 0.0f, 1.0f };
            }
        }
    }

    MemoryUsageBar::MemoryUsageBar(Form* parentForm)
        : Panel(parentForm)
    {
        setId("memoryUsageBar");
        setAnchor(Anchor::BottomLeft);
        setPosition({ 0, 0 });
        setSizeHint({ 0, 20 });
        setPadding({ 3, 3 });
        setColor(glm::vec4(0.15f, 0.15f, 0.15f, 1.0f));
        setHorizontalSizingPolicy(SizingPolicy::FillParent);

        auto bufferLabel = std::make_shared<gui::Label>(parentForm, "");
        bufferLabel->setId("bufferMemoryLabel");
        bufferLabel->setAnchor(Anchor::CenterLeft);
        addControl(bufferLabel);
        m_bufferMemoryUsageLabel = bufferLabel.get();

        auto imageLabel = std::make_shared<gui::Label>(parentForm, "");
        imageLabel->setId("imageMemoryLabel");
        imageLabel->setAnchor(Anchor::CenterLeft);
        imageLabel->setPosition({ 200, 0 });
        addControl(imageLabel);
        m_imageMemoryUsageLabel = imageLabel.get();

        auto stagingLabel = std::make_shared<gui::Label>(parentForm, "");
        stagingLabel->setId("stagingMemoryLabel");
        stagingLabel->setAnchor(Anchor::CenterLeft);
        stagingLabel->setPosition({ 400, 0 });
        addControl(stagingLabel);
        m_stagingMemoryUsageLabel = stagingLabel.get();

        m_stopWatch = std::make_unique<StopWatch>(2.0);
        m_stopWatch->triggered.subscribe([this]()
        {
            auto metrics = m_renderSystem->getDeviceMemoryUsage();

            std::ostringstream bufferMemStream;
            bufferMemStream << "Buffer Memory: " << std::to_string(metrics.bufferMemoryUsed >> 20) << " / " << std::to_string(metrics.bufferMemorySize >> 20) << " MB";
            m_bufferMemoryUsageLabel->setText(bufferMemStream.str());
            m_bufferMemoryUsageLabel->setColor(interpolateColor(static_cast<float>(metrics.bufferMemoryUsed) / static_cast<float>(metrics.bufferMemorySize)));

            std::ostringstream imageMemStream;
            imageMemStream << "Image Memory: " << std::to_string(metrics.imageMemoryUsed >> 20) << " / " << std::to_string(metrics.imageMemorySize >> 20) << " MB";
            m_imageMemoryUsageLabel->setText(imageMemStream.str());
            m_imageMemoryUsageLabel->setColor(interpolateColor(static_cast<float>(metrics.imageMemoryUsed) / static_cast<float>(metrics.imageMemorySize)));
            
            std::ostringstream stagingMemStream;
            stagingMemStream << "Staging Memory: " << std::to_string(metrics.stagingMemoryUsed >> 20) << " / " << std::to_string(metrics.stagingMemorySize >> 20) << " MB";
            m_stagingMemoryUsageLabel->setText(stagingMemStream.str());
            m_stagingMemoryUsageLabel->setColor(interpolateColor(static_cast<float>(metrics.stagingMemoryUsed) / static_cast<float>(metrics.stagingMemorySize)));
        });


        m_form->addStopWatch(m_stopWatch.get());
    }

    MemoryUsageBar::~MemoryUsageBar()
    {
        m_form->removeStopWatch(m_stopWatch.get());
    }
}