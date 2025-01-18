add_cpp_static_library(
    CrispCamera
    "Camera.cpp"
    "Camera.hpp"
    "FreeCameraController.cpp"
    "FreeCameraController.hpp"
    "TargetCameraController.cpp"
    "TargetCameraController.hpp"
)
target_link_libraries(
    CrispCamera
    PUBLIC Crisp::Math
    PUBLIC Crisp::HashMap
    PUBLIC Crisp::Event
    PUBLIC Crisp::Window
    PUBLIC Microsoft.GSL::GSL
)

add_cpp_test(
    CrispCameraTest
    "Test/CameraTest.cpp"
    "Test/FreeCameraControllerTest.cpp"
)
target_link_libraries(
    CrispCameraTest
    PRIVATE Crisp::Camera
)