#pragma once

#include <Crisp/Scenes/Scene.hpp>

#include <Crisp/Core/Event.hpp>
#include <Crisp/PathTracer/RayTracer.hpp>
#include <Crisp/Scenes/RaytracedImage.hpp>

#include <tbb/concurrent_queue.h>

#include <atomic>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace crisp {
class RayTracerScene : public Scene {
public:
    RayTracerScene(Renderer* renderer, Window* window);

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

    std::string m_projectName;
    std::unique_ptr<RayTracedImage> m_image;
    tbb::concurrent_queue<RayTracerUpdate> m_updateQueue;

    std::atomic<float> m_progress;
    std::atomic<float> m_timeSpentRendering;

    std::unique_ptr<RayTracer> m_rayTracer;
    std::vector<float> m_imageData;
};
} // namespace crisp