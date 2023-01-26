add_cpp_static_library(ImGui
    "imconfig.h"
    "imgui.h"
    "imgui.cpp"
    "imgui_demo.cpp"
    "imgui_draw.cpp"
    "imgui_internal.h"
    "imgui_tables.cpp"
    "imgui_widgets.cpp"
    "imgui_impl_glfw.h"
    "imgui_impl_glfw.cpp"
    "imgui_impl_vulkan.h"
    "imgui_impl_vulkan.cpp"
    "imstb_rectpack.h"
    "imstb_textedit.h"
    "imstb_truetype.h")
target_include_directories(ImGui PUBLIC ".")
target_link_libraries(ImGui
    PUBLIC glfw
    PUBLIC vulkan)