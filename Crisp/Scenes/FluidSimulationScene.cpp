#include "FluidSimulationScene.hpp"

#include "Models/FluidSimulation.hpp"
#include "Models/PositionBasedFluid.hpp"
#include "Models/SPH.hpp"

#include "Core/Application.hpp"
#include "Core/Window.hpp"
#include "Camera/CameraController.hpp"

#include "Renderer/RenderPasses/SceneRenderPass.hpp"
#include "Renderer/Pipelines/PointSphereSpritePipeline.hpp"
#include "Renderer/Pipelines/FullScreenQuadPipeline.hpp"
#include "vulkan/VulkanImageView.hpp"
#include "vulkan/VulkanImage.hpp"
#include "Renderer/Renderer.hpp"
#include "Renderer/UniformBuffer.hpp"
#include "vulkan/VulkanSampler.hpp"

#include "GUI/Form.hpp"
#include "GUI/Label.hpp"
#include "GUI/Button.hpp"
#include "GUI/Slider.hpp"

#include "Renderer/RenderGraph.hpp"

namespace crisp
{
    namespace
    {
        static constexpr const char* MainPass = "mainPass";
    }

    FluidSimulationScene::FluidSimulationScene(Renderer* renderer, Application* app)
        : m_renderer(renderer)
        , m_app(app)
    {
        m_cameraController = std::make_unique<CameraController>(app->getWindow());
        m_uniformBuffers.emplace("camera", std::make_unique<UniformBuffer>(m_renderer, sizeof(CameraParameters), BufferUpdatePolicy::PerFrame));

        m_renderGraph = std::make_unique<RenderGraph>(m_renderer);
        auto& mainPassNode = m_renderGraph->addRenderPass(MainPass, std::make_unique<SceneRenderPass>(m_renderer));
        m_renderGraph->addRenderTargetLayoutTransition(MainPass, "SCREEN", 0);
        m_renderGraph->sortRenderPasses();
        m_renderer->setSceneImageView(mainPassNode.renderPass.get(), 0);

        m_fluidSimulation = std::make_unique<SPH>(m_renderer);
        m_app->getWindow()->keyPressed.subscribe<&FluidSimulation::onKeyPressed>(m_fluidSimulation.get());

        createGui();

        m_transformsBuffer = std::make_unique<UniformBuffer>(m_renderer, sizeof(TransformPack), BufferUpdatePolicy::PerFrame);
        m_uniformBuffers.emplace("params", std::make_unique<UniformBuffer>(m_renderer, sizeof(ParticleParams), BufferUpdatePolicy::PerFrame));

        m_pointSpritePipeline = createPointSphereSpritePipeline(m_renderer, mainPassNode.renderPass.get());
        m_pointSpriteDescGroup =
        {
            m_pointSpritePipeline->allocateDescriptorSet(0),
            m_pointSpritePipeline->allocateDescriptorSet(1)
        };
        m_pointSpriteDescGroup.postBufferUpdate(0, 0, m_transformsBuffer->getDescriptorInfo());
        m_pointSpriteDescGroup.postBufferUpdate(1, 0, m_uniformBuffers.at("params")->getDescriptorInfo());
        m_pointSpriteDescGroup.flushUpdates(m_renderer->getDevice());
    }

    FluidSimulationScene::~FluidSimulationScene()
    {
        m_app->getWindow()->keyPressed.unsubscribe<&FluidSimulation::onKeyPressed>(m_fluidSimulation.get());
        m_app->getForm()->remove("fluidSimulationPanel");
    }

    void FluidSimulationScene::resize(int width, int height)
    {
        m_cameraController->resize(width, height);

        m_renderGraph->resize(width, height);
        m_renderer->setSceneImageView(m_renderGraph->getNode(MainPass).renderPass.get(), 0);
    }

    void FluidSimulationScene::update(float dt)
    {
        m_cameraController->update(dt);
        m_uniformBuffers["camera"]->updateStagingBuffer(m_cameraController->getCameraParameters(), sizeof(CameraParameters));

        m_transforms.M = glm::scale(glm::vec3(10.0f));
        m_transforms.MV = m_cameraController->getViewMatrix() * m_transforms.M;
        m_transforms.MVP = m_cameraController->getProjectionMatrix() * m_transforms.MV;
        m_transformsBuffer->updateStagingBuffer(m_transforms);

        m_particleParams.radius = m_fluidSimulation->getParticleRadius() * 10.0f;
        m_particleParams.screenSpaceScale = m_renderer->getSwapChainExtent().width * m_cameraController->getProjectionMatrix()[0][0];
        m_uniformBuffers["params"]->updateStagingBuffer(m_particleParams);

        m_fluidSimulation->update(dt);
    }

    void FluidSimulationScene::render()
    {
        //m_renderGraph->clearCommandLists();
        //m_renderGraph->buildCommandLists();
        //m_renderGraph->executeCommandLists();

        m_renderer->enqueueDrawCommand([this](VkCommandBuffer commandBuffer)
        {
            auto frameIdx = m_renderer->getCurrentVirtualFrameIndex();

            auto& passNode = m_renderGraph->getNode(MainPass);

            m_fluidSimulation->dispatchCompute(commandBuffer, frameIdx);
            passNode.renderPass->begin(commandBuffer);
            m_pointSpriteDescGroup.setDynamicOffset(0, m_transformsBuffer->getDynamicOffset(frameIdx));
            m_pointSpriteDescGroup.setDynamicOffset(1, m_uniformBuffers["params"]->getDynamicOffset(frameIdx));
            m_pointSpritePipeline->bind(commandBuffer);
            m_renderer->setDefaultViewport(commandBuffer);
            m_renderer->setDefaultScissor(commandBuffer);
            m_pointSpriteDescGroup.bind(commandBuffer, m_pointSpritePipeline->getPipelineLayout()->getHandle());
            m_fluidSimulation->drawGeometry(commandBuffer);
            passNode.renderPass->end(commandBuffer, frameIdx);

            passNode.renderPass->getRenderTarget(0)->transitionLayout(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, frameIdx, 1,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        });
    }

    void FluidSimulationScene::createGui()
    {
        using namespace gui;
        std::unique_ptr<Panel> panel = std::make_unique<Panel>(m_app->getForm());

        panel->setId("fluidSimulationPanel");
        panel->setPadding({ 20, 20 });
        panel->setPosition({ 20, 40 });
        panel->setVerticalSizingPolicy(SizingPolicy::WrapContent);
        panel->setHorizontalSizingPolicy(SizingPolicy::WrapContent);

        int y = 0;
        auto addLabeledSlider = [&](const std::string& labelText, double val, double minVal, double maxVal) {
            auto label = std::make_unique<Label>(m_app->getForm(), labelText);
            label->setPosition({ 0, y });
            panel->addControl(std::move(label));
            y += 20;

            auto slider = std::make_unique<DoubleSlider>(m_app->getForm(), minVal, maxVal);
            slider->setId(labelText + "Slider");
            slider->setAnchor(Anchor::TopCenter);
            slider->setOrigin(Origin::TopCenter);
            slider->setPosition({ 0, y });
            slider->setValue(val);
            slider->setIncrement(0.1);
            slider->setPrecision(1);

            DoubleSlider* sliderPtr = slider.get();
            panel->addControl(std::move(slider));
            y += 30;

            return sliderPtr;
        };

        addLabeledSlider("Gravity X", 0.0,  -10.0, +10.0)->valueChanged += [this](double val) { m_fluidSimulation->setGravityX(val); };
        addLabeledSlider("Gravity Y", -9.8, -10.0, +10.0)->valueChanged += [this](double val) { m_fluidSimulation->setGravityX(val); };
        addLabeledSlider("Gravity Z", 0.0,  -10.0, +10.0)->valueChanged += [this](double val) { m_fluidSimulation->setGravityX(val); };

        auto viscoLabel = std::make_unique<Label>(m_app->getForm(), "Viscosity");
        viscoLabel->setPosition({ 0, y });
        panel->addControl(std::move(viscoLabel));
        y += 20;

        auto viscositySlider = std::make_unique<IntSlider>(m_app->getForm());
        viscositySlider->setId("viscositySlider");
        viscositySlider->setAnchor(Anchor::TopCenter);
        viscositySlider->setOrigin(Origin::TopCenter);
        viscositySlider->setPosition({ 0, y });
        viscositySlider->setMaxValue(30);
        viscositySlider->setMinValue(3);
        viscositySlider->setValue(3);
        viscositySlider->valueChanged += [this](int value)
        {
            m_fluidSimulation->setViscosity(static_cast<float>(value));
        };
        panel->addControl(std::move(viscositySlider));
        y += 30;

        auto surfaceTensionLabel = std::make_unique<Label>(m_app->getForm(), "Surface Tension");
        surfaceTensionLabel->setPosition({ 0, y });
        panel->addControl(std::move(surfaceTensionLabel));
        y += 20;

        auto surfaceTensionSlider = std::make_unique<IntSlider>(m_app->getForm());
        surfaceTensionSlider->setId("surfaceTensionSlider");
        surfaceTensionSlider->setAnchor(Anchor::TopCenter);
        surfaceTensionSlider->setOrigin(Origin::TopCenter);
        surfaceTensionSlider->setPosition({ 0, y });
        surfaceTensionSlider->setMaxValue(50);
        surfaceTensionSlider->setMinValue(1);
        surfaceTensionSlider->setValue(1);
        surfaceTensionSlider->valueChanged += [this](int value)
        {
            m_fluidSimulation->setSurfaceTension(static_cast<float>(value));
        };
        panel->addControl(std::move(surfaceTensionSlider));
        y += 30;

        auto resetButton = std::make_unique<Button>(m_app->getForm());
        resetButton->setId("resetButton");
        resetButton->setPosition({ 0, y });
        resetButton->setSizeHint({ 0, 30 });
        resetButton->setText("Reset Simulation");
        resetButton->setHorizontalSizingPolicy(SizingPolicy::FillParent);
        resetButton->clicked += [this]()
        {
            m_fluidSimulation->reset();
        };
        panel->addControl(std::move(resetButton));

        m_app->getForm()->add(std::move(panel));
    }
}