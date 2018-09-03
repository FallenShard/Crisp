#include "RayTracerScene.hpp"

#include <iostream>

#include <Vesper/RayTracer.hpp>

#include "Core/Application.hpp"
#include "Core/Window.hpp"
#include "IO/FileUtils.hpp"
#include "IO/OpenEXRWriter.hpp"
#include "RayTracerGui.hpp"
#include "RaytracedImage.hpp"
#include "GUI/Form.hpp"
#include "vulkan/VulkanImageView.hpp"

namespace crisp
{
    RayTracerScene::RayTracerScene(Renderer* renderer, Application* app)
        : m_numChannels(4)
        , m_progress(0.0f)
        , m_timeSpentRendering(0.0f)
        , m_renderer(renderer)
        , m_app(app)
    {
        m_image = std::make_unique<RayTracedImage>(512, 512, VK_FORMAT_R32G32B32A32_SFLOAT, m_renderer);
        m_rayTracer = std::make_unique<RayTracer>();
        //m_rayTracer->setImageSize(DefaultWindowWidth, DefaultWindowHeight);
        m_rayTracer->setProgressUpdater([this](RayTracerUpdate&& update)
        {
            m_progress = static_cast<float>(update.pixelsRendered) / static_cast<float>(update.numPixels);
            m_timeSpentRendering = update.totalTimeSpentRendering;
            m_updateQueue.emplace(std::move(update));
            rayTracerProgressed(m_progress, m_timeSpentRendering);
        });
        m_renderer->setSceneImageView(nullptr);

        gui::RayTracerGui gui;
        m_app->getForm()->add(gui.buildSceneOptions(m_app->getForm(), this));
        m_app->getForm()->add(gui.buildProgressBar(m_app->getForm(), this));
    }

    RayTracerScene::~RayTracerScene()
    {
        m_app->getForm()->remove("vesperOptionsPanel");
        m_app->getForm()->remove("progressBarBg");
    }

    void RayTracerScene::resize(int width, int height)
    {
        m_image->resize(width, height);
    }

    void RayTracerScene::update(float)
    {
        while (!m_updateQueue.empty())
        {
            RayTracerUpdate update;
            if (m_updateQueue.try_pop(update))
            {
                m_image->postTextureUpdate(update);

                auto imageSize = m_rayTracer->getImageSize();
                for (int y = 0; y < update.height; y++)
                {
                    for (int x = 0; x < update.width; x++)
                    {
                        size_t rowIdx = update.y + y;
                        size_t colIdx = update.x + x;

                        for (int c = 0; c < 4; c++)
                        {
                            m_imageData[(rowIdx * imageSize.x + colIdx) * 4 + c] = update.data[(y * update.width + x) * 4 + c];
                        }
                    }
                }
            }
        }
    }

    void RayTracerScene::render()
    {
        if (m_image)
            m_image->draw();
    }

    void RayTracerScene::openSceneFileFromDialog()
    {
        auto openedFile = fileutils::openFileDialog();
        if (openedFile == "")
            return;

        openSceneFile(openedFile);
    }

    void RayTracerScene::startRendering()
    {
        m_rayTracer->start();
    }

    void RayTracerScene::stopRendering()
    {
        m_rayTracer->stop();
    }

    void RayTracerScene::writeImageToExr()
    {
        std::string outputDirectory = "Output";
        fileutils::createDirectory(outputDirectory);
        OpenEXRWriter writer;
        glm::ivec2 imageSize = m_rayTracer->getImageSize();
        std::cout << "Writing EXR image..." << std::endl;

        std::string fileName = outputDirectory + "/" + m_projectName + ".exr";

        int i = 0;
        while (fileutils::fileExists(fileName))
            fileName = outputDirectory + "/" + m_projectName + "_" + std::to_string(++i) + ".exr";

        writer.write(fileName, m_imageData.data(), imageSize.x, imageSize.y, true);
    }

    void RayTracerScene::openSceneFile(const std::string& filename)
    {
        m_renderer->finish();
        m_projectName = fileutils::getFileNameFromPath(filename);
        m_rayTracer->initializeScene(filename);

        glm::ivec2 imageSize = m_rayTracer->getImageSize();
        m_image = std::make_unique<RayTracedImage>(imageSize.x, imageSize.y, VK_FORMAT_R32G32B32A32_SFLOAT, m_renderer);
        m_imageData.resize(m_numChannels * imageSize.x * imageSize.y);
        m_app->getWindow()->setTitle(filename);
    }

    void RayTracerScene::createGui()
    {
        //gui::Form* form = m_app->getForm();
    }
}