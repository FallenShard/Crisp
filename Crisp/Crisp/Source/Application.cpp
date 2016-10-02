#include "Application.hpp"

#include <iostream>
#include <iomanip>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <chrono>

#include "InputDispatcher.hpp"
#include "Timer.hpp"
#include "FrameTimeLogger.hpp"

namespace crisp
{
    namespace
    {
        int DefaultWindowWidth = 960;
        int DefaultWindowHeight = 540;
        std::string ApplicationTitle = "Crisp";

        glm::vec4 clearColor = glm::vec4(0.2f, 0.2f, 0.2f, 1.f);

        auto frameTimeLogger = FrameTimeLogger<Timer<std::milli>>();
    }

    Application::Application()
        : m_window(nullptr, glfwDestroyWindow)
    {
        std::cout << "Initializing our world...\n";

        m_window.reset(glfwCreateWindow(DefaultWindowWidth, DefaultWindowHeight, ApplicationTitle.c_str(), nullptr, nullptr));

        glfwMakeContextCurrent(m_window.get());
        gladLoadGLLoader((GLADloadproc)(glfwGetProcAddress));

        m_inputDispatcher = std::make_unique<InputDispatcher>(m_window.get());

        m_inputDispatcher->windowResized.subscribe<Application, &Application::onResize>(this);
    }

    Application::~Application()
    {
        m_window.reset();
        glfwTerminate();
    }

    void Application::initDependencies()
    {
        auto result = glfwInit();
        if (result == GLFW_FALSE)
            std::cerr << "Could not initialize GLFW library!" << std::endl;
    }

    void Application::run()
    {
        std::cout << "Hello world!\n";

        frameTimeLogger.start();
        while (!glfwWindowShouldClose(m_window.get()))
        {
            glfwPollEvents();

            //glClear(GL_COLOR_BUFFER_BIT);
            glClearBufferfv(GL_COLOR, 0, glm::value_ptr(clearColor));

            glfwSwapBuffers(m_window.get());
            
            frameTimeLogger.restart();
        }
    }

    void Application::onResize(int width, int height)
    {
        std::cout << "New dims: (" << width << ", " << height << ")\n";
    }
}