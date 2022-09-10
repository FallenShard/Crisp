add_cpp_static_library(CrispAnimation
    "Animation.cpp"
    "Animation.hpp"
    "Animator.cpp"
    "Animator.hpp"
    "PropertyAnimation.hpp")
target_link_libraries(CrispAnimation
    PUBLIC CrispMath
    PUBLIC CrispHashMap
    PUBLIC CrispEvent)