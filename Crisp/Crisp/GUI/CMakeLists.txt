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
    "CrispGui.cpp"
    "CrispGui.hpp"
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
    PUBLIC CrispEvent
    PUBLIC CrispAnimation
    PUBLIC CrispMath
    PUBLIC CrispRenderer
    PUBLIC CrispInput
    PUBLIC CrispBitFlags
    PUBLIC CrispFontIo
    PUBLIC CrispGeometry
    PUBLIC glfw
    PUBLIC vulkan
    PRIVATE CrispImageIo)

add_cpp_static_library(CrispImGui
    "ImGuiUtils.cpp"
    "ImGuiUtils.hpp"
)
target_link_libraries(CrispImGui
    PUBLIC ImGui
    PUBLIC CrispRenderer
    PUBLIC CrispWindow
)