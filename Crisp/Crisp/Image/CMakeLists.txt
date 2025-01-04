add_subdirectory(Io)

add_cpp_static_library(
    CrispImage
    "Image.cpp"
    "Image.hpp"
)

add_cpp_test(
    CrispImageTest
    "Test/ImageTest.cpp"
)
target_link_libraries(
    CrispImageTest
    PRIVATE Crisp::Image
)