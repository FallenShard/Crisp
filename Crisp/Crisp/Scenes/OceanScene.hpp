#pragma once

#include <Crisp/Camera/FreeCameraController.hpp>
#include <Crisp/Geometry/TransformBuffer.hpp>
#include <Crisp/Models/Skybox.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Scenes/Scene.hpp>
#include <Crisp/Vulkan/VulkanImage.hpp>

namespace crisp
{
class OceanScene : public AbstractScene
{
public:
    OceanScene(Renderer* renderer, Window* window);
    ~OceanScene();

    virtual void resize(int width, int height) override;
    virtual void update(float dt) override;
    virtual void render() override;
    virtual void renderGui() override;

private:
    std::unique_ptr<VulkanImage> createInitialSpectrum();
    int applyFFT(std::string image);

    std::unique_ptr<FreeCameraController> m_cameraController;
    std::unique_ptr<TransformBuffer> m_transformBuffer;

    std::vector<std::unique_ptr<RenderNode>> m_renderNodes;

    std::unique_ptr<Skybox> m_skybox;

    float m_time = 0.0;
};
} // namespace crisp
