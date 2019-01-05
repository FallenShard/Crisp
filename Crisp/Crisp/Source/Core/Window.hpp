#pragma once

#include <memory>
#include <vector>
#include <string>

#include <vulkan/vulkan.h>
#include <CrispCore/Math/Headers.hpp>

#include "EventHub.hpp"

struct GLFWwindow;

namespace crisp
{
    enum class CursorState
    {
        Disabled,
        Normal
    };

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

        void setCursorState(CursorState cursorState);
        void setCursorPosition(const glm::vec2& position);
        void setTitle(const std::string& title);

        glm::ivec2 getSize() const;
        glm::vec2 getCursorPosition() const;
        inline GLFWwindow* getHandle() const { return m_window; }

        EventHub& getEventHub();

       
        VkResult createRenderingSurface(VkInstance instance, const VkAllocationCallbacks* allocCallbacks, VkSurfaceKHR* surface) const;

    private:
        GLFWwindow* m_window;

        std::unique_ptr<EventHub> m_eventHub;
    };
}