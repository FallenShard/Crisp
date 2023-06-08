#pragma once

#include <Crisp/Camera/FreeCameraController.hpp>
#include <Crisp/Geometry/TransformBuffer.hpp>
#include <Crisp/Models/Skybox.hpp>
#include <Crisp/Scenes/Scene.hpp>

namespace crisp
{

class AmbientOcclusionScene : public AbstractScene
{
public:
    AmbientOcclusionScene(Renderer* renderer, Window* window);
    ~AmbientOcclusionScene();

    virtual void resize(int width, int height) override;
    virtual void update(float dt) override;
    virtual void render() override;

    void setNumSamples(int numSamples);
    void setRadius(double radius);

private:
    void createGui();

    struct SsaoParams
    {
        int numSamples;
        float radius;
    };

    std::unique_ptr<FreeCameraController> m_cameraController;

    std::unique_ptr<TransformBuffer> m_transformBuffer;

    std::unique_ptr<Skybox> m_skybox;

    SsaoParams m_ssaoParams;

    std::unique_ptr<RenderNode> m_floorNode;
    std::unique_ptr<RenderNode> m_sponzaNode;
};
} // namespace crisp