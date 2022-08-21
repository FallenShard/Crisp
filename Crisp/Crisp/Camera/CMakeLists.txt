add_cpp_static_library(CrispCamera
    "Camera.cpp"
    "Camera.hpp"
    "FreeCameraController.cpp"
    "FreeCameraController.hpp"
    "TargetCameraController.cpp"
    "TargetCameraController.hpp")
target_link_libraries(CrispCamera
    PUBLIC CrispMath
    PUBLIC CrispRobinHood
    PUBLIC CrispEvent
    PUBLIC CrispWindow)