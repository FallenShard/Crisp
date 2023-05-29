#include <Crisp/Scenes/AtmosphereScene.hpp>

#include <imgui.h>

namespace crisp
{
namespace
{
const auto logger = createLoggerMt("AtmosphereScene");

float azimuth = 0.0f;
float altitude = 0.0f;

const VertexLayoutDescription PbrVertexFormat = {
    {VertexAttribute::Position},
    {VertexAttribute::Normal, VertexAttribute::TexCoord, VertexAttribute::Tangent}
};
} // namespace

AtmosphereScene::AtmosphereScene(Renderer* renderer, Window* window)
    : AbstractScene(renderer, window)
{
    setupInput();

    m_cameraController = std::make_unique<FreeCameraController>(*m_window);
    m_resourceContext->createUniformBuffer("camera", sizeof(CameraParameters), BufferUpdatePolicy::PerFrame);
    m_renderer->getDebugMarker().setObjectName(m_resourceContext->getUniformBuffer("camera")->get(), "cameraBuffer");

    // Object transforms
    m_transformBuffer = std::make_unique<TransformBuffer>(m_renderer, 100);
    m_renderer->getDebugMarker().setObjectName(m_transformBuffer->getUniformBuffer()->get(), "transformBuffer");

    createCommonTextures();

    auto nodes = addAtmosphereRenderPasses(
        *m_renderGraph, *m_renderer, *m_resourceContext, m_resourceContext->renderTargetCache, "SCREEN");
    for (auto& [key, value] : nodes)
        m_renderNodes.emplace(std::move(key), std::move(value));
    m_renderer->setSceneImageView(m_renderGraph->getNode("rayMarchingPass").renderPass.get(), 0);

    m_renderGraph->sortRenderPasses().unwrap();
    m_renderGraph->printExecutionOrder();

    m_renderer->getDevice().flushDescriptorUpdates();
}

void AtmosphereScene::resize(int width, int height)
{
    m_cameraController->onViewportResized(width, height);

    m_resourceContext->renderTargetCache.resizeRenderTargets(m_renderer->getDevice(), m_renderer->getSwapChainExtent());

    m_renderGraph->resize(width, height);
    // m_renderer->setSceneImageView(m_renderGraph->getNode(ForwardLightingPass).renderPass.get(), 0);
}

void AtmosphereScene::update(float dt)
{
    // Camera
    m_cameraController->update(dt);
    const auto camParams = m_cameraController->getCameraParameters();
    m_resourceContext->getUniformBuffer("camera")->updateStagingBuffer(camParams);

    // Object transforms
    m_transformBuffer->update(camParams.V, camParams.P);

    ////// auto svp = m_lightSystem->getDirectionalLight().getProjectionMatrix() *
    ////// m_lightSystem->getDirectionalLight().getViewMatrix();
    ///////*atmosphere.gShadowmapViewProjMat = svp;
    ////// atmosphere.gSkyViewProjMat = P * V;
    ////// atmosphere.gSkyInvViewProjMat = glm::inverse(P * V);
    ////// atmosphere.camera = m_cameraController->getCamera().getPosition();
    ////// atmosphere.view_ray = m_cameraController->getCamera().getLookDirection();
    ////// atmosphere.sun_direction = m_lightSystem->getDirectionalLight().getDirection();*/

    ////

    ////// atmosphere.gSkyViewProjMat = VP;

    ////// atmosphere.gSkyViewProjMat = glm::scale(glm::vec3(1.0f, -1.0f, 1.0f)) * camParams.P * camParams.V;
    ////// atmosphere.camera = m_cameraController->getCamera().getPosition();
    /////*atmosphere.view_ray = m_cameraController->getCamera().getLookDir();*/

    ////// atmosphere.gSkyInvViewProjMat = glm::inverse(atmosphere.gSkyViewProjMat);
    ////// m_resourceContext->getUniformBuffer("atmosphere")->updateStagingBuffer(atmosphere);

    ////// commonConstantData.gSkyViewProjMat = atmosphere.gSkyViewProjMat;
    ////// m_resourceContext->getUniformBuffer("atmosphereCommon")->updateStagingBuffer(commonConstantData);

    //// atmoBuffer.cameraPosition = atmosphere.camera;

    const auto screenExtent = m_renderer->getSwapChainExtent();
    m_atmosphereParams.screenResolution = glm::vec2(screenExtent.width, screenExtent.height);
    m_resourceContext->getUniformBuffer("atmosphereBuffer")->updateStagingBuffer(m_atmosphereParams);
}

void AtmosphereScene::render()
{
    m_renderGraph->clearCommandLists();
    m_renderGraph->buildCommandLists(m_renderNodes);
    m_renderGraph->executeCommandLists();
}

void AtmosphereScene::renderGui()
{
    ImGui::Begin("Settings");
    if (ImGui::SliderFloat("Azimuth", &azimuth, 0.0f, 2.0f * glm::pi<float>()))
    {
        m_atmosphereParams.sunDirection.x = std::cos(azimuth) * std::cos(altitude);
        m_atmosphereParams.sunDirection.y = std::sin(altitude);
        m_atmosphereParams.sunDirection.z = std::sin(azimuth) * std::cos(altitude);
    }
    if (ImGui::SliderFloat("Altitude", &altitude, 0.0f, glm::pi<float>()))
    {
        m_atmosphereParams.sunDirection.x = std::cos(azimuth) * std::cos(altitude);
        m_atmosphereParams.sunDirection.y = std::sin(altitude);
        m_atmosphereParams.sunDirection.z = std::sin(azimuth) * std::cos(altitude);
    }
    ImGui::End();
}

RenderNode* AtmosphereScene::createRenderNode(std::string id, bool hasTransform)
{
    if (!hasTransform)
    {
        return m_renderNodes.emplace(id, std::make_unique<RenderNode>()).first->second.get();
    }
    else
    {
        const auto transformHandle{m_transformBuffer->getNextIndex()};
        return m_renderNodes.emplace(id, std::make_unique<RenderNode>(*m_transformBuffer, transformHandle))
            .first->second.get();
    }
}

void AtmosphereScene::createCommonTextures()
{
    constexpr float kAnisotropy{16.0f};
    constexpr float kMaxLod{9.0f};
    auto& imageCache = m_resourceContext->imageCache;
    imageCache.addSampler("nearestNeighbor", createNearestClampSampler(m_renderer->getDevice()));
    imageCache.addSampler("linearRepeat", createLinearRepeatSampler(m_renderer->getDevice(), kAnisotropy));
    imageCache.addSampler("linearMipmap", createLinearClampSampler(m_renderer->getDevice(), kAnisotropy, kMaxLod));
    imageCache.addSampler("linearClamp", createLinearClampSampler(m_renderer->getDevice(), kAnisotropy));
}

void AtmosphereScene::setupInput()
{
    m_connectionHandlers.emplace_back(m_window->keyPressed.subscribe(
        [this](Key key, int)
        {
            switch (key)
            {
            case Key::F5:
                m_resourceContext->recreatePipelines();
                break;
            }
        }));
}
} // namespace crisp