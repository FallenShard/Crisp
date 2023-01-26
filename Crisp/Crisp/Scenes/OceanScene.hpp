#pragma once

#include "Scene.hpp"
#include <Crisp/Renderer/Renderer.hpp>

#include <Crisp/Math/Headers.hpp>

namespace crisp
{
class FreeCameraController;

class TransformBuffer;

struct RenderNode;
class Skybox;

namespace gui
{
class Form;
}

class VulkanImage;

class OceanScene : public AbstractScene
{
public:
    OceanScene(Renderer* renderer, Application* app);
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
