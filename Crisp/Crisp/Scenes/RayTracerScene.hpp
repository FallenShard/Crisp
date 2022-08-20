#pragma once

#include <Crisp/Scenes/Scene.hpp>

#include <Crisp/Event.hpp>
#include <Vesper/RayTracerUpdate.hpp>

#include <tbb/concurrent_queue.h>

#include <atomic>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace crisp
{
class Application;
class Renderer;

class RayTracer;
class RayTracedImage;

class RayTracerScene : public AbstractScene
{
public:
    RayTracerScene(Renderer* renderer, Application* app);
    ~RayTracerScene();

    virtual void resize(int width, int height) override;
    virtual void update(float dt) override;
    virtual void render() override;

    Event<float, float> rayTracerProgressed;

    void openSceneFileFromDialog();
    void startRendering();
    void stopRendering();
    void writeImageToExr();

private:
    void openSceneFile(const std::filesystem::path& filename);
    void createGui();

    Renderer* m_renderer;
    Application* m_app;

    std::string m_projectName;
    std::unique_ptr<RayTracedImage> m_image;
    tbb::concurrent_queue<RayTracerUpdate> m_updateQueue;

    std::atomic<float> m_progress;
    std::atomic<float> m_timeSpentRendering;

    std::unique_ptr<RayTracer> m_rayTracer;
    std::vector<float> m_imageData;
};
} // namespace crisp