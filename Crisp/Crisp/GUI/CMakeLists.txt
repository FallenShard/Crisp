add_cpp_static_library(
    CrispImGui
    "ImGuiUtils.cpp"
    "ImGuiUtils.hpp"
)
target_link_libraries(
    CrispImGui
    PUBLIC ImGui
    PUBLIC Crisp::Window
)

add_cpp_static_library(
    CrispImGuiCameraUtils
    "ImGuiCameraUtils.cpp"
    "ImGuiCameraUtils.hpp"
)
target_link_libraries(
    CrispImGuiCameraUtils
    PUBLIC ImGui
    PUBLIC Crisp::Camera
    PRIVATE Crisp::GlmFormat
)