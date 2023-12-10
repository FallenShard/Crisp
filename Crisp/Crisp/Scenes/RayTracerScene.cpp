#include <Crisp/Scenes/RayTracerScene.hpp>

#include <iostream>

#include <Crisp/Core/Application.hpp>
#include <Crisp/Core/Window.hpp>
#include <Crisp/GUI/Button.hpp>
#include <Crisp/GUI/ComboBox.hpp>
#include <Crisp/GUI/Control.hpp>
#include <Crisp/GUI/Form.hpp>
#include <Crisp/GUI/Label.hpp>
#include <Crisp/GUI/MemoryUsageBar.hpp>
#include <Crisp/GUI/Panel.hpp>
#include <Crisp/GUI/Slider.hpp>
#include <Crisp/IO/FileUtils.hpp>
#include <Crisp/Image/Io/Exr.hpp>
#include <Crisp/Scenes/RaytracedImage.hpp>
#include <Crisp/Vulkan/VulkanImageView.hpp>

namespace crisp {

RayTracerScene::RayTracerScene(Renderer* renderer, Window* window)
    : Scene(renderer, window)
    , m_progress(0.0f)
    , m_timeSpentRendering(0.0f) {
    m_image = std::make_unique<RayTracedImage>(512, 512, m_renderer);
    m_imageData.resize(512 * 512 * 4, 0.01f);
    m_rayTracer = std::make_unique<RayTracer>();
    m_rayTracer->setProgressUpdater([this](RayTracerUpdate&& update) {
        m_progress = static_cast<float>(update.pixelsRendered) / static_cast<float>(update.numPixels);
        m_timeSpentRendering = update.totalTimeSpentRendering;
        m_updateQueue.emplace(std::move(update));
        rayTracerProgressed(m_progress, m_timeSpentRendering);
    });
    m_renderer->setSceneImageView(nullptr, 0);

    createGui();
}

void RayTracerScene::resize(int width, int height) {
    m_image->resize(width, height);
}

void RayTracerScene::update(float) {
    while (!m_updateQueue.empty()) {
        RayTracerUpdate update;
        if (m_updateQueue.try_pop(update)) {
            m_image->postTextureUpdate(update);

            auto imageSize = m_rayTracer->getImageSize();
            for (int y = 0; y < update.height; y++) {
                for (int x = 0; x < update.width; x++) {
                    size_t rowIdx = update.y + y;
                    size_t colIdx = update.x + x;

                    for (int c = 0; c < 4; c++) {
                        m_imageData[(rowIdx * imageSize.x + colIdx) * 4 + c] =
                            update.data[(y * update.width + x) * 4 + c];
                    }
                }
            }
        }
    }
}

void RayTracerScene::render() {
    if (m_image) {
        m_image->draw(m_renderer);
    }
}

void RayTracerScene::openSceneFileFromDialog() {
    std::string openedFile = "D:/version-control/Crisp/Resources/VesperScenes/cbox-test-mis.xml"; // openFileDialog();
    if (openedFile.empty()) {
        return;
    }

    openSceneFile(openedFile);
}

void RayTracerScene::startRendering() {
    m_rayTracer->start();
}

void RayTracerScene::stopRendering() {
    m_rayTracer->stop();
}

void RayTracerScene::writeImageToExr() {
    const std::string outputDirectory = "Output";
    std::filesystem::create_directories(outputDirectory);

    std::filesystem::path filepath = fmt::format("{}/{}.exr", outputDirectory, m_projectName);

    // Avoid overwriting by providing a new filepath with an index
    int i = 0;
    while (std::filesystem::exists(filepath)) {
        filepath = fmt::format("{}/{}_{}.exr", outputDirectory, ++i, m_projectName);
    }

    spdlog::info("Writing an EXR image at {}", filepath.string());
    const glm::ivec2 imageSize = m_rayTracer->getImageSize();
    saveExr(filepath, m_imageData, imageSize.x, imageSize.y, FlipAxis::Y).unwrap();
}

void RayTracerScene::openSceneFile(const std::filesystem::path& filename) {
    m_renderer->flushResourceUpdates(true);
    m_projectName = filename.stem().string();
    m_rayTracer->initializeScene(filename.string());

    glm::ivec2 imageSize = m_rayTracer->getImageSize();
    m_image = std::make_unique<RayTracedImage>(imageSize.x, imageSize.y, m_renderer);
    m_imageData.resize(4 * imageSize.x * imageSize.y);
    // m_app->getWindow().setTitle(filename.string());
}

void RayTracerScene::createGui() {
    gui::Form* form = nullptr; // m_app->getForm();

    auto panel = std::make_unique<gui::Panel>(form);
    panel->setId("vesperOptionsPanel");
    panel->setPosition({10, 30});
    panel->setSizeHint({200, 500});
    panel->setPadding({10, 10});
    panel->setAnchor(gui::Anchor::TopLeft);
    panel->setVerticalSizingPolicy(gui::SizingPolicy::WrapContent);

    auto button = std::make_unique<gui::Button>(form);
    button->setId("openButton");
    button->setText("Open XML Scene");
    button->setSizeHint({100, 30});
    button->setHorizontalSizingPolicy(gui::SizingPolicy::FillParent);
    button->clicked += [this]() { openSceneFileFromDialog(); };
    panel->addControl(std::move(button));

    button = std::make_unique<gui::Button>(form);
    button->setId("renderButton");
    button->setText("Start Raytracing");
    button->setPosition({0, 40});
    button->setSizeHint({100, 30});
    button->setHorizontalSizingPolicy(gui::SizingPolicy::FillParent);
    button->clicked += [this]() { startRendering(); };
    panel->addControl(std::move(button));

    button = std::make_unique<gui::Button>(form);
    button->setId("stopButton");
    button->setText("Stop Raytracing");
    button->setPosition({0, 80});
    button->setSizeHint({100, 30});
    button->setHorizontalSizingPolicy(gui::SizingPolicy::FillParent);
    button->clicked += [this]() { stopRendering(); };
    panel->addControl(std::move(button));

    button = std::make_unique<gui::Button>(form);
    button->setId("saveButton");
    button->setText("Save as EXR");
    button->setPosition({0, 120});
    button->setSizeHint({100, 30});
    button->setHorizontalSizingPolicy(gui::SizingPolicy::FillParent);
    button->clicked += [this]() { writeImageToExr(); };
    panel->addControl(std::move(button));
    form->add(std::move(panel));

    // Progress bar

    auto progressBarBg = std::make_unique<gui::Panel>(form);
    progressBarBg->setId("progressBarBg");
    progressBarBg->setPosition({0, 0});
    progressBarBg->setSizeHint({500, 20});
    progressBarBg->setPadding({3, 3});
    progressBarBg->setColor(glm::vec4(0.15f, 0.15f, 0.15f, 1.0f));
    progressBarBg->setAnchor(gui::Anchor::BottomRight);
    progressBarBg->setHorizontalSizingPolicy(gui::SizingPolicy::FillParent, 0.5f);

    auto progressBar = std::make_unique<gui::Panel>(form);
    progressBar->setId("progressBar");
    progressBar->setPosition({0, 0});
    progressBar->setSizeHint({500, 20});
    progressBar->setPadding({3, 3});
    progressBar->setColor(glm::vec4(0.1f, 0.5f, 0.1f, 1.0f));
    progressBar->setAnchor(gui::Anchor::BottomLeft);
    progressBar->setHorizontalSizingPolicy(gui::SizingPolicy::FillParent, 0.0);

    auto label = std::make_unique<gui::Label>(form, "100.0%");
    label->setId("progressLabel");
    label->setAnchor(gui::Anchor::Center);
    label->setOrigin(gui::Origin::Center);
    label->setColor(glm::vec4(1.0f));

    rayTracerProgressed += [label = label.get(), bar = progressBar.get()](float percentage, float timeSpent) {
        float remainingPct = percentage == 0.0f ? 0.0f : (1.0f - percentage) / percentage * timeSpent / 8.0f;

        std::stringstream stringStream;
        stringStream << std::fixed << std::setprecision(2) << std::setfill('0') << percentage * 100
                     << " %    ETA: " << remainingPct << " s";
        label->setText(stringStream.str());
        bar->setHorizontalSizingPolicy(gui::SizingPolicy::FillParent, percentage);
    };

    progressBarBg->addControl(std::move(progressBar));
    progressBarBg->addControl(std::move(label));

    auto* memBar = form->getControlById<gui::MemoryUsageBar>("memoryUsageBar");
    memBar->addControl(std::move(progressBarBg));
}
} // namespace crisp