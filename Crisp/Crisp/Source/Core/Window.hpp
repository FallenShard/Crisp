#pragma once

#include <memory>
#include <vector>
#include <string>

#include "ApplicationEnvironment.hpp"
#include "Math/Headers.hpp"

namespace crisp
{
    class InputDispatcher;

    class Window
    {
    public:
        Window(const glm::ivec2& position, const glm::ivec2& size, std::string title);
        Window(int x, int y, int width, int height, std::string title);
        ~Window();

        static glm::ivec2 getDesktopResolution();
        static void pollEvents();

        bool shouldClose() const;
        void close();

        void setInputMode(int mode, int value);

        glm::ivec2 getSize() const;
        glm::vec2 getCursorPosition() const;
        inline GLFWwindow* getHandle() const { return m_window; }

        InputDispatcher* getInputDispatcher();

        static std::vector<std::string> getVulkanExtensions();
        VkResult createRenderingSurface(VkInstance instance, const VkAllocationCallbacks* allocCallbacks, VkSurfaceKHR* surface) const;

    private:
        GLFWwindow* m_window;

        std::unique_ptr<InputDispatcher> m_inputDispatcher;
    };
}