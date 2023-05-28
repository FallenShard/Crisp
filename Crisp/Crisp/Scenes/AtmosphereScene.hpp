#pragma once

#include <Crisp/Camera/FreeCameraController.hpp>
#include <Crisp/Camera/TargetCameraController.hpp>
#include <Crisp/Core/ConnectionHandler.hpp>
#include <Crisp/Core/HashMap.hpp>
#include <Crisp/Models/Atmosphere.hpp>
#include <Crisp/Scenes/Scene.hpp>

namespace crisp
{
class AtmosphereScene : public AbstractScene
{
public:
    AtmosphereScene(Renderer* renderer, Application* app);

    virtual void resize(int width, int height) override;
    virtual void update(float dt) override;
    virtual void render() override;
    virtual void renderGui() override;

private:
    RenderNode* createRenderNode(std::string nodeId, bool hasTransform);

    void createCommonTextures();

    void setupInput();

    std::vector<ConnectionHandler> m_connectionHandlers;

    std::unique_ptr<FreeCameraController> m_cameraController;

    std::unique_ptr<TransformBuffer> m_transformBuffer;

    FlatHashMap<std::string, std::unique_ptr<RenderNode>> m_renderNodes;

    AtmosphereParameters m_atmosphereParams;
};
} // namespace crisp
