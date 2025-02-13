#include <Crisp/Scenes/FluidSimulationScene.hpp>

#include <Crisp/Camera/FreeCameraController.hpp>
#include <Crisp/Core/Application.hpp>
#include <Crisp/Core/Window.hpp>
#include <Crisp/Gui/Button.hpp>
#include <Crisp/Gui/Form.hpp>
#include <Crisp/Gui/Label.hpp>
#include <Crisp/Gui/Slider.hpp>
#include <Crisp/Models/FluidSimulation.hpp>
#include <Crisp/Models/SPH.hpp>
#include <Crisp/Renderer/RenderGraph.hpp>
#include <Crisp/Renderer/RenderPasses/ForwardLightingPass.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImage.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImageView.hpp>
#include <Crisp/Vulkan/Rhi/VulkanPipeline.hpp>
#include <Crisp/Vulkan/Rhi/VulkanSampler.hpp>

namespace crisp {
namespace {
static constexpr const char* MainPass = "mainPass";
}

FluidSimulationScene::FluidSimulationScene(Renderer* renderer, Window* window)
    : Scene(renderer, window) {
    m_cameraController = std::make_unique<FreeCameraController>(*m_window);
    m_uniformBuffers.emplace(
        "camera", std::make_unique<UniformBuffer>(m_renderer, sizeof(CameraParameters), BufferUpdatePolicy::PerFrame));

    auto& mainPassNode = m_renderGraph->addRenderPass(
        MainPass,
        createForwardLightingPass(m_renderer->getDevice(), m_renderTargetCache, renderer->getSwapChainExtent()));
    m_renderGraph->addDependency(MainPass, "SCREEN", 0);

    m_renderer->setSceneImageView(mainPassNode.renderPass.get(), 0);

    m_transformsBuffer =
        std::make_unique<UniformBuffer>(m_renderer, sizeof(TransformPack), BufferUpdatePolicy::PerFrame);
    m_uniformBuffers.emplace(
        "params", std::make_unique<UniformBuffer>(m_renderer, sizeof(ParticleParams), BufferUpdatePolicy::PerFrame));

    m_pointSpritePipeline = m_renderer->createPipeline("PointSprite.lua", *mainPassNode.renderPass, 0);
    m_pointSpriteMaterial = std::make_unique<Material>(m_pointSpritePipeline.get());
    m_pointSpriteMaterial->writeDescriptor(0, 0, *m_transformsBuffer);
    m_pointSpriteMaterial->writeDescriptor(1, 0, *m_uniformBuffers.at("params"));

    m_fluidSimulation = std::make_unique<SPH>(m_renderer, m_renderGraph.get());
    m_window->keyPressed.subscribe<&FluidSimulation::onKeyPressed>(m_fluidSimulation.get());

    m_fluidGeometry = std::make_unique<Geometry>();
    m_fluidGeometry->addNonOwningVertexBuffer(m_fluidSimulation->getVertexBuffer("position"));
    m_fluidGeometry->addNonOwningVertexBuffer(m_fluidSimulation->getVertexBuffer("color"));
    m_fluidGeometry->setVertexCount(m_fluidSimulation->getParticleCount());
    m_fluidGeometry->setInstanceCount(1);

    m_fluidRenderNode = RenderNode(m_transformsBuffer.get(), &m_transforms, {0, 0});
    m_fluidRenderNode.geometry = m_fluidGeometry.get();
    m_fluidRenderNode.pass(MainPass).material = m_pointSpriteMaterial.get();

    m_renderer->getDevice().flushDescriptorUpdates();

    // createGui();
    m_renderGraph->sortRenderPasses().unwrap();
    m_renderGraph->printExecutionOrder();
}

FluidSimulationScene::~FluidSimulationScene() {
    m_window->keyPressed.unsubscribe<&FluidSimulation::onKeyPressed>(m_fluidSimulation.get());
    // m_app->getForm()->remove("fluidSimulationPanel");
}

void FluidSimulationScene::resize(int width, int height) {
    m_cameraController->onViewportResized(width, height);

    m_renderGraph->resize(width, height);
    m_renderer->setSceneImageView(m_renderGraph->getNode(MainPass).renderPass.get(), 0);
}

void FluidSimulationScene::update(float dt) {
    m_cameraController->update(dt);
    m_uniformBuffers["camera"]->updateStagingBuffer2(m_cameraController->getCameraParameters());

    const float scale = 10.0f;
    m_transforms.M = glm::scale(glm::vec3(scale));
    m_transforms.MV = m_cameraController->getCamera().getViewMatrix() * m_transforms.M;
    m_transforms.MVP = m_cameraController->getCamera().getProjectionMatrix() * m_transforms.MV;
    m_transformsBuffer->updateStagingBuffer2(m_transforms);

    m_fluidSimulation->update(dt);

    m_particleParams.radius = m_fluidSimulation->getParticleRadius() * scale;
    m_particleParams.screenSpaceScale =
        m_renderer->getSwapChainExtent().width * m_cameraController->getCamera().getProjectionMatrix()[0][0];
    m_uniformBuffers["params"]->updateStagingBuffer2(m_particleParams);

    const uint32_t vertexByteOffset =
        m_fluidSimulation->getCurrentSection() * m_fluidSimulation->getParticleCount() * sizeof(glm::vec4);
    m_fluidGeometry->setVertexBufferOffset(0, vertexByteOffset);
    m_fluidGeometry->setVertexBufferOffset(1, vertexByteOffset);
}

void FluidSimulationScene::render() {
    m_renderGraph->clearCommandLists();
    m_renderGraph->addToCommandLists(m_fluidRenderNode);
    m_renderGraph->executeCommandLists();
}

void FluidSimulationScene::createGui() {
    using namespace gui;
    gui::Form* form = {};
    std::unique_ptr<Panel> panel = std::make_unique<Panel>(form);

    panel->setId("fluidSimulationPanel");
    panel->setPadding({20, 20});
    panel->setPosition({20, 40});
    panel->setVerticalSizingPolicy(SizingPolicy::WrapContent);
    panel->setHorizontalSizingPolicy(SizingPolicy::WrapContent);

    int y = 0;
    auto addLabeledSlider = [&](const std::string& labelText, double val, double minVal, double maxVal) {
        auto label = std::make_unique<Label>(form, labelText);
        label->setPosition({0, y});
        panel->addControl(std::move(label));
        y += 20;

        auto slider = std::make_unique<DoubleSlider>(form, minVal, maxVal);
        slider->setId(labelText + "Slider");
        slider->setAnchor(Anchor::TopCenter);
        slider->setOrigin(Origin::TopCenter);
        slider->setPosition({0, y});
        slider->setValue(val);
        slider->setIncrement(0.1);
        slider->setPrecision(1);

        DoubleSlider* sliderPtr = slider.get();
        panel->addControl(std::move(slider));
        y += 30;

        return sliderPtr;
    };

    addLabeledSlider("Gravity X", 0.0, -10.0, +10.0)->valueChanged += [this](double val) {
        m_fluidSimulation->setGravityX(static_cast<float>(val));
    };
    addLabeledSlider("Gravity Y", -9.8, -10.0, +10.0)->valueChanged += [this](double val) {
        m_fluidSimulation->setGravityY(static_cast<float>(val));
    };
    addLabeledSlider("Gravity Z", 0.0, -10.0, +10.0)->valueChanged += [this](double val) {
        m_fluidSimulation->setGravityZ(static_cast<float>(val));
    };

    auto viscoLabel = std::make_unique<Label>(form, "Viscosity");
    viscoLabel->setPosition({0, y});
    panel->addControl(std::move(viscoLabel));
    y += 20;

    auto viscositySlider = std::make_unique<IntSlider>(form);
    viscositySlider->setId("viscositySlider");
    viscositySlider->setAnchor(Anchor::TopCenter);
    viscositySlider->setOrigin(Origin::TopCenter);
    viscositySlider->setPosition({0, y});
    viscositySlider->setMaxValue(30);
    viscositySlider->setMinValue(3);
    viscositySlider->setValue(3);
    viscositySlider->valueChanged += [this](int value) { m_fluidSimulation->setViscosity(static_cast<float>(value)); };
    panel->addControl(std::move(viscositySlider));
    y += 30;

    auto surfaceTensionLabel = std::make_unique<Label>(form, "Surface Tension");
    surfaceTensionLabel->setPosition({0, y});
    panel->addControl(std::move(surfaceTensionLabel));
    y += 20;

    auto surfaceTensionSlider = std::make_unique<IntSlider>(form);
    surfaceTensionSlider->setId("surfaceTensionSlider");
    surfaceTensionSlider->setAnchor(Anchor::TopCenter);
    surfaceTensionSlider->setOrigin(Origin::TopCenter);
    surfaceTensionSlider->setPosition({0, y});
    surfaceTensionSlider->setMaxValue(50);
    surfaceTensionSlider->setMinValue(1);
    surfaceTensionSlider->setValue(1);
    surfaceTensionSlider->valueChanged += [this](int value) {
        m_fluidSimulation->setSurfaceTension(static_cast<float>(value));
    };
    panel->addControl(std::move(surfaceTensionSlider));
    y += 30;

    auto resetButton = std::make_unique<Button>(form);
    resetButton->setId("resetButton");
    resetButton->setPosition({0, y});
    resetButton->setSizeHint({0, 30});
    resetButton->setText("Reset Simulation");
    resetButton->setHorizontalSizingPolicy(SizingPolicy::FillParent);
    resetButton->clicked += [this]() { m_fluidSimulation->reset(); };
    panel->addControl(std::move(resetButton));

    form->add(std::move(panel));
}
} // namespace crisp