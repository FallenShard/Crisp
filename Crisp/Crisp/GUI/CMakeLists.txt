add_cpp_static_library(CrispGui
    "DrawComponents/RectDrawComponent.hpp"
    "DrawComponents/TextDrawComponent.hpp"
    "Button.cpp"
    "Button.hpp"
    "CheckBox.cpp"
    "CheckBox.hpp"
    "ComboBox.cpp"
    "ComboBox.hpp"
    "ComboBoxItem.cpp"
    "ComboBoxItem.hpp"
    "Control.cpp"
    "Control.hpp"
    "ControlGroup.cpp"
    "ControlGroup.hpp"
    "DebugRect.cpp"
    "DebugRect.hpp"
    "DynamicUniformBufferResource.cpp"
    "DynamicUniformBufferResource.hpp"
    "Form.cpp"
    "Form.hpp"
    "GuiEnums.hpp"
    "Label.cpp"
    "Label.hpp"
    "ListBox.cpp"
    "ListBox.hpp"
    "MemoryUsageBar.cpp"
    "MemoryUsageBar.hpp"
    "Panel.cpp"
    "Panel.hpp"
    "RenderSystem.cpp"
    "RenderSystem.hpp"
    "Slider.hpp"
    "SliderModel.hpp"
    "StatusBar.cpp"
    "StatusBar.hpp"
    "StopWatch.hpp")
target_link_libraries(CrispGui
    PUBLIC Crisp::Event
    PUBLIC Crisp::Animation
    PUBLIC Crisp::Math
    PUBLIC Crisp::Renderer
    PUBLIC Crisp::Input
    PUBLIC Crisp::BitFlags
    PUBLIC Crisp::FontIo
    PUBLIC Crisp::Geometry
    PUBLIC glfw
    PUBLIC Crisp::VulkanHeader
    PRIVATE Crisp::ImageIo)

add_cpp_static_library(CrispImGui
    "ImGuiUtils.cpp"
    "ImGuiUtils.hpp"
)
target_link_libraries(CrispImGui
    PUBLIC ImGui
    PUBLIC Crisp::Renderer
    PUBLIC Crisp::Window
)

add_cpp_static_library(CrispImGuiCameraUtils
    "ImGuiCameraUtils.cpp"
    "ImGuiCameraUtils.hpp"
)
target_link_libraries(CrispImGuiCameraUtils
    PUBLIC ImGui
    PUBLIC Crisp::Camera
    PRIVATE Crisp::GlmFormat
)